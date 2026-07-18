#include "mph_selection.h"

#include <stdio.h>

void mph_selection_init(mph_selection_t *selection) {
    if (selection == NULL) {
        return;
    }

    selection->mode = MPH_ACTIVE_MODE_NONE;
    selection->profile_id[0] = '\0';
    selection->enforce_audio_defaults = true;

    for (int role = 0; role < MPH_DEVICE_ROLE_COUNT; role += 1) {
        mph_device_id_clear(&selection->role_device_ids[role]);
    }
}

mph_status_t mph_selection_set_profile_id(mph_selection_t *selection, const char *profile_id) {
    if (selection == NULL || profile_id == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    int written = snprintf(selection->profile_id, sizeof(selection->profile_id), "%s", profile_id);
    if (written < 0 || written >= (int)sizeof(selection->profile_id)) {
        selection->profile_id[0] = '\0';
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return MPH_STATUS_OK;
}

mph_status_t mph_selection_set_role_device(mph_selection_t *selection, mph_device_role_t role,
                                           const mph_device_id_t *device_id) {
    if (selection == NULL || role < 0 || role >= MPH_DEVICE_ROLE_COUNT || device_id == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    selection->role_device_ids[role] = *device_id;
    return MPH_STATUS_OK;
}

const mph_device_id_t *mph_selection_get_role_device(const mph_selection_t *selection,
                                                     mph_device_role_t role) {
    if (selection == NULL || role < 0 || role >= MPH_DEVICE_ROLE_COUNT) {
        return NULL;
    }

    return &selection->role_device_ids[role];
}

const char *mph_device_role_name(mph_device_role_t role) {
    switch (role) {
    case MPH_DEVICE_ROLE_DEFAULT_INPUT:
        return "default_input";
    case MPH_DEVICE_ROLE_DEFAULT_OUTPUT:
        return "default_output";
    case MPH_DEVICE_ROLE_SYSTEM_OUTPUT:
        return "system_output";
    case MPH_DEVICE_ROLE_PREFERRED_CAMERA:
        return "preferred_camera";
    case MPH_DEVICE_ROLE_COUNT:
        return "role_count";
    }

    return "unknown_role";
}
