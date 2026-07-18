#include "mph_usb.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MPH_USB_ID_HASH_CAPACITY 32
#define MPH_USB_CLASS_HOST_DEVICE "IOUSBHostDevice"
#define MPH_USB_CLASS_LEGACY_DEVICE "IOUSBDevice"
#define MPH_USB_DEVICE_CLASS_HUB 9
#define MPH_USB_INTERFACE_CLASS_AUDIO 1
#define MPH_USB_INTERFACE_CLASS_HID 3
#define MPH_USB_INTERFACE_CLASS_VIDEO 14

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

static bool usb_text_contains(const mph_usb_raw_device_t *raw_device, const char *needle) {
    return raw_device != NULL && (text_contains_folded(raw_device->product_name, needle) ||
                                  text_contains_folded(raw_device->vendor_name, needle) ||
                                  text_contains_folded(raw_device->serial_number, needle) ||
                                  text_contains_folded(raw_device->location_path, needle));
}

static bool usb_has_interface_class(const mph_usb_raw_device_t *raw_device, uint32_t class_code) {
    if (raw_device == NULL || class_code >= 32) {
        return false;
    }

    return (raw_device->interface_class_mask & (UINT32_C(1) << class_code)) != 0;
}

static bool usb_has_meaningful_product_name(const mph_usb_raw_device_t *raw_device) {
    if (raw_device == NULL || text_is_empty(raw_device->product_name)) {
        return false;
    }

    char normalized[MPH_DEVICE_TEXT_CAPACITY];
    if (mph_device_normalize_name(raw_device->product_name, normalized, sizeof(normalized)) !=
        MPH_STATUS_OK) {
        return false;
    }

    return strcmp(normalized, "usb device") != 0 && strcmp(normalized, "generic usb device") != 0 &&
           strcmp(normalized, "unknown usb device") != 0;
}

static bool usb_has_identity_metadata(const mph_usb_raw_device_t *raw_device) {
    return raw_device != NULL &&
           (raw_device->vendor_id != 0 || raw_device->product_id != 0 ||
            usb_has_meaningful_product_name(raw_device) || !text_is_empty(raw_device->vendor_name) ||
            !text_is_empty(raw_device->serial_number));
}

static uint32_t cf_u32_value(CFTypeRef value) {
    if (value == NULL || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return 0;
    }

    int64_t number = 0;
    if (!CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &number)) {
        return 0;
    }

    return number >= 0 && number <= UINT32_MAX ? (uint32_t)number : 0;
}

static uint32_t usb_u32_property(io_registry_entry_t entry, const char *key) {
    CFStringRef property_key =
        CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    if (property_key == NULL) {
        return 0;
    }

    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, property_key, kCFAllocatorDefault, 0);
    CFRelease(property_key);
    uint32_t result = cf_u32_value(value);
    if (value != NULL) {
        CFRelease(value);
    }

    return result;
}

static void usb_string_property(io_registry_entry_t entry, const char *key, char *buffer,
                                size_t capacity) {
    if (buffer == NULL || capacity == 0) {
        return;
    }

    buffer[0] = '\0';
    CFStringRef property_key =
        CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    if (property_key == NULL) {
        return;
    }

    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, property_key, kCFAllocatorDefault, 0);
    CFRelease(property_key);
    if (value == NULL) {
        return;
    }

    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        if (!CFStringGetCString((CFStringRef)value, buffer, (CFIndex)capacity,
                                kCFStringEncodingUTF8)) {
            buffer[0] = '\0';
        }
    }

    CFRelease(value);
}

static uint32_t usb_speed_to_mbps(uint32_t raw_speed) {
    switch (raw_speed) {
    case 1:
        return 12;
    case 2:
        return 480;
    case 3:
        return 5000;
    case 4:
        return 10000;
    case 5:
        return 20000;
    default:
        return raw_speed;
    }
}

static uint64_t usb_registry_id(io_registry_entry_t entry) {
    uint64_t registry_id = 0;
    return IORegistryEntryGetRegistryEntryID(entry, &registry_id) == KERN_SUCCESS ? registry_id : 0;
}

static uint64_t usb_parent_registry_id(io_registry_entry_t entry) {
    io_registry_entry_t parent = IO_OBJECT_NULL;
    if (IORegistryEntryGetParentEntry(entry, kIOServicePlane, &parent) != KERN_SUCCESS) {
        return 0;
    }

    uint64_t registry_id = usb_registry_id(parent);
    IOObjectRelease(parent);
    return registry_id;
}

