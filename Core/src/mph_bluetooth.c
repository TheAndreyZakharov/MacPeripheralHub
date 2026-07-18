#include "mph_bluetooth.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MPH_BLUETOOTH_ID_HASH_CAPACITY 32
#define MPH_BLUETOOTH_MAJOR_DEVICE_AUDIO_VIDEO 4
#define MPH_BLUETOOTH_MAJOR_DEVICE_PERIPHERAL 5

static bool text_is_empty(const char *value) {
    return value == NULL || value[0] == '\0';
}

static uint64_t fnv1a_hash(const char *value) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (value == NULL) {
        return hash;
    }

    for (size_t index = 0; value[index] != '\0'; index += 1) {
        hash ^= (unsigned char)value[index];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static bool text_contains_folded(const char *haystack, const char *needle) {
    if (text_is_empty(haystack) || text_is_empty(needle)) {
        return false;
    }

    size_t needle_length = strlen(needle);
    for (size_t haystack_index = 0; haystack[haystack_index] != '\0'; haystack_index += 1) {
        size_t needle_index = 0;
        while (needle_index < needle_length && haystack[haystack_index + needle_index] != '\0' &&
               tolower((unsigned char)haystack[haystack_index + needle_index]) ==
                   tolower((unsigned char)needle[needle_index])) {
            needle_index += 1;
        }
        if (needle_index == needle_length) {
            return true;
        }
    }

    return false;
}

static bool normalized_text_equal(const char *left, const char *right) {
    char normalized_left[MPH_DEVICE_TEXT_CAPACITY];
    char normalized_right[MPH_DEVICE_TEXT_CAPACITY];
    if (mph_device_normalize_name(left, normalized_left, sizeof(normalized_left)) !=
            MPH_STATUS_OK ||
        mph_device_normalize_name(right, normalized_right, sizeof(normalized_right)) !=
            MPH_STATUS_OK) {
        return false;
    }

    return normalized_left[0] != '\0' && strcmp(normalized_left, normalized_right) == 0;
}

static uint32_t bluetooth_major_device_class(const mph_bluetooth_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return 0;
    }

    return raw_device->major_device_class != 0 ? raw_device->major_device_class
                                               : ((raw_device->class_of_device >> 8) & 0x1F);
}

static bool bluetooth_name_contains(const mph_bluetooth_raw_device_t *raw_device,
                                    const char *needle) {
    return raw_device != NULL && text_contains_folded(raw_device->name, needle);
}

static bool bluetooth_has_meaningful_name(const mph_bluetooth_raw_device_t *raw_device) {
    if (raw_device == NULL || text_is_empty(raw_device->name)) {
        return false;
    }

    char normalized[MPH_DEVICE_TEXT_CAPACITY];
    if (mph_device_normalize_name(raw_device->name, normalized, sizeof(normalized)) !=
        MPH_STATUS_OK) {
        return false;
    }

    return strcmp(normalized, "bluetooth device") != 0 &&
           strcmp(normalized, "unknown bluetooth device") != 0;
}

void mph_bluetooth_raw_device_init(mph_bluetooth_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return;
    }

    raw_device->name[0] = '\0';
    raw_device->address[0] = '\0';
    raw_device->class_of_device = 0;
    raw_device->major_device_class = 0;
    raw_device->minor_device_class = 0;
    raw_device->major_service_class = 0;
    raw_device->is_paired = false;
    raw_device->is_connected = false;
}

mph_status_t mph_bluetooth_normalize_address(const char *address, char *buffer, size_t capacity) {
    if (address == NULL || buffer == NULL || capacity == 0) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    buffer[0] = '\0';
    char digits[12];
    size_t digit_count = 0;
    for (size_t index = 0; address[index] != '\0'; index += 1) {
        unsigned char value = (unsigned char)address[index];
        if (isxdigit(value)) {
            if (digit_count >= sizeof(digits)) {
                return MPH_STATUS_INVALID_ARGUMENT;
            }
            digits[digit_count] = (char)tolower(value);
            digit_count += 1;
        } else if (value != ':' && value != '-' && !isspace(value)) {
            return MPH_STATUS_INVALID_ARGUMENT;
        }
    }

    if (digit_count == 0) {
        return MPH_STATUS_NOT_FOUND;
    }
    if (digit_count != sizeof(digits)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }
    if (capacity < 18) {
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    int written = snprintf(buffer, capacity, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c", digits[0], digits[1],
                           digits[2], digits[3], digits[4], digits[5], digits[6], digits[7],
                           digits[8], digits[9], digits[10], digits[11]);
    return written > 0 && (size_t)written < capacity ? MPH_STATUS_OK : MPH_STATUS_CAPACITY_EXCEEDED;
}

bool mph_bluetooth_is_audio_device(const mph_bluetooth_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return false;
    }

    if (bluetooth_major_device_class(raw_device) == MPH_BLUETOOTH_MAJOR_DEVICE_AUDIO_VIDEO) {
        return true;
    }

    return bluetooth_name_contains(raw_device, "airpods") ||
           bluetooth_name_contains(raw_device, "headphone") ||
           bluetooth_name_contains(raw_device, "headset") ||
           bluetooth_name_contains(raw_device, "speaker") ||
           bluetooth_name_contains(raw_device, "microphone") ||
           bluetooth_name_contains(raw_device, "audio");
}

