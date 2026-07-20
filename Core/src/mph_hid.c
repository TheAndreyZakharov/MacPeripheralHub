#include "mph_hid.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDDeviceKeys.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MPH_HID_ID_HASH_CAPACITY 32

typedef struct {
    mph_device_list_t *out_devices;
    mph_status_t status;
} hid_enumeration_context_t;

static bool text_is_empty(const char *value) {
    return value == NULL || value[0] == '\0';
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

static bool hid_text_contains(const mph_hid_raw_device_t *raw_device, const char *needle) {
    return raw_device != NULL && (text_contains_folded(raw_device->product_name, needle) ||
                                  text_contains_folded(raw_device->manufacturer, needle) ||
                                  text_contains_folded(raw_device->transport_name, needle));
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

static uint32_t hid_u32_property(IOHIDDeviceRef device, CFStringRef key) {
    CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (value == NULL || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return 0;
    }

    int number = 0;
    if (!CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &number) || number < 0) {
        return 0;
    }

    return (uint32_t)number;
}

static void hid_string_property(IOHIDDeviceRef device, CFStringRef key, char *buffer,
                                size_t capacity) {
    if (buffer == NULL || capacity == 0) {
        return;
    }

    buffer[0] = '\0';
    CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID()) {
        return;
    }

    if (!CFStringGetCString((CFStringRef)value, buffer, (CFIndex)capacity, kCFStringEncodingUTF8)) {
        buffer[0] = '\0';
    }
}

static uint64_t hid_registry_id(IOHIDDeviceRef device) {
    io_service_t service = IOHIDDeviceGetService(device);
    if (service == IO_OBJECT_NULL) {
        return 0;
    }

    uint64_t registry_id = 0;
    return IORegistryEntryGetRegistryEntryID(service, &registry_id) == KERN_SUCCESS ? registry_id
                                                                                    : 0;
}

static mph_status_t fill_raw_hid_device(IOHIDDeviceRef device,
                                        mph_hid_raw_device_t *out_raw_device) {
    if (device == NULL || out_raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_hid_raw_device_init(out_raw_device);
    out_raw_device->vendor_id = hid_u32_property(device, CFSTR(kIOHIDVendorIDKey));
    out_raw_device->product_id = hid_u32_property(device, CFSTR(kIOHIDProductIDKey));
    out_raw_device->usage_page = hid_u32_property(device, CFSTR(kIOHIDPrimaryUsagePageKey));
    out_raw_device->usage = hid_u32_property(device, CFSTR(kIOHIDPrimaryUsageKey));
    out_raw_device->registry_id = hid_registry_id(device);
    hid_string_property(device, CFSTR(kIOHIDProductKey), out_raw_device->product_name,
                        sizeof(out_raw_device->product_name));
    hid_string_property(device, CFSTR(kIOHIDManufacturerKey), out_raw_device->manufacturer,
                        sizeof(out_raw_device->manufacturer));
    hid_string_property(device, CFSTR(kIOHIDSerialNumberKey), out_raw_device->serial_number,
                        sizeof(out_raw_device->serial_number));
    hid_string_property(device, CFSTR(kIOHIDTransportKey), out_raw_device->transport_name,
                        sizeof(out_raw_device->transport_name));
    out_raw_device->is_connected = true;
    return MPH_STATUS_OK;
}

void mph_hid_raw_device_init(mph_hid_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return;
    }

    raw_device->vendor_id = 0;
    raw_device->product_id = 0;
    raw_device->usage_page = 0;
    raw_device->usage = 0;
    raw_device->registry_id = 0;
    raw_device->product_name[0] = '\0';
    raw_device->manufacturer[0] = '\0';
    raw_device->serial_number[0] = '\0';
    raw_device->transport_name[0] = '\0';
    raw_device->is_connected = false;
}

mph_device_category_t mph_hid_infer_category(const mph_hid_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return MPH_DEVICE_CATEGORY_UNKNOWN;
    }

    if (raw_device->usage_page == kHIDPage_GenericDesktop &&
        raw_device->usage == kHIDUsage_GD_Keyboard) {
        return MPH_DEVICE_CATEGORY_KEYBOARD;
    }

    if (raw_device->usage_page == kHIDPage_GenericDesktop &&
        raw_device->usage == kHIDUsage_GD_Mouse) {
        return MPH_DEVICE_CATEGORY_MOUSE;
    }

    if (raw_device->usage_page == kHIDPage_Digitizer &&
        raw_device->usage == kHIDUsage_Dig_TouchPad) {
        return MPH_DEVICE_CATEGORY_TRACKPAD;
    }

    if (hid_text_contains(raw_device, "keyboard")) {
        return MPH_DEVICE_CATEGORY_KEYBOARD;
    }

    if (hid_text_contains(raw_device, "mouse")) {
        return MPH_DEVICE_CATEGORY_MOUSE;
    }

    if (hid_text_contains(raw_device, "trackpad") || hid_text_contains(raw_device, "touchpad")) {
        return MPH_DEVICE_CATEGORY_TRACKPAD;
    }

    return MPH_DEVICE_CATEGORY_UNKNOWN;
}

mph_device_transport_t mph_hid_infer_transport(const mph_hid_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return MPH_DEVICE_TRANSPORT_UNKNOWN;
    }

    if (hid_text_contains(raw_device, "bluetooth") || hid_text_contains(raw_device, "ble")) {
        return MPH_DEVICE_TRANSPORT_BLUETOOTH;
    }

    if (hid_text_contains(raw_device, "usb")) {
        return MPH_DEVICE_TRANSPORT_USB;
    }

    if (hid_text_contains(raw_device, "built-in") || hid_text_contains(raw_device, "builtin") ||
        hid_text_contains(raw_device, "spi") || hid_text_contains(raw_device, "i2c")) {
        return MPH_DEVICE_TRANSPORT_BUILT_IN;
    }

    if (hid_text_contains(raw_device, "thunderbolt")) {
        return MPH_DEVICE_TRANSPORT_THUNDERBOLT;
    }

    return MPH_DEVICE_TRANSPORT_UNKNOWN;
}