static uint32_t usb_registry_depth(io_registry_entry_t entry) {
    uint32_t depth = 0;
    io_registry_entry_t current = entry;
    while (current != IO_OBJECT_NULL) {
        io_registry_entry_t parent = IO_OBJECT_NULL;
        kern_return_t result = IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent);
        if (current != entry) {
            IOObjectRelease(current);
        }
        if (result != KERN_SUCCESS || parent == IO_OBJECT_NULL) {
            break;
        }

        depth += 1;
        current = parent;
    }

    return depth;
}

static void apply_child_interface_metadata(io_registry_entry_t service,
                                           mph_usb_raw_device_t *raw_device) {
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IORegistryEntryGetChildIterator(service, kIOServicePlane, &iterator) != KERN_SUCCESS) {
        return;
    }

    io_registry_entry_t child = IO_OBJECT_NULL;
    while ((child = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        uint32_t interface_class = usb_u32_property(child, "bInterfaceClass");
        if (interface_class < 32) {
            raw_device->interface_class_mask |= UINT32_C(1) << interface_class;
        }
        IOObjectRelease(child);
    }

    IOObjectRelease(iterator);
}

static mph_status_t fill_raw_usb_device(io_registry_entry_t service,
                                        mph_usb_raw_device_t *out_raw_device) {
    if (service == IO_OBJECT_NULL || out_raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_usb_raw_device_init(out_raw_device);
    out_raw_device->vendor_id = usb_u32_property(service, "idVendor");
    out_raw_device->product_id = usb_u32_property(service, "idProduct");
    out_raw_device->device_class = usb_u32_property(service, "bDeviceClass");
    out_raw_device->device_subclass = usb_u32_property(service, "bDeviceSubClass");
    out_raw_device->device_protocol = usb_u32_property(service, "bDeviceProtocol");
    out_raw_device->speed_mbps = usb_speed_to_mbps(usb_u32_property(service, "Device Speed"));
    out_raw_device->power_ma = usb_u32_property(service, "Bus Power Available");
    out_raw_device->location_id = usb_u32_property(service, "locationID");
    out_raw_device->registry_id = usb_registry_id(service);
    out_raw_device->parent_registry_id = usb_parent_registry_id(service);
    out_raw_device->depth = usb_registry_depth(service);
    usb_string_property(service, "USB Product Name", out_raw_device->product_name,
                        sizeof(out_raw_device->product_name));
    usb_string_property(service, "USB Vendor Name", out_raw_device->vendor_name,
                        sizeof(out_raw_device->vendor_name));
    usb_string_property(service, "USB Serial Number", out_raw_device->serial_number,
                        sizeof(out_raw_device->serial_number));
    io_string_t registry_path;
    if (IORegistryEntryGetPath(service, kIOServicePlane, registry_path) == KERN_SUCCESS) {
        snprintf(out_raw_device->location_path, sizeof(out_raw_device->location_path), "%s",
                 registry_path);
    }
    apply_child_interface_metadata(service, out_raw_device);
    out_raw_device->is_connected = true;
    return MPH_STATUS_OK;
}

void mph_usb_raw_device_init(mph_usb_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return;
    }

    raw_device->vendor_id = 0;
    raw_device->product_id = 0;
    raw_device->device_class = 0;
    raw_device->device_subclass = 0;
    raw_device->device_protocol = 0;
    raw_device->interface_class_mask = 0;
    raw_device->speed_mbps = 0;
    raw_device->power_ma = 0;
    raw_device->location_id = 0;
    raw_device->registry_id = 0;
    raw_device->parent_registry_id = 0;
    raw_device->depth = 0;
    raw_device->product_name[0] = '\0';
    raw_device->vendor_name[0] = '\0';
    raw_device->serial_number[0] = '\0';
    raw_device->location_path[0] = '\0';
    raw_device->is_connected = false;
}

