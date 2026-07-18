#include "mph_swift_bridge.h"

#include "mph_device_id.h"

static const char *empty_if_null(const char *value) {
    return value != NULL ? value : "";
}

const char *mph_swift_device_id_cstr(const mph_device_t *device) {
    return device != NULL ? mph_device_id_cstr(&device->id) : "";
}

const char *mph_swift_device_display_name_cstr(const mph_device_t *device) {
    return empty_if_null(device != NULL ? device->display_name : NULL);
}

const char *mph_swift_device_vendor_name_cstr(const mph_device_t *device) {
    return empty_if_null(device != NULL ? device->vendor_name : NULL);
}

const char *mph_swift_device_model_name_cstr(const mph_device_t *device) {
    return empty_if_null(device != NULL ? device->model_name : NULL);
}

const char *mph_swift_device_serial_number_cstr(const mph_device_t *device) {
    return empty_if_null(device != NULL ? device->serial_number : NULL);
}

const char *mph_swift_device_camera_unique_id_cstr(const mph_device_t *device) {
    return empty_if_null(device != NULL ? device->camera.unique_id : NULL);
}

const char *mph_swift_device_bluetooth_address_cstr(const mph_device_t *device) {
    return empty_if_null(device != NULL ? device->bluetooth.address : NULL);
}

const char *mph_swift_profile_id_cstr(const mph_profile_t *profile) {
    return empty_if_null(profile != NULL ? profile->id : NULL);
}

const char *mph_swift_profile_name_cstr(const mph_profile_t *profile) {
    return empty_if_null(profile != NULL ? profile->name : NULL);
}

const char *mph_swift_selection_profile_id_cstr(const mph_selection_t *selection) {
    return empty_if_null(selection != NULL ? selection->profile_id : NULL);
}

const char *mph_swift_selection_role_device_id_cstr(const mph_selection_t *selection,
                                                    mph_device_role_t role) {
    const mph_device_id_t *device_id = mph_selection_get_role_device(selection, role);
    return mph_device_id_cstr(device_id);
}
