#include "mph_reconcile.h"

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

        mph_status_t status =
            mph_reconcile_plan_add(plan, audio_roles[index].action_type, audio_roles[index].role,
                                   desired_id);
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
