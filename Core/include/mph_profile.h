#ifndef MPH_PROFILE_H
#define MPH_PROFILE_H

#include "mph_selection.h"
#include "mph_time.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_PROFILE_ID_CAPACITY 64
#define MPH_PROFILE_NAME_CAPACITY 96

typedef struct {
    char id[MPH_PROFILE_ID_CAPACITY];
    char name[MPH_PROFILE_NAME_CAPACITY];
    mph_selection_t selection;
    uint64_t created_at_unix_ms;
    uint64_t updated_at_unix_ms;
} mph_profile_t;

void mph_profile_init(mph_profile_t *profile);
mph_status_t mph_profile_configure(mph_profile_t *profile, const char *id, const char *name);
mph_status_t mph_profile_set_name(mph_profile_t *profile, const char *name);
mph_status_t mph_profile_set_role_device(mph_profile_t *profile, mph_device_role_t role,
                                         const mph_device_id_t *device_id);
bool mph_profile_is_valid(const mph_profile_t *profile);

#ifdef __cplusplus
}
#endif

#endif
