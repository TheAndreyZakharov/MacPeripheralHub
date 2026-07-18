#ifndef MPH_DISPLAY_H
#define MPH_DISPLAY_H

#include "mph_device.h"
#include "mph_device_list.h"
#include "mph_result.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_DISPLAY_TEXT_CAPACITY MPH_DEVICE_TEXT_CAPACITY

typedef struct {
    uint32_t display_id;
    char name[MPH_DISPLAY_TEXT_CAPACITY];
    char vendor_name[MPH_DISPLAY_TEXT_CAPACITY];
    char model_name[MPH_DISPLAY_TEXT_CAPACITY];
    char serial_number[MPH_DEVICE_SERIAL_CAPACITY];
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t serial_numeric;
    uint32_t width_px;
    uint32_t height_px;
    double refresh_rate_hz;
    bool is_main;
    bool is_builtin;
    bool is_online;
    mph_device_transport_t transport;
} mph_display_raw_device_t;

void mph_display_raw_device_init(mph_display_raw_device_t *raw_device);
mph_status_t mph_display_device_id(mph_device_id_t *out_device_id,
                                   const mph_display_raw_device_t *raw_device);
mph_device_transport_t mph_display_infer_transport(const mph_display_raw_device_t *raw_device);
mph_status_t mph_display_map_raw_device(const mph_display_raw_device_t *raw_device,
                                        mph_device_t *out_device);
mph_status_t mph_display_enumerate_devices(mph_device_list_t *out_devices);

#ifdef __cplusplus
}
#endif

#endif