mph_device_category_t mph_usb_infer_category(const mph_usb_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return MPH_DEVICE_CATEGORY_UNKNOWN;
    }

    if (raw_device->device_class == MPH_USB_DEVICE_CLASS_HUB ||
        usb_text_contains(raw_device, "hub")) {
        return MPH_DEVICE_CATEGORY_HUB;
    }

    if (usb_text_contains(raw_device, "dock") || usb_text_contains(raw_device, "docking station")) {
        return MPH_DEVICE_CATEGORY_DOCK;
    }

    if (usb_has_interface_class(raw_device, MPH_USB_INTERFACE_CLASS_VIDEO) ||
        usb_text_contains(raw_device, "camera") || usb_text_contains(raw_device, "webcam") ||
        usb_text_contains(raw_device, "uvc") || usb_text_contains(raw_device, "cam link")) {
        return MPH_DEVICE_CATEGORY_CAMERA;
    }

    if (usb_has_interface_class(raw_device, MPH_USB_INTERFACE_CLASS_AUDIO) ||
        usb_text_contains(raw_device, "audio") || usb_text_contains(raw_device, "sound") ||
        usb_text_contains(raw_device, "microphone") || usb_text_contains(raw_device, "speaker") ||
        usb_text_contains(raw_device, "scarlett")) {
        return MPH_DEVICE_CATEGORY_AUDIO_INTERFACE;
    }

    if (usb_text_contains(raw_device, "trackpad") || usb_text_contains(raw_device, "touchpad")) {
        return MPH_DEVICE_CATEGORY_TRACKPAD;
    }

    if (usb_text_contains(raw_device, "keyboard")) {
        return MPH_DEVICE_CATEGORY_KEYBOARD;
    }

    if (usb_text_contains(raw_device, "mouse")) {
        return MPH_DEVICE_CATEGORY_MOUSE;
    }

    if (usb_has_interface_class(raw_device, MPH_USB_INTERFACE_CLASS_HID)) {
        return MPH_DEVICE_CATEGORY_USB;
    }

    return usb_has_identity_metadata(raw_device) ? MPH_DEVICE_CATEGORY_USB
                                                 : MPH_DEVICE_CATEGORY_UNKNOWN;
}

