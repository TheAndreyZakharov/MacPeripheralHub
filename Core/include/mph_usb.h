#ifndef MPH_USB_H
#define MPH_USB_H

#include "mph_device.h"
#include "mph_device_list.h"
#include "mph_result.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_USB_TEXT_CAPACITY MPH_DEVICE_TEXT_CAPACITY

typedef struct {
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t device_class;
    uint32_t device_subclass;
    uint32_t device_protocol;
    uint32_t interface_class_mask;
    uint32_t speed_mbps;
    uint32_t power_ma;
    uint32_t location_id;
    uint64_t registry_id;
    uint64_t parent_registry_id;
    uint32_t depth;
    char product_name[MPH_USB_TEXT_CAPACITY];
    char vendor_name[MPH_USB_TEXT_CAPACITY];
    char serial_number[MPH_DEVICE_SERIAL_CAPACITY];
    char location_path[MPH_USB_TEXT_CAPACITY];
    bool is_connected;
} mph_usb_raw_device_t;

void mph_usb_raw_device_init(mph_usb_raw_device_t *raw_device);
mph_device_category_t mph_usb_infer_category(const mph_usb_raw_device_t *raw_device);
mph_status_t mph_usb_device_id(mph_device_id_t *out_device_id,
                               const mph_usb_raw_device_t *raw_device);
bool mph_usb_matches_device(const mph_usb_raw_device_t *raw_device, const mph_device_t *device);
mph_status_t mph_usb_map_raw_device(const mph_usb_raw_device_t *raw_device,
                                    mph_device_t *out_device);
mph_status_t mph_usb_append_mapped_device(mph_device_list_t *out_devices,
                                          const mph_usb_raw_device_t *raw_device);
mph_status_t mph_usb_enumerate_devices(mph_device_list_t *out_devices);

#ifdef __cplusplus
}
#endif

#endif
