#include "mph_display.h"

#include "mph_log.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t serial_numeric;
    char product_name[MPH_DISPLAY_TEXT_CAPACITY];
} display_iokit_metadata_t;

typedef struct {
    char *buffer;
    size_t capacity;
    bool copied;
} cf_string_copy_context_t;

static bool text_is_empty(const char *value) {
    return value == NULL || value[0] == '\0';
}

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

static bool text_contains_folded(const char *haystack, const char *needle) {
    if (text_is_empty(haystack) || text_is_empty(needle)) {
        return false;
    }

    size_t needle_length = strlen(needle);
    if (needle_length == 0) {
        return false;
    }

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

static bool display_text_contains(const mph_display_raw_device_t *raw_device, const char *needle) {
    return raw_device != NULL && (text_contains_folded(raw_device->name, needle) ||
                                  text_contains_folded(raw_device->vendor_name, needle) ||
                                  text_contains_folded(raw_device->model_name, needle));
}

static uint32_t cf_dictionary_u32(CFDictionaryRef dictionary, CFStringRef key) {
    if (dictionary == NULL || key == NULL) {
        return 0;
    }

    CFTypeRef value = CFDictionaryGetValue(dictionary, key);
    if (value == NULL || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return 0;
    }

    int number = 0;
    if (!CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &number) || number < 0) {
        return 0;
    }

    return (uint32_t)number;
}

static void copy_first_cf_string_value(const void *key, const void *value, void *context) {
    (void)key;
    cf_string_copy_context_t *copy_context = (cf_string_copy_context_t *)context;
    if (copy_context == NULL || copy_context->copied || value == NULL ||
        CFGetTypeID(value) != CFStringGetTypeID()) {
        return;
    }

    copy_context->copied =
        CFStringGetCString((CFStringRef)value, copy_context->buffer,
                           (CFIndex)copy_context->capacity, kCFStringEncodingUTF8);
    if (!copy_context->copied && copy_context->capacity > 0) {
        copy_context->buffer[0] = '\0';
    }
}

static void copy_display_product_name(CFDictionaryRef info, char *buffer, size_t capacity) {
    if (info == NULL || buffer == NULL || capacity == 0) {
        return;
    }

    buffer[0] = '\0';
    CFTypeRef names = CFDictionaryGetValue(info, CFSTR(kDisplayProductName));
    if (names == NULL || CFGetTypeID(names) != CFDictionaryGetTypeID()) {
        return;
    }

    cf_string_copy_context_t context = {
        buffer,
        capacity,
        false,
    };
    CFDictionaryApplyFunction((CFDictionaryRef)names, copy_first_cf_string_value, &context);
}

static bool display_metadata_matches(const display_iokit_metadata_t *metadata, uint32_t vendor_id,
                                     uint32_t product_id, uint32_t serial_numeric) {
    if (metadata == NULL || metadata->vendor_id != vendor_id ||
        metadata->product_id != product_id) {
        return false;
    }

    return serial_numeric == 0 || metadata->serial_numeric == 0 ||
           metadata->serial_numeric == serial_numeric;
}

static bool copy_iokit_display_metadata(uint32_t vendor_id, uint32_t product_id,
                                        uint32_t serial_numeric,
                                        display_iokit_metadata_t *out_metadata) {
    if (out_metadata == NULL || vendor_id == 0 || product_id == 0) {
        return false;
    }

    out_metadata->vendor_id = 0;
    out_metadata->product_id = 0;
    out_metadata->serial_numeric = 0;
    out_metadata->product_name[0] = '\0';

    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t status = IOServiceGetMatchingServices(
        kIOMainPortDefault, IOServiceMatching("IODisplayConnect"), &iterator);
    if (status != KERN_SUCCESS) {
        return false;
    }

    bool found = false;
    io_service_t service = IO_OBJECT_NULL;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        CFDictionaryRef info = IODisplayCreateInfoDictionary(service, kIODisplayOnlyPreferredName);
        if (info != NULL) {
            display_iokit_metadata_t candidate;
            candidate.vendor_id = cf_dictionary_u32(info, CFSTR(kDisplayVendorID));
            candidate.product_id = cf_dictionary_u32(info, CFSTR(kDisplayProductID));
            candidate.serial_numeric = cf_dictionary_u32(info, CFSTR(kDisplaySerialNumber));
            candidate.product_name[0] = '\0';
            copy_display_product_name(info, candidate.product_name, sizeof(candidate.product_name));

            if (display_metadata_matches(&candidate, vendor_id, product_id, serial_numeric)) {
                *out_metadata = candidate;
                found = true;
            }
            CFRelease(info);
        }

        IOObjectRelease(service);
        if (found) {
            break;
        }
    }

    IOObjectRelease(iterator);
    return found;
}

