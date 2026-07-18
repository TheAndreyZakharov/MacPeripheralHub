#ifndef MPH_RECONCILE_H
#define MPH_RECONCILE_H

#include "mph_device_list.h"
#include "mph_selection.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_RECONCILE_MAX_ACTIONS 8

typedef enum {
    MPH_RECONCILE_ACTION_NOOP = 0,
    MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT,
    MPH_RECONCILE_ACTION_SET_DEFAULT_OUTPUT,
    MPH_RECONCILE_ACTION_SET_SYSTEM_OUTPUT,
    MPH_RECONCILE_ACTION_MARK_MISSING
} mph_reconcile_action_type_t;

typedef struct {
    mph_reconcile_action_type_t type;
    mph_device_role_t role;
    mph_device_id_t target_device_id;
} mph_reconcile_action_t;

typedef struct {
    mph_reconcile_action_t actions[MPH_RECONCILE_MAX_ACTIONS];
    size_t action_count;
} mph_reconcile_plan_t;

typedef struct {
    uint64_t debounce_interval_ms;
    uint64_t retry_initial_delay_ms;
    uint64_t retry_max_delay_ms;
    uint32_t max_retry_count;
} mph_reconcile_policy_t;

typedef struct {
    uint64_t last_action_unix_ms;
    uint64_t next_retry_unix_ms;
    uint32_t retry_count;
    mph_device_id_t last_target_device_id;
} mph_reconcile_role_state_t;

typedef struct {
    mph_reconcile_role_state_t roles[MPH_DEVICE_ROLE_COUNT];
} mph_reconcile_state_t;

typedef struct {
    const mph_selection_t *desired;
    const mph_selection_t *current;
    const mph_device_list_t *available_devices;
    uint64_t now_unix_ms;
} mph_reconcile_context_t;

void mph_reconcile_plan_init(mph_reconcile_plan_t *plan);
mph_status_t mph_reconcile_plan_add(mph_reconcile_plan_t *plan, mph_reconcile_action_type_t type,
                                    mph_device_role_t role,
                                    const mph_device_id_t *target_device_id);
void mph_reconcile_policy_default(mph_reconcile_policy_t *policy);
void mph_reconcile_state_init(mph_reconcile_state_t *state);
void mph_reconcile_context_init(mph_reconcile_context_t *context);
bool mph_reconcile_role_due(const mph_reconcile_role_state_t *role_state, uint64_t now_unix_ms);
mph_status_t mph_reconcile_evaluate_audio_defaults(const mph_reconcile_context_t *context,
                                                   mph_reconcile_state_t *state,
                                                   const mph_reconcile_policy_t *policy,
                                                   mph_reconcile_plan_t *plan);
mph_status_t mph_reconcile_apply_audio_plan(const mph_reconcile_plan_t *plan);
mph_status_t mph_reconcile_audio_defaults(const mph_selection_t *desired,
                                          const mph_selection_t *current,
                                          mph_reconcile_plan_t *plan);
const char *mph_reconcile_action_name(mph_reconcile_action_type_t type);

#ifdef __cplusplus
}
#endif

#endif
