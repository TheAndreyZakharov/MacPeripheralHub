#ifndef MPH_DEVICE_H
#define MPH_DEVICE_H

#include "mph_device_id.h"
#include "mph_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_DEVICE_TEXT_CAPACITY 128
#define MPH_DEVICE_SERIAL_CAPACITY 96
#define MPH_DEVICE_FINGERPRINT_CAPACITY 256

typedef enum {
    MPH_DEVICE_CONNECTION_UNKNOWN = 0,
    MPH_DEVICE_CONNECTION_CONNECTED,
    MPH_DEVICE_CONNECTION_DISCONNECTED,
    MPH_DEVICE_CONNECTION_UNAVAILABLE
} mph_device_connection_state_t;

typedef enum {
    MPH_DEVICE_CATEGORY_DISPLAY = 0,
    MPH_DEVICE_CATEGORY_AUDIO_INPUT,
    MPH_DEVICE_CATEGORY_AUDIO_OUTPUT,
    MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT,
    MPH_DEVICE_CATEGORY_CAMERA,
    MPH_DEVICE_CATEGORY_KEYBOARD,
    MPH_DEVICE_CATEGORY_MOUSE,
    MPH_DEVICE_CATEGORY_TRACKPAD,
    MPH_DEVICE_CATEGORY_BLUETOOTH,
    MPH_DEVICE_CATEGORY_USB,
    MPH_DEVICE_CATEGORY_HUB,
    MPH_DEVICE_CATEGORY_DOCK,
    MPH_DEVICE_CATEGORY_AUDIO_INTERFACE,
    MPH_DEVICE_CATEGORY_UNKNOWN
} mph_device_category_t;

typedef enum {
    MPH_DEVICE_TRANSPORT_BUILT_IN = 0,
    MPH_DEVICE_TRANSPORT_USB,
    MPH_DEVICE_TRANSPORT_BLUETOOTH,
    MPH_DEVICE_TRANSPORT_THUNDERBOLT,
    MPH_DEVICE_TRANSPORT_HDMI,
    MPH_DEVICE_TRANSPORT_DISPLAY_PORT,
    MPH_DEVICE_TRANSPORT_VIRTUAL,
    MPH_DEVICE_TRANSPORT_AGGREGATE,
    MPH_DEVICE_TRANSPORT_UNKNOWN
} mph_device_transport_t;

typedef struct {
    double sample_rate_hz;
    uint32_t channel_count;
    bool is_default_input;
    bool is_default_output;
    bool is_default_system_output;
} mph_audio_characteristics_t;

typedef struct {
    uint32_t width_px;
    uint32_t height_px;
    double refresh_rate_hz;
    bool is_main;
} mph_display_characteristics_t;

typedef struct {
    char unique_id[MPH_DEVICE_TEXT_CAPACITY];
} mph_camera_characteristics_t;

typedef struct {
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t usage_page;
    uint32_t usage;
} mph_hid_characteristics_t;

typedef struct {
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t speed_mbps;
    uint32_t power_ma;
} mph_usb_characteristics_t;

typedef struct {
    char address[MPH_DEVICE_TEXT_CAPACITY];
    bool is_paired;
    bool is_connected;
    uint32_t class_of_device;
} mph_bluetooth_characteristics_t;

typedef struct {
    mph_device_id_t id;
    mph_device_category_t category;
    mph_device_transport_t transport;
    mph_device_connection_state_t connection_state;
    bool is_connected;
    char display_name[MPH_DEVICE_TEXT_CAPACITY];
    char vendor_name[MPH_DEVICE_TEXT_CAPACITY];
    char model_name[MPH_DEVICE_TEXT_CAPACITY];
    char serial_number[MPH_DEVICE_SERIAL_CAPACITY];
    mph_audio_characteristics_t audio;
    mph_display_characteristics_t display;
    mph_camera_characteristics_t camera;
    mph_hid_characteristics_t hid;
    mph_usb_characteristics_t usb;
    mph_bluetooth_characteristics_t bluetooth;
} mph_device_t;

void mph_device_init(mph_device_t *device);
mph_status_t mph_device_set_display_name(mph_device_t *device, const char *display_name);
mph_status_t mph_device_set_vendor_name(mph_device_t *device, const char *vendor_name);
mph_status_t mph_device_set_model_name(mph_device_t *device, const char *model_name);
mph_status_t mph_device_set_serial_number(mph_device_t *device, const char *serial_number);
mph_status_t mph_device_set_camera_unique_id(mph_device_t *device, const char *unique_id);
mph_status_t mph_device_set_bluetooth_address(mph_device_t *device, const char *address);
void mph_device_set_connection_state(mph_device_t *device,
                                     mph_device_connection_state_t connection_state);
const char *mph_device_category_name(mph_device_category_t category);
const char *mph_device_transport_name(mph_device_transport_t transport);
const char *mph_device_connection_state_name(mph_device_connection_state_t connection_state);
mph_device_category_t mph_device_category_from_name(const char *name);
mph_device_transport_t mph_device_transport_from_name(const char *name);
bool mph_device_category_is_audio(mph_device_category_t category);
mph_status_t mph_device_normalize_name(const char *input, char *output, size_t capacity);
mph_device_category_t mph_device_infer_category(const mph_device_t *device);
void mph_device_apply_inferred_category(mph_device_t *device);
mph_status_t mph_device_fingerprint(const mph_device_t *device, char *buffer, size_t capacity);
int mph_device_match_score(const mph_device_t *known_device, const mph_device_t *candidate_device);
bool mph_device_is_probable_match(const mph_device_t *known_device,
                                  const mph_device_t *candidate_device);

#ifdef __cplusplus
}
#endif

#endif