static void apply_iokit_metadata(mph_display_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return;
    }

    display_iokit_metadata_t metadata;
    if (!copy_iokit_display_metadata(raw_device->vendor_id, raw_device->product_id,
                                     raw_device->serial_numeric, &metadata)) {
        return;
    }

    if (raw_device->serial_numeric == 0) {
        raw_device->serial_numeric = metadata.serial_numeric;
    }

    if (raw_device->name[0] == '\0' && metadata.product_name[0] != '\0') {
        (void)copy_text(raw_device->name, sizeof(raw_device->name), metadata.product_name);
    }
    if (raw_device->model_name[0] == '\0' && metadata.product_name[0] != '\0') {
        (void)copy_text(raw_device->model_name, sizeof(raw_device->model_name),
                        metadata.product_name);
    }
}

static double display_refresh_rate(CGDirectDisplayID display_id) {
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);
    if (mode == NULL) {
        return 0.0;
    }

    double refresh_rate = CGDisplayModeGetRefreshRate(mode);
    CGDisplayModeRelease(mode);
    return refresh_rate > 0.0 ? refresh_rate : 0.0;
}

static mph_status_t fill_raw_display(CGDirectDisplayID display_id,
                                     CGDirectDisplayID main_display_id,
                                     mph_display_raw_device_t *out_raw) {
    if (out_raw == NULL || display_id == 0) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_display_raw_device_init(out_raw);
    out_raw->display_id = (uint32_t)display_id;
    out_raw->vendor_id = CGDisplayVendorNumber(display_id);
    out_raw->product_id = CGDisplayModelNumber(display_id);
    out_raw->serial_numeric = CGDisplaySerialNumber(display_id);
    out_raw->width_px = (uint32_t)CGDisplayPixelsWide(display_id);
    out_raw->height_px = (uint32_t)CGDisplayPixelsHigh(display_id);
    out_raw->refresh_rate_hz = display_refresh_rate(display_id);
    out_raw->is_main = display_id == main_display_id;
    out_raw->is_builtin = CGDisplayIsBuiltin(display_id) != 0;
    out_raw->is_online = true;
    out_raw->transport =
        out_raw->is_builtin ? MPH_DEVICE_TRANSPORT_BUILT_IN : MPH_DEVICE_TRANSPORT_UNKNOWN;

    if (out_raw->serial_numeric != 0) {
        snprintf(out_raw->serial_number, sizeof(out_raw->serial_number), "%u",
                 out_raw->serial_numeric);
    }

    apply_iokit_metadata(out_raw);
    if (out_raw->name[0] == '\0') {
        snprintf(out_raw->name, sizeof(out_raw->name), "Display %u", out_raw->display_id);
    }

    return MPH_STATUS_OK;
}

void mph_display_raw_device_init(mph_display_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return;
    }

    raw_device->display_id = 0;
    raw_device->name[0] = '\0';
    raw_device->vendor_name[0] = '\0';
    raw_device->model_name[0] = '\0';
    raw_device->serial_number[0] = '\0';
    raw_device->vendor_id = 0;
    raw_device->product_id = 0;
    raw_device->serial_numeric = 0;
    raw_device->width_px = 0;
    raw_device->height_px = 0;
    raw_device->refresh_rate_hz = 0.0;
    raw_device->is_main = false;
    raw_device->is_builtin = false;
    raw_device->is_online = false;
    raw_device->transport = MPH_DEVICE_TRANSPORT_UNKNOWN;
}

mph_status_t mph_display_device_id(mph_device_id_t *out_device_id,
                                   const mph_display_raw_device_t *raw_device) {
    if (out_device_id == NULL || raw_device == NULL || raw_device->display_id == 0) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    char stable_value[MPH_DEVICE_ID_CAPACITY];
    int written = 0;
    if (raw_device->vendor_id != 0 || raw_device->product_id != 0 ||
        raw_device->serial_numeric != 0) {
        written =
            snprintf(stable_value, sizeof(stable_value), "v%u-p%u-s%u-cg%u", raw_device->vendor_id,
                     raw_device->product_id, raw_device->serial_numeric, raw_device->display_id);
    } else {
        written = snprintf(stable_value, sizeof(stable_value), "cg%u", raw_device->display_id);
    }

    if (written < 0 || (size_t)written >= sizeof(stable_value)) {
        mph_device_id_clear(out_device_id);
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return mph_device_id_from_parts(out_device_id, "display", stable_value);
}

mph_device_transport_t mph_display_infer_transport(const mph_display_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return MPH_DEVICE_TRANSPORT_UNKNOWN;
    }

    if (raw_device->is_builtin) {
        return MPH_DEVICE_TRANSPORT_BUILT_IN;
    }

    if (raw_device->transport != MPH_DEVICE_TRANSPORT_UNKNOWN) {
        return raw_device->transport;
    }

    if (display_text_contains(raw_device, "thunderbolt") ||
        display_text_contains(raw_device, "usb-c") || display_text_contains(raw_device, "usbc")) {
        return MPH_DEVICE_TRANSPORT_THUNDERBOLT;
    }

    if (display_text_contains(raw_device, "displayport") ||
        display_text_contains(raw_device, "display port")) {
        return MPH_DEVICE_TRANSPORT_DISPLAY_PORT;
    }

    if (display_text_contains(raw_device, "hdmi")) {
        return MPH_DEVICE_TRANSPORT_HDMI;
    }

    return MPH_DEVICE_TRANSPORT_UNKNOWN;
}

