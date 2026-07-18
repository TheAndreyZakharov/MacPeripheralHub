#ifndef MPH_CAMERA_H
#define MPH_CAMERA_H

#include "mph_device.h"
#include "mph_device_list.h"
#include "mph_result.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_CAMERA_TEXT_CAPACITY MPH_DEVICE_TEXT_CAPACITY

typedef struct {
    char unique_id[MPH_CAMERA_TEXT_CAPACITY];
    char localized_name[MPH_CAMERA_TEXT_CAPACITY];
    char manufacturer[MPH_CAMERA_TEXT_CAPACITY];
    char model_id[MPH_CAMERA_TEXT_CAPACITY];
    char device_type[MPH_CAMERA_TEXT_CAPACITY];
    mph_device_transport_t transport;
    bool is_connected;
} mph_camera_raw_device_t;

void mph_camera_raw_device_init(mph_camera_raw_device_t *raw_device);
bool mph_camera_global_default_supported(void);
mph_status_t mph_camera_device_id(mph_device_id_t *out_device_id,
                                  const mph_camera_raw_device_t *raw_device);
mph_device_transport_t mph_camera_infer_transport(const mph_camera_raw_device_t *raw_device);
mph_status_t mph_camera_map_raw_device(const mph_camera_raw_device_t *raw_device,
                                       mph_device_t *out_device);
mph_status_t mph_camera_enumerate_devices(mph_device_list_t *out_devices);

#ifdef __cplusplus
}
#endif

#endif
