#include "mph_reconcile.h"

#include "mph_core_audio.h"

static const mph_reconcile_policy_t fallback_policy = {
    500,
    1000,
    30000,
    8,
};

static bool role_is_audio_default(mph_device_role_t role) {
    return role == MPH_DEVICE_ROLE_DEFAULT_INPUT || role == MPH_DEVICE_ROLE_DEFAULT_OUTPUT ||
           role == MPH_DEVICE_ROLE_SYSTEM_OUTPUT;
}

static mph_reconcile_action_type_t action_type_for_role(mph_device_role_t role) {
    switch (role) {
    case MPH_DEVICE_ROLE_DEFAULT_INPUT:
        return MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT;
    case MPH_DEVICE_ROLE_DEFAULT_OUTPUT:
        return MPH_RECONCILE_ACTION_SET_DEFAULT_OUTPUT;
    case MPH_DEVICE_ROLE_SYSTEM_OUTPUT:
        return MPH_RECONCILE_ACTION_SET_SYSTEM_OUTPUT;
    case MPH_DEVICE_ROLE_PREFERRED_CAMERA:
    case MPH_DEVICE_ROLE_COUNT:
        return MPH_RECONCILE_ACTION_NOOP;
    }

    return MPH_RECONCILE_ACTION_NOOP;
}

static bool device_is_available_for_role(const mph_device_list_t *available_devices,
                                         mph_device_role_t role, const mph_device_id_t *device_id) {
    if (available_devices == NULL) {
        return true;
    }

    const mph_device_t *device = mph_device_list_find_by_id(available_devices, device_id);
    if (device == NULL || !device->is_connected) {
        return false;
    }

    switch (role) {
    case MPH_DEVICE_ROLE_DEFAULT_INPUT:
        return device->category == MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    case MPH_DEVICE_ROLE_DEFAULT_OUTPUT:
        return device->category == MPH_DEVICE_CATEGORY_AUDIO_OUTPUT ||
               device->category == MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT;
    case MPH_DEVICE_ROLE_SYSTEM_OUTPUT:
        return device->category == MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT ||
               device->category == MPH_DEVICE_CATEGORY_AUDIO_OUTPUT;
    case MPH_DEVICE_ROLE_PREFERRED_CAMERA:
        return device->category == MPH_DEVICE_CATEGORY_CAMERA;
    case MPH_DEVICE_ROLE_COUNT:
        return false;
    }

    return false;
}

static bool target_changed(const mph_reconcile_role_state_t *role_state,
                           const mph_device_id_t *target_device_id) {
    return role_state == NULL ||
           !mph_device_id_equal(&role_state->last_target_device_id, target_device_id);
}

static uint64_t retry_delay_ms(const mph_reconcile_policy_t *policy, uint32_t retry_count) {
    uint64_t delay = policy->retry_initial_delay_ms;
    for (uint32_t index = 0; index < retry_count && delay < policy->retry_max_delay_ms;
         index += 1) {
        delay *= 2;
        if (delay > policy->retry_max_delay_ms) {
            delay = policy->retry_max_delay_ms;
        }
    }

    return delay;
}

static bool debounce_elapsed(const mph_reconcile_role_state_t *role_state,
                             const mph_reconcile_policy_t *policy, uint64_t now_unix_ms) {
    if (role_state == NULL || role_state->last_action_unix_ms == 0 ||
        policy->debounce_interval_ms == 0) {
        return true;
    }

    return now_unix_ms >= role_state->last_action_unix_ms &&
           now_unix_ms - role_state->last_action_unix_ms >= policy->debounce_interval_ms;
}

static void remember_planned_action(mph_reconcile_role_state_t *role_state,
                                    const mph_device_id_t *target_device_id, uint64_t now_unix_ms) {
    if (role_state == NULL || target_device_id == NULL) {
        return;
    }

    role_state->last_action_unix_ms = now_unix_ms;
    role_state->next_retry_unix_ms = 0;
    role_state->retry_count = 0;
    role_state->last_target_device_id = *target_device_id;
}

