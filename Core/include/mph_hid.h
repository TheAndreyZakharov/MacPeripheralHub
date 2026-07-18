#ifndef MPH_HID_H
#define MPH_HID_H

#include "mph_device.h"
#include "mph_device_list.h"
#include "mph_result.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_HID_TEXT_CAPACITY MPH_DEVICE_TEXT_CAPACITY

typedef struct {
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t usage_page;
    uint32_t usage;
    uint64_t registry_id;
    char product_name[MPH_HID_TEXT_CAPACITY];
    char manufacturer[MPH_HID_TEXT_CAPACITY];
    char serial_number[MPH_DEVICE_SERIAL_CAPACITY];
    char transport_name[MPH_HID_TEXT_CAPACITY];
    bool is_connected;
} mph_hid_raw_device_t;

void mph_hid_raw_device_init(mph_hid_raw_device_t *raw_device);
mph_device_category_t mph_hid_infer_category(const mph_hid_raw_device_t *raw_device);
mph_device_transport_t mph_hid_infer_transport(const mph_hid_raw_device_t *raw_device);
mph_status_t mph_hid_device_id(mph_device_id_t *out_device_id,
                               const mph_hid_raw_device_t *raw_device);
mph_status_t mph_hid_map_raw_device(const mph_hid_raw_device_t *raw_device,
                                    mph_device_t *out_device);
mph_status_t mph_hid_enumerate_devices(mph_device_list_t *out_devices);

#ifdef __cplusplus
}
#endif

#endif