bool mph_bluetooth_is_hid_device(const mph_bluetooth_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return false;
    }

    if (bluetooth_major_device_class(raw_device) == MPH_BLUETOOTH_MAJOR_DEVICE_PERIPHERAL) {
        return true;
    }

    return bluetooth_name_contains(raw_device, "keyboard") ||
           bluetooth_name_contains(raw_device, "mouse") ||
           bluetooth_name_contains(raw_device, "trackpad") ||
           bluetooth_name_contains(raw_device, "touchpad");
}

mph_device_category_t mph_bluetooth_infer_category(const mph_bluetooth_raw_device_t *raw_device) {
    return raw_device != NULL ? MPH_DEVICE_CATEGORY_BLUETOOTH : MPH_DEVICE_CATEGORY_UNKNOWN;
}

mph_status_t mph_bluetooth_device_id(mph_device_id_t *out_device_id,
                                     const mph_bluetooth_raw_device_t *raw_device) {
    if (out_device_id == NULL || raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    char normalized_address[MPH_BLUETOOTH_ADDRESS_CAPACITY];
    mph_status_t status = mph_bluetooth_normalize_address(raw_device->address, normalized_address,
                                                          sizeof(normalized_address));
    if (mph_status_is_ok(status)) {
        return mph_device_id_from_parts(out_device_id, "bluetooth", normalized_address);
    }

    if (bluetooth_has_meaningful_name(raw_device)) {
        char hashed_name[MPH_BLUETOOTH_ID_HASH_CAPACITY];
        snprintf(hashed_name, sizeof(hashed_name), "name-%016" PRIx64,
                 fnv1a_hash(raw_device->name));
        return mph_device_id_from_parts(out_device_id, "bluetooth", hashed_name);
    }

    return MPH_STATUS_NOT_FOUND;
}

bool mph_bluetooth_matches_device(const mph_bluetooth_raw_device_t *raw_device,
                                  const mph_device_t *device) {
    if (raw_device == NULL || device == NULL) {
        return false;
    }

    char raw_address[MPH_BLUETOOTH_ADDRESS_CAPACITY];
    char device_address[MPH_BLUETOOTH_ADDRESS_CAPACITY];
    if (mph_status_is_ok(mph_bluetooth_normalize_address(raw_device->address, raw_address,
                                                         sizeof(raw_address))) &&
        mph_status_is_ok(mph_bluetooth_normalize_address(device->bluetooth.address, device_address,
                                                         sizeof(device_address))) &&
        strcmp(raw_address, device_address) == 0) {
        return true;
    }

    if (device->transport != MPH_DEVICE_TRANSPORT_BLUETOOTH ||
        !normalized_text_equal(raw_device->name, device->display_name)) {
        return false;
    }

    if (mph_device_category_is_audio(device->category) &&
        mph_bluetooth_is_audio_device(raw_device)) {
        return true;
    }

    bool hid_category = device->category == MPH_DEVICE_CATEGORY_KEYBOARD ||
                        device->category == MPH_DEVICE_CATEGORY_MOUSE ||
                        device->category == MPH_DEVICE_CATEGORY_TRACKPAD;
    if (hid_category && mph_bluetooth_is_hid_device(raw_device)) {
        return true;
    }

    return device->category == MPH_DEVICE_CATEGORY_BLUETOOTH;
}

mph_status_t mph_bluetooth_map_raw_device(const mph_bluetooth_raw_device_t *raw_device,
                                          mph_device_t *out_device) {
    if (raw_device == NULL || out_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_t device;
    mph_device_init(&device);
    mph_status_t status = mph_bluetooth_device_id(&device.id, raw_device);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    device.category = mph_bluetooth_infer_category(raw_device);
    device.transport = MPH_DEVICE_TRANSPORT_BLUETOOTH;
    mph_device_set_connection_state(&device, raw_device->is_connected
                                                 ? MPH_DEVICE_CONNECTION_CONNECTED
                                                 : MPH_DEVICE_CONNECTION_DISCONNECTED);

    const char *display_name = raw_device->name;
    if (text_is_empty(display_name)) {
        display_name = raw_device->address[0] != '\0' ? raw_device->address : "Bluetooth Device";
    }
    status = mph_device_set_display_name(&device, display_name);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    char normalized_address[MPH_BLUETOOTH_ADDRESS_CAPACITY];
    status = mph_bluetooth_normalize_address(raw_device->address, normalized_address,
                                             sizeof(normalized_address));
    if (mph_status_is_ok(status)) {
        status = mph_device_set_bluetooth_address(&device, normalized_address);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    device.bluetooth.is_paired = raw_device->is_paired;
    device.bluetooth.is_connected = raw_device->is_connected;
    device.bluetooth.class_of_device = raw_device->class_of_device;

    *out_device = device;
    return MPH_STATUS_OK;
}

mph_status_t mph_bluetooth_append_mapped_device(mph_device_list_t *out_devices,
                                                const mph_bluetooth_raw_device_t *raw_device) {
    if (out_devices == NULL || raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_t device;
    mph_status_t status = mph_bluetooth_map_raw_device(raw_device, &device);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    if (mph_device_list_find_by_id(out_devices, &device.id) != NULL) {
        return MPH_STATUS_OK;
    }

    for (size_t index = 0; index < mph_device_list_count(out_devices); index += 1) {
        const mph_device_t *existing = mph_device_list_get(out_devices, index);
        if (mph_bluetooth_matches_device(raw_device, existing)) {
            return MPH_STATUS_OK;
        }
    }

    return mph_device_list_append(out_devices, &device);
}
