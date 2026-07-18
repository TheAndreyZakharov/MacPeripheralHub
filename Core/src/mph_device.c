#include "mph_device.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MPH_DEVICE_MATCH_EXACT_ID 100
#define MPH_DEVICE_MATCH_SERIAL 90
#define MPH_DEVICE_MATCH_UNIQUE_ID 85
#define MPH_DEVICE_MATCH_VENDOR_PRODUCT 75
#define MPH_DEVICE_MATCH_VENDOR_MODEL_NAME 65
#define MPH_DEVICE_MATCH_NAME_ONLY 45
#define MPH_DEVICE_MATCH_THRESHOLD 60

static mph_status_t copy_text(char *destination, size_t capacity, const char *value) {
    if (destination == NULL || capacity == 0 || value == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    int written = snprintf(destination, capacity, "%s", value);
    if (written < 0 || (size_t)written >= capacity) {
        destination[0] = '\0';
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return MPH_STATUS_OK;
}

static bool text_is_empty(const char *value) {
    return value == NULL || value[0] == '\0';
}

static bool text_equal(const char *left, const char *right) {
    return !text_is_empty(left) && !text_is_empty(right) && strcmp(left, right) == 0;
}

static bool normalized_text_equal(const char *left, const char *right) {
    char normalized_left[MPH_DEVICE_TEXT_CAPACITY];
    char normalized_right[MPH_DEVICE_TEXT_CAPACITY];

    if (!mph_status_is_ok(
            mph_device_normalize_name(left, normalized_left, sizeof(normalized_left))) ||
        !mph_status_is_ok(
            mph_device_normalize_name(right, normalized_right, sizeof(normalized_right)))) {
        return false;
    }

    return normalized_left[0] != '\0' && strcmp(normalized_left, normalized_right) == 0;
}

static bool normalized_contains(const char *haystack, const char *needle) {
    char normalized_haystack[MPH_DEVICE_TEXT_CAPACITY];
    char normalized_needle[MPH_DEVICE_TEXT_CAPACITY];

    if (!mph_status_is_ok(mph_device_normalize_name(haystack, normalized_haystack,
                                                    sizeof(normalized_haystack))) ||
        !mph_status_is_ok(
            mph_device_normalize_name(needle, normalized_needle, sizeof(normalized_needle)))) {
        return false;
    }

    return normalized_needle[0] != '\0' && strstr(normalized_haystack, normalized_needle) != NULL;
}

static bool category_compatible(mph_device_category_t left, mph_device_category_t right) {
    if (left == MPH_DEVICE_CATEGORY_UNKNOWN || right == MPH_DEVICE_CATEGORY_UNKNOWN) {
        return true;
    }

    if (left == right) {
        return true;
    }

    bool left_output =
        left == MPH_DEVICE_CATEGORY_AUDIO_OUTPUT || left == MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT;
    bool right_output = right == MPH_DEVICE_CATEGORY_AUDIO_OUTPUT ||
                        right == MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT;
    return left_output && right_output;
}

static bool transport_compatible(mph_device_transport_t left, mph_device_transport_t right) {
    return left == MPH_DEVICE_TRANSPORT_UNKNOWN || right == MPH_DEVICE_TRANSPORT_UNKNOWN ||
           left == right;
}

static bool has_stable_name(const char *value) {
    char normalized[MPH_DEVICE_TEXT_CAPACITY];
    if (!mph_status_is_ok(mph_device_normalize_name(value, normalized, sizeof(normalized)))) {
        return false;
    }

    return strlen(normalized) >= 3 && strcmp(normalized, "unknown") != 0 &&
           strcmp(normalized, "usb device") != 0 && strcmp(normalized, "bluetooth device") != 0;
}

static int best_score(int current, int candidate) {
    return candidate > current ? candidate : current;
}

static mph_status_t write_fingerprint(char *buffer, size_t capacity, const char *prefix,
                                      const char *category, const char *transport,
                                      const char *primary, const char *secondary) {
    if (buffer == NULL || capacity == 0 || text_is_empty(prefix) || text_is_empty(primary)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    char normalized_primary[MPH_DEVICE_TEXT_CAPACITY];
    char normalized_secondary[MPH_DEVICE_TEXT_CAPACITY];
    mph_status_t status =
        mph_device_normalize_name(primary, normalized_primary, sizeof(normalized_primary));
    if (!mph_status_is_ok(status)) {
        return status;
    }

    normalized_secondary[0] = '\0';
    if (!text_is_empty(secondary)) {
        status = mph_device_normalize_name(secondary, normalized_secondary,
                                           sizeof(normalized_secondary));
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    int written = snprintf(
        buffer, capacity, "%s:%s:%s:%s:%s", prefix, category != NULL ? category : "unknown",
        transport != NULL ? transport : "unknown", normalized_primary, normalized_secondary);
    if (written < 0 || (size_t)written >= capacity) {
        buffer[0] = '\0';
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return MPH_STATUS_OK;
}

void mph_device_init(mph_device_t *device) {
    if (device == NULL) {
        return;
    }

    mph_device_id_clear(&device->id);
    device->category = MPH_DEVICE_CATEGORY_UNKNOWN;
    device->transport = MPH_DEVICE_TRANSPORT_UNKNOWN;
    device->connection_state = MPH_DEVICE_CONNECTION_UNKNOWN;
    device->is_connected = false;
    device->display_name[0] = '\0';
    device->vendor_name[0] = '\0';
    device->model_name[0] = '\0';
    device->serial_number[0] = '\0';

    device->audio.sample_rate_hz = 0.0;
    device->audio.channel_count = 0;
    device->audio.is_default_input = false;
    device->audio.is_default_output = false;
    device->audio.is_default_system_output = false;

    device->display.width_px = 0;
    device->display.height_px = 0;
    device->display.refresh_rate_hz = 0.0;
    device->display.is_main = false;

    device->camera.unique_id[0] = '\0';
    device->camera.supports_global_default = false;

    device->hid.vendor_id = 0;
    device->hid.product_id = 0;
    device->hid.usage_page = 0;
    device->hid.usage = 0;

    device->usb.vendor_id = 0;
    device->usb.product_id = 0;
    device->usb.speed_mbps = 0;
    device->usb.power_ma = 0;

    device->bluetooth.address[0] = '\0';
    device->bluetooth.is_paired = false;
    device->bluetooth.is_connected = false;
    device->bluetooth.class_of_device = 0;
}

mph_status_t mph_device_set_display_name(mph_device_t *device, const char *display_name) {
    return device != NULL ? copy_text(device->display_name, MPH_DEVICE_TEXT_CAPACITY, display_name)
                          : MPH_STATUS_INVALID_ARGUMENT;
}

mph_status_t mph_device_set_vendor_name(mph_device_t *device, const char *vendor_name) {
    return device != NULL ? copy_text(device->vendor_name, MPH_DEVICE_TEXT_CAPACITY, vendor_name)
                          : MPH_STATUS_INVALID_ARGUMENT;
}

mph_status_t mph_device_set_model_name(mph_device_t *device, const char *model_name) {
    return device != NULL ? copy_text(device->model_name, MPH_DEVICE_TEXT_CAPACITY, model_name)
                          : MPH_STATUS_INVALID_ARGUMENT;
}

mph_status_t mph_device_set_serial_number(mph_device_t *device, const char *serial_number) {
    return device != NULL
               ? copy_text(device->serial_number, MPH_DEVICE_SERIAL_CAPACITY, serial_number)
               : MPH_STATUS_INVALID_ARGUMENT;
}

mph_status_t mph_device_set_camera_unique_id(mph_device_t *device, const char *unique_id) {
    return device != NULL ? copy_text(device->camera.unique_id, MPH_DEVICE_TEXT_CAPACITY, unique_id)
                          : MPH_STATUS_INVALID_ARGUMENT;
}

mph_status_t mph_device_set_bluetooth_address(mph_device_t *device, const char *address) {
    return device != NULL ? copy_text(device->bluetooth.address, MPH_DEVICE_TEXT_CAPACITY, address)
                          : MPH_STATUS_INVALID_ARGUMENT;
}

void mph_device_set_connection_state(mph_device_t *device,
                                     mph_device_connection_state_t connection_state) {
    if (device == NULL) {
        return;
    }

    device->connection_state = connection_state;
    device->is_connected = connection_state == MPH_DEVICE_CONNECTION_CONNECTED;
    if (device->category == MPH_DEVICE_CATEGORY_BLUETOOTH) {
        device->bluetooth.is_connected = device->is_connected;
    }
}

const char *mph_device_category_name(mph_device_category_t category) {
    switch (category) {
    case MPH_DEVICE_CATEGORY_DISPLAY:
        return "display";
    case MPH_DEVICE_CATEGORY_AUDIO_INPUT:
        return "audio_input";
    case MPH_DEVICE_CATEGORY_AUDIO_OUTPUT:
        return "audio_output";
    case MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT:
        return "audio_system_output";
    case MPH_DEVICE_CATEGORY_CAMERA:
        return "camera";
    case MPH_DEVICE_CATEGORY_KEYBOARD:
        return "keyboard";
    case MPH_DEVICE_CATEGORY_MOUSE:
        return "mouse";
    case MPH_DEVICE_CATEGORY_TRACKPAD:
        return "trackpad";
    case MPH_DEVICE_CATEGORY_BLUETOOTH:
        return "bluetooth";
    case MPH_DEVICE_CATEGORY_USB:
        return "usb";
    case MPH_DEVICE_CATEGORY_HUB:
        return "hub";
    case MPH_DEVICE_CATEGORY_DOCK:
        return "dock";
    case MPH_DEVICE_CATEGORY_AUDIO_INTERFACE:
        return "audio_interface";
    case MPH_DEVICE_CATEGORY_UNKNOWN:
        return "unknown";
    }

    return "unknown";
}

mph_device_category_t mph_device_category_from_name(const char *name) {
    if (text_is_empty(name)) {
        return MPH_DEVICE_CATEGORY_UNKNOWN;
    }

    for (mph_device_category_t category = MPH_DEVICE_CATEGORY_DISPLAY;
         category <= MPH_DEVICE_CATEGORY_UNKNOWN; category += 1) {
        if (normalized_text_equal(name, mph_device_category_name(category))) {
            return category;
        }
    }

    return MPH_DEVICE_CATEGORY_UNKNOWN;
}

const char *mph_device_transport_name(mph_device_transport_t transport) {
    switch (transport) {
    case MPH_DEVICE_TRANSPORT_BUILT_IN:
        return "built_in";
    case MPH_DEVICE_TRANSPORT_USB:
        return "usb";
    case MPH_DEVICE_TRANSPORT_BLUETOOTH:
        return "bluetooth";
    case MPH_DEVICE_TRANSPORT_THUNDERBOLT:
        return "thunderbolt";
    case MPH_DEVICE_TRANSPORT_HDMI:
        return "hdmi";
    case MPH_DEVICE_TRANSPORT_DISPLAY_PORT:
        return "display_port";
    case MPH_DEVICE_TRANSPORT_VIRTUAL:
        return "virtual";
    case MPH_DEVICE_TRANSPORT_AGGREGATE:
        return "aggregate";
    case MPH_DEVICE_TRANSPORT_UNKNOWN:
        return "unknown";
    }

    return "unknown";
}

const char *mph_device_connection_state_name(mph_device_connection_state_t connection_state) {
    switch (connection_state) {
    case MPH_DEVICE_CONNECTION_CONNECTED:
        return "connected";
    case MPH_DEVICE_CONNECTION_DISCONNECTED:
        return "disconnected";
    case MPH_DEVICE_CONNECTION_UNAVAILABLE:
        return "unavailable";
    case MPH_DEVICE_CONNECTION_UNKNOWN:
        return "unknown";
    }

    return "unknown";
}

mph_device_transport_t mph_device_transport_from_name(const char *name) {
    if (text_is_empty(name)) {
        return MPH_DEVICE_TRANSPORT_UNKNOWN;
    }

    for (mph_device_transport_t transport = MPH_DEVICE_TRANSPORT_BUILT_IN;
         transport <= MPH_DEVICE_TRANSPORT_UNKNOWN; transport += 1) {
        if (normalized_text_equal(name, mph_device_transport_name(transport))) {
            return transport;
        }
    }

    if (normalized_text_equal(name, "displayport")) {
        return MPH_DEVICE_TRANSPORT_DISPLAY_PORT;
    }

    return MPH_DEVICE_TRANSPORT_UNKNOWN;
}

bool mph_device_category_is_audio(mph_device_category_t category) {
    return category == MPH_DEVICE_CATEGORY_AUDIO_INPUT ||
           category == MPH_DEVICE_CATEGORY_AUDIO_OUTPUT ||
           category == MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT ||
           category == MPH_DEVICE_CATEGORY_AUDIO_INTERFACE;
}

mph_status_t mph_device_normalize_name(const char *input, char *output, size_t capacity) {
    if (input == NULL || output == NULL || capacity == 0) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    size_t write_index = 0;
    bool previous_was_separator = true;
    for (size_t read_index = 0; input[read_index] != '\0'; read_index += 1) {
        unsigned char value = (unsigned char)input[read_index];
        bool is_separator = isspace(value) || value == '_' || value == '-' || value == '.' ||
                            value == '/' || value == '\\' || value == ':';

        if (is_separator) {
            if (!previous_was_separator) {
                if (write_index + 1 >= capacity) {
                    output[0] = '\0';
                    return MPH_STATUS_CAPACITY_EXCEEDED;
                }
                output[write_index] = ' ';
                write_index += 1;
                previous_was_separator = true;
            }
            continue;
        }

        if (write_index + 1 >= capacity) {
            output[0] = '\0';
            return MPH_STATUS_CAPACITY_EXCEEDED;
        }

        output[write_index] = (char)tolower(value);
        write_index += 1;
        previous_was_separator = false;
    }

    while (write_index > 0 && output[write_index - 1] == ' ') {
        write_index -= 1;
    }

    output[write_index] = '\0';
    return MPH_STATUS_OK;
}

mph_device_category_t mph_device_infer_category(const mph_device_t *device) {
    if (device == NULL) {
        return MPH_DEVICE_CATEGORY_UNKNOWN;
    }

    if (device->category != MPH_DEVICE_CATEGORY_UNKNOWN) {
        return device->category;
    }

    if (device->display.width_px > 0 || device->display.height_px > 0 ||
        normalized_contains(device->display_name, "display") ||
        normalized_contains(device->display_name, "monitor")) {
        return MPH_DEVICE_CATEGORY_DISPLAY;
    }

    if (!text_is_empty(device->camera.unique_id) ||
        normalized_contains(device->display_name, "camera") ||
        normalized_contains(device->display_name, "webcam") ||
        normalized_contains(device->display_name, "facetime")) {
        return MPH_DEVICE_CATEGORY_CAMERA;
    }

    if (device->hid.usage_page == 0x01 && device->hid.usage == 0x06) {
        return MPH_DEVICE_CATEGORY_KEYBOARD;
    }

    if (device->hid.usage_page == 0x01 && device->hid.usage == 0x02) {
        return MPH_DEVICE_CATEGORY_MOUSE;
    }

    if ((device->hid.usage_page == 0x0D && device->hid.usage == 0x05) ||
        normalized_contains(device->display_name, "trackpad")) {
        return MPH_DEVICE_CATEGORY_TRACKPAD;
    }

    if (device->audio.is_default_input || normalized_contains(device->display_name, "line in")) {
        return MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    }

    if (device->audio.is_default_system_output) {
        return MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT;
    }

    if (device->audio.is_default_output || normalized_contains(device->display_name, "line out")) {
        return MPH_DEVICE_CATEGORY_AUDIO_OUTPUT;
    }

    if (normalized_contains(device->display_name, "microphone") ||
        normalized_contains(device->display_name, "mic")) {
        return MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    }

    if (normalized_contains(device->display_name, "speaker") ||
        normalized_contains(device->display_name, "headphone") ||
        normalized_contains(device->display_name, "airpods")) {
        return MPH_DEVICE_CATEGORY_AUDIO_OUTPUT;
    }

    if (normalized_contains(device->display_name, "audio interface") ||
        normalized_contains(device->display_name, "audio card")) {
        return MPH_DEVICE_CATEGORY_AUDIO_INTERFACE;
    }

    if (normalized_contains(device->display_name, "dock")) {
        return MPH_DEVICE_CATEGORY_DOCK;
    }

    if (normalized_contains(device->display_name, "hub")) {
        return MPH_DEVICE_CATEGORY_HUB;
    }

    if (!text_is_empty(device->bluetooth.address) ||
        device->transport == MPH_DEVICE_TRANSPORT_BLUETOOTH) {
        return MPH_DEVICE_CATEGORY_BLUETOOTH;
    }

    if (device->transport == MPH_DEVICE_TRANSPORT_USB || device->usb.vendor_id != 0 ||
        device->usb.product_id != 0) {
        return MPH_DEVICE_CATEGORY_USB;
    }

    return MPH_DEVICE_CATEGORY_UNKNOWN;
}

void mph_device_apply_inferred_category(mph_device_t *device) {
    if (device == NULL || device->category != MPH_DEVICE_CATEGORY_UNKNOWN) {
        return;
    }

    device->category = mph_device_infer_category(device);
}

mph_status_t mph_device_fingerprint(const mph_device_t *device, char *buffer, size_t capacity) {
    if (device == NULL || buffer == NULL || capacity == 0) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    const char *category = mph_device_category_name(mph_device_infer_category(device));
    const char *transport = mph_device_transport_name(device->transport);

    if (!mph_device_id_is_empty(&device->id)) {
        return write_fingerprint(buffer, capacity, "id", category, transport,
                                 mph_device_id_cstr(&device->id), NULL);
    }

    if (!text_is_empty(device->serial_number)) {
        return write_fingerprint(buffer, capacity, "serial", category, transport,
                                 device->serial_number, device->model_name);
    }

    if (!text_is_empty(device->bluetooth.address)) {
        return write_fingerprint(buffer, capacity, "bt", category, transport,
                                 device->bluetooth.address, NULL);
    }

    if (!text_is_empty(device->camera.unique_id)) {
        return write_fingerprint(buffer, capacity, "camera", category, transport,
                                 device->camera.unique_id, NULL);
    }

    if (device->hid.vendor_id != 0 && device->hid.product_id != 0) {
        int written = snprintf(buffer, capacity, "hid:%s:%s:%04x:%04x:%u:%u", category, transport,
                               device->hid.vendor_id, device->hid.product_id,
                               device->hid.usage_page, device->hid.usage);
        if (written < 0 || (size_t)written >= capacity) {
            buffer[0] = '\0';
            return MPH_STATUS_CAPACITY_EXCEEDED;
        }
        return MPH_STATUS_OK;
    }

    if (device->usb.vendor_id != 0 && device->usb.product_id != 0) {
        int written = snprintf(buffer, capacity, "usb:%s:%s:%04x:%04x", category, transport,
                               device->usb.vendor_id, device->usb.product_id);
        if (written < 0 || (size_t)written >= capacity) {
            buffer[0] = '\0';
            return MPH_STATUS_CAPACITY_EXCEEDED;
        }
        return MPH_STATUS_OK;
    }

    if (has_stable_name(device->display_name)) {
        return write_fingerprint(buffer, capacity, "name", category, transport,
                                 device->display_name, device->vendor_name);
    }

    return MPH_STATUS_NOT_FOUND;
}

int mph_device_match_score(const mph_device_t *known_device, const mph_device_t *candidate_device) {
    if (known_device == NULL || candidate_device == NULL) {
        return 0;
    }

    if (!mph_device_id_is_empty(&known_device->id) &&
        mph_device_id_equal(&known_device->id, &candidate_device->id)) {
        return MPH_DEVICE_MATCH_EXACT_ID;
    }

    if (!category_compatible(mph_device_infer_category(known_device),
                             mph_device_infer_category(candidate_device)) ||
        !transport_compatible(known_device->transport, candidate_device->transport)) {
        return 0;
    }

    int score = 0;

    if (text_equal(known_device->serial_number, candidate_device->serial_number)) {
        score = best_score(score, MPH_DEVICE_MATCH_SERIAL);
    }

    if (text_equal(known_device->bluetooth.address, candidate_device->bluetooth.address) ||
        text_equal(known_device->camera.unique_id, candidate_device->camera.unique_id)) {
        score = best_score(score, MPH_DEVICE_MATCH_UNIQUE_ID);
    }

    if (known_device->hid.vendor_id != 0 && known_device->hid.product_id != 0 &&
        known_device->hid.vendor_id == candidate_device->hid.vendor_id &&
        known_device->hid.product_id == candidate_device->hid.product_id &&
        known_device->hid.usage_page == candidate_device->hid.usage_page &&
        known_device->hid.usage == candidate_device->hid.usage) {
        score = best_score(score, MPH_DEVICE_MATCH_VENDOR_PRODUCT);
    }

    if (known_device->usb.vendor_id != 0 && known_device->usb.product_id != 0 &&
        known_device->usb.vendor_id == candidate_device->usb.vendor_id &&
        known_device->usb.product_id == candidate_device->usb.product_id) {
        score = best_score(score, MPH_DEVICE_MATCH_VENDOR_PRODUCT);
    }

    if (has_stable_name(known_device->display_name) &&
        normalized_text_equal(known_device->display_name, candidate_device->display_name)) {
        if (normalized_text_equal(known_device->vendor_name, candidate_device->vendor_name) ||
            normalized_text_equal(known_device->model_name, candidate_device->model_name)) {
            score = best_score(score, MPH_DEVICE_MATCH_VENDOR_MODEL_NAME);
        } else {
            score = best_score(score, MPH_DEVICE_MATCH_NAME_ONLY);
        }
    }

    return score;
}

bool mph_device_is_probable_match(const mph_device_t *known_device,
                                  const mph_device_t *candidate_device) {
    return mph_device_match_score(known_device, candidate_device) >= MPH_DEVICE_MATCH_THRESHOLD;
}
