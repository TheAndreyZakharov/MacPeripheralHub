#ifndef MPH_RECONCILE_H
#define MPH_RECONCILE_H

#include "mph_selection.h"

#include <stddef.h>

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

void mph_reconcile_plan_init(mph_reconcile_plan_t *plan);
mph_status_t mph_reconcile_plan_add(mph_reconcile_plan_t *plan, mph_reconcile_action_type_t type,
                                    mph_device_role_t role,
                                    const mph_device_id_t *target_device_id);
mph_status_t mph_reconcile_audio_defaults(const mph_selection_t *desired,
                                          const mph_selection_t *current,
                                          mph_reconcile_plan_t *plan);
const char *mph_reconcile_action_name(mph_reconcile_action_type_t type);

#ifdef __cplusplus
}
#endif

#endif
