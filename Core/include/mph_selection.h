#ifndef MPH_SELECTION_H
#define MPH_SELECTION_H

#include "mph_device_id.h"
#include "mph_result.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MPH_DEVICE_ROLE_DEFAULT_INPUT = 0,
    MPH_DEVICE_ROLE_DEFAULT_OUTPUT,
    MPH_DEVICE_ROLE_SYSTEM_OUTPUT,
    MPH_DEVICE_ROLE_PREFERRED_CAMERA,
    MPH_DEVICE_ROLE_COUNT
} mph_device_role_t;

typedef enum {
    MPH_ACTIVE_MODE_NONE = 0,
    MPH_ACTIVE_MODE_PROFILE,
    MPH_ACTIVE_MODE_MANUAL
} mph_active_mode_t;

typedef struct {
    mph_active_mode_t mode;
    char profile_id[MPH_DEVICE_ID_CAPACITY];
    mph_device_id_t role_device_ids[MPH_DEVICE_ROLE_COUNT];
    bool enforce_audio_defaults;
} mph_selection_t;

void mph_selection_init(mph_selection_t *selection);
mph_status_t mph_selection_set_profile_id(mph_selection_t *selection, const char *profile_id);
mph_status_t mph_selection_set_role_device(mph_selection_t *selection, mph_device_role_t role,
                                           const mph_device_id_t *device_id);
const mph_device_id_t *mph_selection_get_role_device(const mph_selection_t *selection,
                                                     mph_device_role_t role);
const char *mph_device_role_name(mph_device_role_t role);

#ifdef __cplusplus
}
#endif

#endif
