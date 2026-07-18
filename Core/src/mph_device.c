#include "mph_device.h"

#include <stdio.h>

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

void mph_device_init(mph_device_t *device) {
    if (device == NULL) {
        return;
    }

    mph_device_id_clear(&device->id);
    device->category = MPH_DEVICE_CATEGORY_UNKNOWN;
    device->transport = MPH_DEVICE_TRANSPORT_UNKNOWN;
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

bool mph_device_category_is_audio(mph_device_category_t category) {
    return category == MPH_DEVICE_CATEGORY_AUDIO_INPUT ||
           category == MPH_DEVICE_CATEGORY_AUDIO_OUTPUT ||
           category == MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT ||
           category == MPH_DEVICE_CATEGORY_AUDIO_INTERFACE;
}
