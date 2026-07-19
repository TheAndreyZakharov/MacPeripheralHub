#include "mph_camera.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MPH_CAMERA_ID_HASH_CAPACITY 32

static bool text_is_empty(const char *value) {
    return value == NULL || value[0] == '\0';
}

static uint64_t fnv1a_hash(const char *value) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (value == NULL) {
        return hash;
    }

    for (size_t index = 0; value[index] != '\0'; index += 1) {
        hash ^= (unsigned char)value[index];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static bool text_contains_folded(const char *haystack, const char *needle) {
    if (text_is_empty(haystack) || text_is_empty(needle)) {
        return false;
    }

    size_t needle_length = strlen(needle);
    for (size_t haystack_index = 0; haystack[haystack_index] != '\0'; haystack_index += 1) {
        size_t needle_index = 0;
        while (needle_index < needle_length && haystack[haystack_index + needle_index] != '\0' &&
               tolower((unsigned char)haystack[haystack_index + needle_index]) ==
                   tolower((unsigned char)needle[needle_index])) {
            needle_index += 1;
        }
        if (needle_index == needle_length) {
            return true;
        }
    }

    return false;
}

static bool camera_text_contains(const mph_camera_raw_device_t *raw_device, const char *needle) {
    return raw_device != NULL && (text_contains_folded(raw_device->localized_name, needle) ||
                                  text_contains_folded(raw_device->manufacturer, needle) ||
                                  text_contains_folded(raw_device->model_id, needle) ||
                                  text_contains_folded(raw_device->device_type, needle));
}

void mph_camera_raw_device_init(mph_camera_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return;
    }

    raw_device->unique_id[0] = '\0';
    raw_device->localized_name[0] = '\0';
    raw_device->manufacturer[0] = '\0';
    raw_device->model_id[0] = '\0';
    raw_device->device_type[0] = '\0';
    raw_device->transport = MPH_DEVICE_TRANSPORT_UNKNOWN;
    raw_device->is_connected = false;
}

bool mph_camera_global_default_supported(void) {
    return false;
}

const char *mph_camera_permission_status_name(mph_camera_permission_status_t status) {
    switch (status) {
    case MPH_CAMERA_PERMISSION_UNKNOWN:
        return "unknown";
    case MPH_CAMERA_PERMISSION_NOT_DETERMINED:
        return "not_determined";
    case MPH_CAMERA_PERMISSION_RESTRICTED:
        return "restricted";
    case MPH_CAMERA_PERMISSION_DENIED:
        return "denied";
    case MPH_CAMERA_PERMISSION_AUTHORIZED:
        return "authorized";
    }

    return "unknown";
}

mph_status_t mph_camera_device_id(mph_device_id_t *out_device_id,
                                  const mph_camera_raw_device_t *raw_device) {
    if (out_device_id == NULL || raw_device == NULL || raw_device->unique_id[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = mph_device_id_from_parts(out_device_id, "camera", raw_device->unique_id);
    if (status != MPH_STATUS_CAPACITY_EXCEEDED) {
        return status;
    }

    char hashed_uid[MPH_CAMERA_ID_HASH_CAPACITY];
    snprintf(hashed_uid, sizeof(hashed_uid), "uid-%016" PRIx64, fnv1a_hash(raw_device->unique_id));
    return mph_device_id_from_parts(out_device_id, "camera", hashed_uid);
}

mph_device_transport_t mph_camera_infer_transport(const mph_camera_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return MPH_DEVICE_TRANSPORT_UNKNOWN;
    }

    if (raw_device->transport != MPH_DEVICE_TRANSPORT_UNKNOWN) {
        return raw_device->transport;
    }

    if (camera_text_contains(raw_device, "built-in") ||
        camera_text_contains(raw_device, "builtin") ||
        camera_text_contains(raw_device, "facetime")) {
        return MPH_DEVICE_TRANSPORT_BUILT_IN;
    }

    if (camera_text_contains(raw_device, "usb") || camera_text_contains(raw_device, "uvc") ||
        camera_text_contains(raw_device, "external")) {
        return MPH_DEVICE_TRANSPORT_USB;
    }

    if (camera_text_contains(raw_device, "continuity")) {
        return MPH_DEVICE_TRANSPORT_BLUETOOTH;
    }

    return MPH_DEVICE_TRANSPORT_UNKNOWN;
}

mph_status_t mph_camera_map_raw_device(const mph_camera_raw_device_t *raw_device,
                                       mph_device_t *out_device) {
    if (raw_device == NULL || out_device == NULL || raw_device->unique_id[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_t device;
    mph_device_init(&device);

    mph_status_t status = mph_camera_device_id(&device.id, raw_device);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    device.category = MPH_DEVICE_CATEGORY_CAMERA;
    device.transport = mph_camera_infer_transport(raw_device);
    mph_device_set_connection_state(&device, raw_device->is_connected
                                                 ? MPH_DEVICE_CONNECTION_CONNECTED
                                                 : MPH_DEVICE_CONNECTION_DISCONNECTED);

    if (!text_is_empty(raw_device->localized_name)) {
        status = mph_device_set_display_name(&device, raw_device->localized_name);
    } else {
        status = mph_device_set_display_name(&device, "Camera");
    }
    if (!mph_status_is_ok(status)) {
        return status;
    }

    if (!text_is_empty(raw_device->manufacturer)) {
        status = mph_device_set_vendor_name(&device, raw_device->manufacturer);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    if (!text_is_empty(raw_device->model_id)) {
        status = mph_device_set_model_name(&device, raw_device->model_id);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    } else if (!text_is_empty(raw_device->device_type)) {
        status = mph_device_set_model_name(&device, raw_device->device_type);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    status = mph_device_set_camera_unique_id(&device, raw_device->unique_id);
    if (!mph_status_is_ok(status)) {
        return status;
    }
    device.camera.supports_global_default = mph_camera_global_default_supported();

    *out_device = device;
    return MPH_STATUS_OK;
}