mph_status_t mph_usb_device_id(mph_device_id_t *out_device_id,
                               const mph_usb_raw_device_t *raw_device) {
    if (out_device_id == NULL || raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_category_t category = mph_usb_infer_category(raw_device);
    char stable_value[MPH_DEVICE_ID_CAPACITY];
    int written = 0;
    if (!text_is_empty(raw_device->serial_number)) {
        written = snprintf(stable_value, sizeof(stable_value), "%s-v%u-p%u-s%s",
                           mph_device_category_name(category), raw_device->vendor_id,
                           raw_device->product_id, raw_device->serial_number);
    } else if (raw_device->vendor_id != 0 || raw_device->product_id != 0) {
        written =
            snprintf(stable_value, sizeof(stable_value), "%s-v%u-p%u-l%u-r%" PRIu64,
                     mph_device_category_name(category), raw_device->vendor_id,
                     raw_device->product_id, raw_device->location_id, raw_device->registry_id);
    } else if (raw_device->registry_id != 0) {
        written = snprintf(stable_value, sizeof(stable_value), "%s-r%" PRIu64,
                           mph_device_category_name(category), raw_device->registry_id);
    } else if (!text_is_empty(raw_device->product_name)) {
        written =
            snprintf(stable_value, sizeof(stable_value), "%s-name-%016" PRIx64,
                     mph_device_category_name(category), fnv1a_hash(raw_device->product_name));
    } else {
        return MPH_STATUS_NOT_FOUND;
    }

    if (written < 0 || (size_t)written >= sizeof(stable_value)) {
        char hashed_value[MPH_USB_ID_HASH_CAPACITY];
        const char *hash_source = raw_device->serial_number[0] != '\0' ? raw_device->serial_number
                                                                       : raw_device->product_name;
        snprintf(hashed_value, sizeof(hashed_value), "usb-%016" PRIx64, fnv1a_hash(hash_source));
        return mph_device_id_from_parts(out_device_id, "usb", hashed_value);
    }

    return mph_device_id_from_parts(out_device_id, "usb", stable_value);
}

bool mph_usb_matches_device(const mph_usb_raw_device_t *raw_device, const mph_device_t *device) {
    if (raw_device == NULL || device == NULL) {
        return false;
    }

    if (!text_is_empty(raw_device->serial_number) && !text_is_empty(device->serial_number) &&
        strcmp(raw_device->serial_number, device->serial_number) == 0) {
        return true;
    }

    bool same_usb_ids = raw_device->vendor_id != 0 && raw_device->product_id != 0 &&
                        ((device->usb.vendor_id == raw_device->vendor_id &&
                          device->usb.product_id == raw_device->product_id) ||
                         (device->hid.vendor_id == raw_device->vendor_id &&
                          device->hid.product_id == raw_device->product_id));
    if (same_usb_ids) {
        if (text_is_empty(raw_device->serial_number) || text_is_empty(device->serial_number)) {
            return true;
        }
        return strcmp(raw_device->serial_number, device->serial_number) == 0;
    }

    bool linkable_category = mph_device_category_is_audio(device->category) ||
                             device->category == MPH_DEVICE_CATEGORY_AUDIO_INTERFACE ||
                             device->category == MPH_DEVICE_CATEGORY_CAMERA ||
                             device->category == MPH_DEVICE_CATEGORY_KEYBOARD ||
                             device->category == MPH_DEVICE_CATEGORY_MOUSE ||
                             device->category == MPH_DEVICE_CATEGORY_TRACKPAD ||
                             device->category == MPH_DEVICE_CATEGORY_USB;
    return device->transport == MPH_DEVICE_TRANSPORT_USB && linkable_category &&
           normalized_text_equal(raw_device->product_name, device->display_name);
}

mph_status_t mph_usb_map_raw_device(const mph_usb_raw_device_t *raw_device,
                                    mph_device_t *out_device) {
    if (raw_device == NULL || out_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_t device;
    mph_device_init(&device);
    mph_status_t status = mph_usb_device_id(&device.id, raw_device);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    device.category = mph_usb_infer_category(raw_device);
    device.transport = MPH_DEVICE_TRANSPORT_USB;
    mph_device_set_connection_state(&device, raw_device->is_connected
                                                 ? MPH_DEVICE_CONNECTION_CONNECTED
                                                 : MPH_DEVICE_CONNECTION_DISCONNECTED);

    const char *display_name = raw_device->product_name;
    if (text_is_empty(display_name)) {
        display_name =
            device.category == MPH_DEVICE_CATEGORY_UNKNOWN ? "Unknown USB Device" : "USB Device";
    }
    status = mph_device_set_display_name(&device, display_name);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    if (!text_is_empty(raw_device->vendor_name)) {
        status = mph_device_set_vendor_name(&device, raw_device->vendor_name);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    if (!text_is_empty(raw_device->product_name)) {
        status = mph_device_set_model_name(&device, raw_device->product_name);
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

    device.usb.vendor_id = raw_device->vendor_id;
    device.usb.product_id = raw_device->product_id;
    device.usb.speed_mbps = raw_device->speed_mbps;
    device.usb.power_ma = raw_device->power_ma;

    *out_device = device;
    return MPH_STATUS_OK;
}

mph_status_t mph_usb_append_mapped_device(mph_device_list_t *out_devices,
                                          const mph_usb_raw_device_t *raw_device) {
    if (out_devices == NULL || raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_t device;
    mph_status_t status = mph_usb_map_raw_device(raw_device, &device);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    if (mph_device_list_find_by_id(out_devices, &device.id) != NULL) {
        return MPH_STATUS_OK;
    }

    for (size_t index = 0; index < mph_device_list_count(out_devices); index += 1) {
        const mph_device_t *existing = mph_device_list_get(out_devices, index);
        if (mph_usb_matches_device(raw_device, existing)) {
            return MPH_STATUS_OK;
        }
    }

    return mph_device_list_append(out_devices, &device);
}

static mph_status_t enumerate_usb_class(const char *class_name, mph_device_list_t *out_devices) {
    CFMutableDictionaryRef matching = IOServiceMatching(class_name);
    if (matching == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t result = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
    if (result != KERN_SUCCESS) {
        return MPH_STATUS_INTERNAL_ERROR;
    }

    mph_status_t status = MPH_STATUS_OK;
    io_registry_entry_t service = IO_OBJECT_NULL;
    while (mph_status_is_ok(status) && (service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        mph_usb_raw_device_t raw_device;
        status = fill_raw_usb_device(service, &raw_device);
        if (mph_status_is_ok(status)) {
            status = mph_usb_append_mapped_device(out_devices, &raw_device);
        }
        IOObjectRelease(service);
    }

    IOObjectRelease(iterator);
    return status;
}

mph_status_t mph_usb_enumerate_devices(mph_device_list_t *out_devices) {
    if (out_devices == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = enumerate_usb_class(MPH_USB_CLASS_HOST_DEVICE, out_devices);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    return enumerate_usb_class(MPH_USB_CLASS_LEGACY_DEVICE, out_devices);
}
