#include "mph_profile.h"

#include <stdio.h>
#include <string.h>

static mph_status_t copy_profile_text(char *destination, size_t capacity, const char *value) {
    if (destination == NULL || capacity == 0 || value == NULL || value[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    int written = snprintf(destination, capacity, "%s", value);
    if (written < 0 || (size_t)written >= capacity) {
        destination[0] = '\0';
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return MPH_STATUS_OK;
}

void mph_profile_init(mph_profile_t *profile) {
    if (profile == NULL) {
        return;
    }

    profile->id[0] = '\0';
    profile->name[0] = '\0';
    mph_selection_init(&profile->selection);
    profile->selection.mode = MPH_ACTIVE_MODE_PROFILE;
    profile->created_at_unix_ms = 0;
    profile->updated_at_unix_ms = 0;
}

mph_status_t mph_profile_configure(mph_profile_t *profile, const char *id, const char *name) {
    if (profile == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = copy_profile_text(profile->id, MPH_PROFILE_ID_CAPACITY, id);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    status = mph_profile_set_name(profile, name);
    if (!mph_status_is_ok(status)) {
        profile->id[0] = '\0';
        return status;
    }

    status = mph_selection_set_profile_id(&profile->selection, id);
    if (!mph_status_is_ok(status)) {
        profile->id[0] = '\0';
        profile->name[0] = '\0';
        return status;
    }

    uint64_t now = mph_time_now_unix_ms();
    profile->created_at_unix_ms = now;
    profile->updated_at_unix_ms = now;
    return MPH_STATUS_OK;
}

mph_status_t mph_profile_set_name(mph_profile_t *profile, const char *name) {
    if (profile == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = copy_profile_text(profile->name, MPH_PROFILE_NAME_CAPACITY, name);
    if (mph_status_is_ok(status)) {
        profile->updated_at_unix_ms = mph_time_now_unix_ms();
    }

    return status;
}

mph_status_t mph_profile_set_role_device(mph_profile_t *profile, mph_device_role_t role,
                                         const mph_device_id_t *device_id) {
    if (profile == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = mph_selection_set_role_device(&profile->selection, role, device_id);
    if (mph_status_is_ok(status)) {
        profile->updated_at_unix_ms = mph_time_now_unix_ms();
    }

    return status;
}

bool mph_profile_is_valid(const mph_profile_t *profile) {
    return profile != NULL && profile->id[0] != '\0' && profile->name[0] != '\0' &&
           strcmp(profile->selection.profile_id, profile->id) == 0;
}
