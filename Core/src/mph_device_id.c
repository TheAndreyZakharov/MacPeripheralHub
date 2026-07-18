#include "mph_device_id.h"

#include <stdio.h>
#include <string.h>

void mph_device_id_clear(mph_device_id_t *device_id) {
    if (device_id == NULL) {
        return;
    }

    device_id->value[0] = '\0';
}

mph_status_t mph_device_id_set(mph_device_id_t *device_id, const char *value) {
    if (device_id == NULL || value == NULL || value[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    int written = snprintf(device_id->value, MPH_DEVICE_ID_CAPACITY, "%s", value);
    if (written < 0 || written >= MPH_DEVICE_ID_CAPACITY) {
        mph_device_id_clear(device_id);
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return MPH_STATUS_OK;
}

mph_status_t mph_device_id_from_parts(mph_device_id_t *device_id, const char *namespace_name,
                                      const char *stable_value) {
    if (device_id == NULL || namespace_name == NULL || stable_value == NULL ||
        namespace_name[0] == '\0' || stable_value[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    int written =
        snprintf(device_id->value, MPH_DEVICE_ID_CAPACITY, "%s:%s", namespace_name, stable_value);
    if (written < 0 || written >= MPH_DEVICE_ID_CAPACITY) {
        mph_device_id_clear(device_id);
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return MPH_STATUS_OK;
}

bool mph_device_id_is_empty(const mph_device_id_t *device_id) {
    return device_id == NULL || device_id->value[0] == '\0';
}

bool mph_device_id_equal(const mph_device_id_t *left, const mph_device_id_t *right) {
    if (left == NULL || right == NULL) {
        return false;
    }

    return strcmp(left->value, right->value) == 0;
}

const char *mph_device_id_cstr(const mph_device_id_t *device_id) {
    return device_id != NULL ? device_id->value : "";
}
