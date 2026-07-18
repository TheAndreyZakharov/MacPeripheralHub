#ifndef MPH_CORE_AUDIO_H
#define MPH_CORE_AUDIO_H

#include "mph_device.h"
#include "mph_device_list.h"
#include "mph_result.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_CORE_AUDIO_TEXT_CAPACITY MPH_DEVICE_TEXT_CAPACITY

typedef enum {
    MPH_CORE_AUDIO_ROLE_INPUT = 0,
    MPH_CORE_AUDIO_ROLE_OUTPUT,
    MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT
} mph_core_audio_role_t;

typedef struct {
    char uid[MPH_CORE_AUDIO_TEXT_CAPACITY];
    char name[MPH_CORE_AUDIO_TEXT_CAPACITY];
    char manufacturer[MPH_CORE_AUDIO_TEXT_CAPACITY];
    mph_device_transport_t transport;
    double sample_rate_hz;
    uint32_t input_channel_count;
    uint32_t output_channel_count;
    bool is_alive;
    bool is_default_input;
    bool is_default_output;
    bool is_default_system_output;
} mph_core_audio_raw_device_t;

void mph_core_audio_raw_device_init(mph_core_audio_raw_device_t *raw_device);
mph_status_t mph_core_audio_role_device_id(mph_device_id_t *out_device_id,
                                           mph_core_audio_role_t role, const char *core_audio_uid);
mph_status_t mph_core_audio_map_raw_device(const mph_core_audio_raw_device_t *raw_device,
                                           mph_device_list_t *out_devices);
mph_status_t mph_core_audio_enumerate_devices(mph_device_list_t *out_devices);
mph_status_t mph_core_audio_get_default_input(mph_device_id_t *out_device_id, bool *out_found);
mph_status_t mph_core_audio_get_default_output(mph_device_id_t *out_device_id, bool *out_found);
mph_status_t mph_core_audio_get_default_system_output(mph_device_id_t *out_device_id,
                                                      bool *out_found);
mph_status_t mph_core_audio_set_default_input(const mph_device_id_t *device_id);
mph_status_t mph_core_audio_set_default_output(const mph_device_id_t *device_id);
mph_status_t mph_core_audio_set_default_system_output(const mph_device_id_t *device_id);

#ifdef __cplusplus
}
#endif

#endif
