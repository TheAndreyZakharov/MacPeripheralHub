#include "mph_bluetooth.h"

#import <Foundation/Foundation.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#import <IOBluetooth/IOBluetooth.h>

static void copy_nsstring(NSString *value, char *buffer, size_t capacity) {
    if (buffer == NULL || capacity == 0) {
        return;
    }

    buffer[0] = '\0';
    if (value == nil) {
        return;
    }

    const char *utf8 = [value UTF8String];
    if (utf8 == NULL) {
        return;
    }

    snprintf(buffer, capacity, "%s", utf8);
}

static mph_status_t fill_raw_bluetooth_device(IOBluetoothDevice *device,
                                              mph_bluetooth_raw_device_t *out_raw_device) {
    if (device == nil || out_raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_bluetooth_raw_device_init(out_raw_device);
    copy_nsstring(device.name, out_raw_device->name, sizeof(out_raw_device->name));
    if (out_raw_device->name[0] == '\0') {
        copy_nsstring(device.nameOrAddress, out_raw_device->name, sizeof(out_raw_device->name));
    }
    copy_nsstring(device.addressString, out_raw_device->address, sizeof(out_raw_device->address));
    out_raw_device->class_of_device = (uint32_t)device.classOfDevice;
    out_raw_device->major_device_class = (uint32_t)device.deviceClassMajor;
    out_raw_device->minor_device_class = (uint32_t)device.deviceClassMinor;
    out_raw_device->major_service_class = (uint32_t)device.serviceClassMajor;
    out_raw_device->is_paired = [device isPaired];
    out_raw_device->is_connected = [device isConnected];

    return out_raw_device->address[0] != '\0' || out_raw_device->name[0] != '\0'
               ? MPH_STATUS_OK
               : MPH_STATUS_NOT_FOUND;
}

static mph_status_t append_bluetooth_device(IOBluetoothDevice *device,
                                            mph_device_list_t *out_devices) {
    mph_bluetooth_raw_device_t raw_device;
    mph_status_t status = fill_raw_bluetooth_device(device, &raw_device);
    if (status == MPH_STATUS_NOT_FOUND) {
        return MPH_STATUS_OK;
    }
    if (!mph_status_is_ok(status)) {
        return status;
    }

    return mph_bluetooth_append_mapped_device(out_devices, &raw_device);
}

mph_status_t mph_bluetooth_enumerate_devices(mph_device_list_t *out_devices) {
    if (out_devices == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    @autoreleasepool {
        NSArray *pairedDevices = [IOBluetoothDevice pairedDevices];
        for (IOBluetoothDevice *device in pairedDevices) {
            mph_status_t status = append_bluetooth_device(device, out_devices);
            if (!mph_status_is_ok(status)) {
                return status;
            }
        }

        NSArray *recentDevices = [IOBluetoothDevice recentDevices:0];
        for (IOBluetoothDevice *device in recentDevices) {
            if (![device isConnected]) {
                continue;
            }
            mph_status_t status = append_bluetooth_device(device, out_devices);
            if (!mph_status_is_ok(status)) {
                return status;
            }
        }
    }

    return MPH_STATUS_OK;
}

#pragma clang diagnostic pop