mph_status_t mph_hid_device_id(mph_device_id_t *out_device_id,
                               const mph_hid_raw_device_t *raw_device) {
    if (out_device_id == NULL || raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_category_t category = mph_hid_infer_category(raw_device);
    if (category == MPH_DEVICE_CATEGORY_UNKNOWN) {
        return MPH_STATUS_NOT_FOUND;
    }

    char stable_value[MPH_DEVICE_ID_CAPACITY];
    int written = 0;
    if (!text_is_empty(raw_device->serial_number)) {
        written = snprintf(stable_value, sizeof(stable_value), "%s-v%u-p%u-s%s-u%u-%u",
                           mph_device_category_name(category), raw_device->vendor_id,
                           raw_device->product_id, raw_device->serial_number,
                           raw_device->usage_page, raw_device->usage);
    } else if (raw_device->vendor_id != 0 || raw_device->product_id != 0) {
        written = snprintf(stable_value, sizeof(stable_value), "%s-v%u-p%u-u%u-%u",
                           mph_device_category_name(category), raw_device->vendor_id,
                           raw_device->product_id, raw_device->usage_page, raw_device->usage);
    } else if (!text_is_empty(raw_device->product_name)) {
        written = snprintf(stable_value, sizeof(stable_value), "%s-name-%016" PRIx64 "-u%u-%u",
                           mph_device_category_name(category), fnv1a_hash(raw_device->product_name),
                           raw_device->usage_page, raw_device->usage);
    } else {
        return MPH_STATUS_NOT_FOUND;
    }

    if (written < 0 || (size_t)written >= sizeof(stable_value)) {
        char hashed_value[MPH_HID_ID_HASH_CAPACITY];
        snprintf(hashed_value, sizeof(hashed_value), "hid-%016" PRIx64,
                 fnv1a_hash(raw_device->serial_number[0] != '\0' ? raw_device->serial_number
                                                                 : raw_device->product_name));
        return mph_device_id_from_parts(out_device_id, "hid", hashed_value);
    }

    return mph_device_id_from_parts(out_device_id, "hid", stable_value);
}

mph_status_t mph_hid_map_raw_device(const mph_hid_raw_device_t *raw_device,
                                    mph_device_t *out_device) {
    if (raw_device == NULL || out_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_category_t category = mph_hid_infer_category(raw_device);
    if (category == MPH_DEVICE_CATEGORY_UNKNOWN) {
        return MPH_STATUS_NOT_FOUND;
    }

    mph_device_t device;
    mph_device_init(&device);
    mph_status_t status = mph_hid_device_id(&device.id, raw_device);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    device.category = category;
    device.transport = mph_hid_infer_transport(raw_device);
    mph_device_set_connection_state(&device, raw_device->is_connected
                                                 ? MPH_DEVICE_CONNECTION_CONNECTED
                                                 : MPH_DEVICE_CONNECTION_DISCONNECTED);

    if (!text_is_empty(raw_device->product_name)) {
        status = mph_device_set_display_name(&device, raw_device->product_name);
    } else {
        status = mph_device_set_display_name(&device, mph_device_category_name(category));
    }
    if (!mph_status_is_ok(status)) {
        return status;
    }

    if (!text_is_empty(raw_device->manufacturer)) {
        status = mph_device_set_vendor_name(&device, raw_device->manufacturer);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    if (!text_is_empty(raw_device->serial_number)) {
        status = mph_device_set_serial_number(&device, raw_device->serial_number);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    device.hid.vendor_id = raw_device->vendor_id;
    device.hid.product_id = raw_device->product_id;
    device.hid.usage_page = raw_device->usage_page;
    device.hid.usage = raw_device->usage;

    *out_device = device;
    return MPH_STATUS_OK;
}

static void append_hid_device(const void *value, void *context) {
    hid_enumeration_context_t *enumeration_context = (hid_enumeration_context_t *)context;
    if (enumeration_context == NULL || !mph_status_is_ok(enumeration_context->status)) {
        return;
    }

    IOHIDDeviceRef hid_device = (IOHIDDeviceRef)value;
    mph_hid_raw_device_t raw_device;
    mph_status_t status = fill_raw_hid_device(hid_device, &raw_device);
    if (!mph_status_is_ok(status)) {
        return;
    }

    mph_device_t device;
    status = mph_hid_map_raw_device(&raw_device, &device);
    if (status == MPH_STATUS_NOT_FOUND) {
        return;
    }
    if (!mph_status_is_ok(status)) {
        enumeration_context->status = status;
        return;
    }

    if (mph_device_list_find_by_id(enumeration_context->out_devices, &device.id) != NULL) {
        return;
    }

    enumeration_context->status = mph_device_list_append(enumeration_context->out_devices, &device);
}

mph_status_t mph_hid_enumerate_devices(mph_device_list_t *out_devices) {
    if (out_devices == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (manager == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    IOHIDManagerSetDeviceMatching(manager, NULL);
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (devices != NULL) {
        hid_enumeration_context_t context = {
            out_devices,
            MPH_STATUS_OK,
        };
        CFSetApplyFunction(devices, append_hid_device, &context);
        CFRelease(devices);
        CFRelease(manager);
        return context.status;
    }

    CFRelease(manager);
    return MPH_STATUS_OK;
}