static void remember_missing_device(mph_reconcile_role_state_t *role_state,
                                    const mph_reconcile_policy_t *policy,
                                    const mph_device_id_t *target_device_id, uint64_t now_unix_ms) {
    if (role_state == NULL || policy == NULL || target_device_id == NULL) {
        return;
    }

    if (target_changed(role_state, target_device_id)) {
        role_state->retry_count = 0;
        role_state->last_target_device_id = *target_device_id;
    }

    uint64_t delay = retry_delay_ms(policy, role_state->retry_count);
    role_state->last_action_unix_ms = now_unix_ms;
    role_state->next_retry_unix_ms = now_unix_ms + delay;
    if (role_state->retry_count < policy->max_retry_count) {
        role_state->retry_count += 1;
    }
}

void mph_reconcile_plan_init(mph_reconcile_plan_t *plan) {
    if (plan == NULL) {
        return;
    }

    plan->action_count = 0;
}

mph_status_t mph_reconcile_plan_add(mph_reconcile_plan_t *plan, mph_reconcile_action_type_t type,
                                    mph_device_role_t role,
                                    const mph_device_id_t *target_device_id) {
    if (plan == NULL || target_device_id == NULL || role < 0 || role >= MPH_DEVICE_ROLE_COUNT) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    if (plan->action_count >= MPH_RECONCILE_MAX_ACTIONS) {
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    mph_reconcile_action_t *action = &plan->actions[plan->action_count];
    action->type = type;
    action->role = role;
    action->target_device_id = *target_device_id;
    plan->action_count += 1;
    return MPH_STATUS_OK;
}

void mph_reconcile_policy_default(mph_reconcile_policy_t *policy) {
    if (policy == NULL) {
        return;
    }

    *policy = fallback_policy;
}

void mph_reconcile_state_init(mph_reconcile_state_t *state) {
    if (state == NULL) {
        return;
    }

    for (int role = 0; role < MPH_DEVICE_ROLE_COUNT; role += 1) {
        state->roles[role].last_action_unix_ms = 0;
        state->roles[role].next_retry_unix_ms = 0;
        state->roles[role].retry_count = 0;
        mph_device_id_clear(&state->roles[role].last_target_device_id);
    }
}

void mph_reconcile_context_init(mph_reconcile_context_t *context) {
    if (context == NULL) {
        return;
    }

    context->desired = NULL;
    context->current = NULL;
    context->available_devices = NULL;
    context->now_unix_ms = 0;
}

bool mph_reconcile_role_due(const mph_reconcile_role_state_t *role_state, uint64_t now_unix_ms) {
    return role_state == NULL || role_state->next_retry_unix_ms == 0 ||
           now_unix_ms >= role_state->next_retry_unix_ms;
}

mph_status_t mph_reconcile_evaluate_audio_defaults(const mph_reconcile_context_t *context,
                                                   mph_reconcile_state_t *state,
                                                   const mph_reconcile_policy_t *policy,
                                                   mph_reconcile_plan_t *plan) {
    if (context == NULL || context->desired == NULL || context->current == NULL || plan == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    const mph_reconcile_policy_t *effective_policy = policy != NULL ? policy : &fallback_policy;
    mph_reconcile_plan_init(plan);
    if (!context->desired->enforce_audio_defaults) {
        return MPH_STATUS_OK;
    }

    const mph_device_role_t roles[] = {
        MPH_DEVICE_ROLE_DEFAULT_INPUT,
        MPH_DEVICE_ROLE_DEFAULT_OUTPUT,
        MPH_DEVICE_ROLE_SYSTEM_OUTPUT,
    };

    for (size_t index = 0; index < sizeof(roles) / sizeof(roles[0]); index += 1) {
        mph_device_role_t role = roles[index];
        const mph_device_id_t *desired_id = mph_selection_get_role_device(context->desired, role);
        const mph_device_id_t *current_id = mph_selection_get_role_device(context->current, role);
        if (mph_device_id_is_empty(desired_id) || mph_device_id_equal(desired_id, current_id)) {
            if (state != NULL && role_is_audio_default(role)) {
                mph_device_id_clear(&state->roles[role].last_target_device_id);
                state->roles[role].retry_count = 0;
                state->roles[role].next_retry_unix_ms = 0;
            }
            continue;
        }

        mph_reconcile_role_state_t *role_state = state != NULL ? &state->roles[role] : NULL;
        if (role_state != NULL && !target_changed(role_state, desired_id) &&
            !mph_reconcile_role_due(role_state, context->now_unix_ms)) {
            continue;
        }

        if (!device_is_available_for_role(context->available_devices, role, desired_id)) {
            if (role_state != NULL &&
                role_state->retry_count >= effective_policy->max_retry_count &&
                !target_changed(role_state, desired_id)) {
                continue;
            }

            mph_status_t status =
                mph_reconcile_plan_add(plan, MPH_RECONCILE_ACTION_MARK_MISSING, role, desired_id);
            if (!mph_status_is_ok(status)) {
                return status;
            }
            remember_missing_device(role_state, effective_policy, desired_id, context->now_unix_ms);
            continue;
        }

        if (role_state != NULL && !target_changed(role_state, desired_id) &&
            !debounce_elapsed(role_state, effective_policy, context->now_unix_ms)) {
            continue;
        }

        mph_reconcile_action_type_t action_type = action_type_for_role(role);
        if (action_type == MPH_RECONCILE_ACTION_NOOP) {
            continue;
        }

        mph_status_t status = mph_reconcile_plan_add(plan, action_type, role, desired_id);
        if (!mph_status_is_ok(status)) {
            return status;
        }
        remember_planned_action(role_state, desired_id, context->now_unix_ms);
    }

    return MPH_STATUS_OK;
}

mph_status_t mph_reconcile_apply_audio_plan(const mph_reconcile_plan_t *plan) {
    if (plan == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < plan->action_count; index += 1) {
        const mph_reconcile_action_t *action = &plan->actions[index];
        mph_status_t status = MPH_STATUS_OK;
        switch (action->type) {
        case MPH_RECONCILE_ACTION_NOOP:
        case MPH_RECONCILE_ACTION_MARK_MISSING:
            status = MPH_STATUS_OK;
            break;
        case MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT:
            status = mph_core_audio_set_default_input(&action->target_device_id);
            break;
        case MPH_RECONCILE_ACTION_SET_DEFAULT_OUTPUT:
            status = mph_core_audio_set_default_output(&action->target_device_id);
            break;
        case MPH_RECONCILE_ACTION_SET_SYSTEM_OUTPUT:
            status = mph_core_audio_set_default_system_output(&action->target_device_id);
            break;
        }

        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    return MPH_STATUS_OK;
}

mph_status_t mph_reconcile_audio_defaults(const mph_selection_t *desired,
                                          const mph_selection_t *current,
                                          mph_reconcile_plan_t *plan) {
    if (desired == NULL || current == NULL || plan == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_reconcile_plan_init(plan);
    if (!desired->enforce_audio_defaults) {
        return MPH_STATUS_OK;
    }

    const struct {
        mph_device_role_t role;
        mph_reconcile_action_type_t action_type;
    } audio_roles[] = {
        {MPH_DEVICE_ROLE_DEFAULT_INPUT, MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT},
        {MPH_DEVICE_ROLE_DEFAULT_OUTPUT, MPH_RECONCILE_ACTION_SET_DEFAULT_OUTPUT},
        {MPH_DEVICE_ROLE_SYSTEM_OUTPUT, MPH_RECONCILE_ACTION_SET_SYSTEM_OUTPUT},
    };

    for (size_t index = 0; index < sizeof(audio_roles) / sizeof(audio_roles[0]); index += 1) {
        const mph_device_id_t *desired_id =
            mph_selection_get_role_device(desired, audio_roles[index].role);
        const mph_device_id_t *current_id =
            mph_selection_get_role_device(current, audio_roles[index].role);

        if (mph_device_id_is_empty(desired_id) || mph_device_id_equal(desired_id, current_id)) {
            continue;
        }

        mph_status_t status = mph_reconcile_plan_add(plan, audio_roles[index].action_type,
                                                     audio_roles[index].role, desired_id);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    return MPH_STATUS_OK;
}

const char *mph_reconcile_action_name(mph_reconcile_action_type_t type) {
    switch (type) {
    case MPH_RECONCILE_ACTION_NOOP:
        return "noop";
    case MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT:
        return "set_default_input";
    case MPH_RECONCILE_ACTION_SET_DEFAULT_OUTPUT:
        return "set_default_output";
    case MPH_RECONCILE_ACTION_SET_SYSTEM_OUTPUT:
        return "set_system_output";
    case MPH_RECONCILE_ACTION_MARK_MISSING:
        return "mark_missing";
    }

    return "unknown_action";
}