mph_status_t mph_display_map_raw_device(const mph_display_raw_device_t *raw_device,
                                        mph_device_t *out_device) {
    if (raw_device == NULL || out_device == NULL || raw_device->display_id == 0) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_t device;
    mph_device_init(&device);

    mph_status_t status = mph_display_device_id(&device.id, raw_device);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    device.category = MPH_DEVICE_CATEGORY_DISPLAY;
    device.transport = mph_display_infer_transport(raw_device);
    mph_device_set_connection_state(&device, raw_device->is_online
                                                 ? MPH_DEVICE_CONNECTION_CONNECTED
                                                 : MPH_DEVICE_CONNECTION_DISCONNECTED);

    if (!text_is_empty(raw_device->name)) {
        status = mph_device_set_display_name(&device, raw_device->name);
    } else {
        char fallback_name[MPH_DISPLAY_TEXT_CAPACITY];
        snprintf(fallback_name, sizeof(fallback_name), "Display %u", raw_device->display_id);
        status = mph_device_set_display_name(&device, fallback_name);
    }
    if (!mph_status_is_ok(status)) {
        return status;
    }

    if (!text_is_empty(raw_device->vendor_name)) {
        status = mph_device_set_vendor_name(&device, raw_device->vendor_name);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    } else if (raw_device->vendor_id != 0) {
        char vendor_name[MPH_DISPLAY_TEXT_CAPACITY];
        snprintf(vendor_name, sizeof(vendor_name), "vendor:%u", raw_device->vendor_id);
        status = mph_device_set_vendor_name(&device, vendor_name);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    if (!text_is_empty(raw_device->model_name)) {
        status = mph_device_set_model_name(&device, raw_device->model_name);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    } else if (raw_device->product_id != 0) {
        char model_name[MPH_DISPLAY_TEXT_CAPACITY];
        snprintf(model_name, sizeof(model_name), "product:%u", raw_device->product_id);
        status = mph_device_set_model_name(&device, model_name);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    if (!text_is_empty(raw_device->serial_number)) {
        status = mph_device_set_serial_number(&device, raw_device->serial_number);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    } else if (raw_device->serial_numeric != 0) {
        char serial_number[MPH_DEVICE_SERIAL_CAPACITY];
        snprintf(serial_number, sizeof(serial_number), "%u", raw_device->serial_numeric);
        status = mph_device_set_serial_number(&device, serial_number);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    device.display.width_px = raw_device->width_px;
    device.display.height_px = raw_device->height_px;
    device.display.refresh_rate_hz = raw_device->refresh_rate_hz;
    device.display.is_main = raw_device->is_main;

    *out_device = device;
    return MPH_STATUS_OK;
}

mph_status_t mph_display_enumerate_devices(mph_device_list_t *out_devices) {
    if (out_devices == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    uint32_t display_count = 0;
    CGError count_status = CGGetOnlineDisplayList(0, NULL, &display_count);
    if (count_status != kCGErrorSuccess) {
        mph_log_message(MPH_LOG_LEVEL_ERROR, "display", "CGGetOnlineDisplayList(count) failed");
        return MPH_STATUS_INTERNAL_ERROR;
    }

    if (display_count == 0) {
        return MPH_STATUS_OK;
    }

    CGDirectDisplayID *display_ids =
        (CGDirectDisplayID *)calloc(display_count, sizeof(*display_ids));
    if (display_ids == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    CGError list_status = CGGetOnlineDisplayList(display_count, display_ids, &display_count);
    if (list_status != kCGErrorSuccess) {
        free(display_ids);
        mph_log_message(MPH_LOG_LEVEL_ERROR, "display", "CGGetOnlineDisplayList(list) failed");
        return MPH_STATUS_INTERNAL_ERROR;
    }

    CGDirectDisplayID main_display_id = CGMainDisplayID();
    for (uint32_t index = 0; index < display_count; index += 1) {
        mph_display_raw_device_t raw_device;
        mph_status_t status = fill_raw_display(display_ids[index], main_display_id, &raw_device);
        if (!mph_status_is_ok(status)) {
            continue;
        }

        mph_device_t device;
        status = mph_display_map_raw_device(&raw_device, &device);
        if (!mph_status_is_ok(status)) {
            free(display_ids);
            return status;
        }

        status = mph_device_list_append(out_devices, &device);
        if (!mph_status_is_ok(status)) {
            free(display_ids);
            return status;
        }
    }

    free(display_ids);
    return MPH_STATUS_OK;
}
