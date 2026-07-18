#ifndef MPH_BLUETOOTH_H
#define MPH_BLUETOOTH_H

#include "mph_device.h"
#include "mph_device_list.h"
#include "mph_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_BLUETOOTH_TEXT_CAPACITY MPH_DEVICE_TEXT_CAPACITY
#define MPH_BLUETOOTH_ADDRESS_CAPACITY MPH_DEVICE_TEXT_CAPACITY

typedef struct {
    char name[MPH_BLUETOOTH_TEXT_CAPACITY];
    char address[MPH_BLUETOOTH_ADDRESS_CAPACITY];
    uint32_t class_of_device;
    uint32_t major_device_class;
    uint32_t minor_device_class;
    uint32_t major_service_class;
    bool is_paired;
    bool is_connected;
} mph_bluetooth_raw_device_t;

void mph_bluetooth_raw_device_init(mph_bluetooth_raw_device_t *raw_device);
mph_status_t mph_bluetooth_normalize_address(const char *address, char *buffer, size_t capacity);
bool mph_bluetooth_is_audio_device(const mph_bluetooth_raw_device_t *raw_device);
bool mph_bluetooth_is_hid_device(const mph_bluetooth_raw_device_t *raw_device);
mph_device_category_t mph_bluetooth_infer_category(const mph_bluetooth_raw_device_t *raw_device);
mph_status_t mph_bluetooth_device_id(mph_device_id_t *out_device_id,
                                     const mph_bluetooth_raw_device_t *raw_device);
bool mph_bluetooth_matches_device(const mph_bluetooth_raw_device_t *raw_device,
                                  const mph_device_t *device);
mph_status_t mph_bluetooth_map_raw_device(const mph_bluetooth_raw_device_t *raw_device,
                                          mph_device_t *out_device);
mph_status_t mph_bluetooth_append_mapped_device(mph_device_list_t *out_devices,
                                                const mph_bluetooth_raw_device_t *raw_device);
mph_status_t mph_bluetooth_enumerate_devices(mph_device_list_t *out_devices);

#ifdef __cplusplus
}
#endif

#endif
