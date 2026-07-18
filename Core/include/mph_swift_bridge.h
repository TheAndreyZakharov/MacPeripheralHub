#ifndef MPH_SWIFT_BRIDGE_H
#define MPH_SWIFT_BRIDGE_H

#include "mph_device.h"
#include "mph_profile.h"
#include "mph_selection.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *mph_swift_device_id_cstr(const mph_device_t *device);
const char *mph_swift_device_display_name_cstr(const mph_device_t *device);
const char *mph_swift_device_vendor_name_cstr(const mph_device_t *device);
const char *mph_swift_device_model_name_cstr(const mph_device_t *device);
const char *mph_swift_device_serial_number_cstr(const mph_device_t *device);
const char *mph_swift_device_camera_unique_id_cstr(const mph_device_t *device);
const char *mph_swift_device_bluetooth_address_cstr(const mph_device_t *device);

const char *mph_swift_profile_id_cstr(const mph_profile_t *profile);
const char *mph_swift_profile_name_cstr(const mph_profile_t *profile);

const char *mph_swift_selection_profile_id_cstr(const mph_selection_t *selection);
const char *mph_swift_selection_role_device_id_cstr(const mph_selection_t *selection,
                                                    mph_device_role_t role);

#ifdef __cplusplus
}
#endif

#endif
