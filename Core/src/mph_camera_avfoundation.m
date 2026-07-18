#include "mph_camera.h"

#include "mph_log.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include <IOKit/audio/IOAudioTypes.h>

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

static mph_device_transport_t camera_transport_from_avfoundation(int32_t transport_type) {
    switch (transport_type) {
    case kIOAudioDeviceTransportTypeBuiltIn:
        return MPH_DEVICE_TRANSPORT_BUILT_IN;
    case kIOAudioDeviceTransportTypeUSB:
        return MPH_DEVICE_TRANSPORT_USB;
    case kIOAudioDeviceTransportTypeBluetooth:
    case kIOAudioDeviceTransportTypeWireless:
        return MPH_DEVICE_TRANSPORT_BLUETOOTH;
    case kIOAudioDeviceTransportTypeVirtual:
        return MPH_DEVICE_TRANSPORT_VIRTUAL;
    case kIOAudioDeviceTransportTypeDisplayPort:
        return MPH_DEVICE_TRANSPORT_DISPLAY_PORT;
    case kIOAudioDeviceTransportTypeHdmi:
        return MPH_DEVICE_TRANSPORT_HDMI;
    case kIOAudioDeviceTransportTypeThunderbolt:
        return MPH_DEVICE_TRANSPORT_THUNDERBOLT;
    default:
        return MPH_DEVICE_TRANSPORT_UNKNOWN;
    }
}

static NSArray<AVCaptureDeviceType> *camera_discovery_device_types(void) {
    NSMutableArray<AVCaptureDeviceType> *deviceTypes = [NSMutableArray array];
    [deviceTypes addObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
    [deviceTypes addObject:AVCaptureDeviceTypeDeskViewCamera];

    if (@available(macOS 14.0, *)) {
        [deviceTypes addObject:AVCaptureDeviceTypeExternal];
        [deviceTypes addObject:AVCaptureDeviceTypeContinuityCamera];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [deviceTypes addObject:AVCaptureDeviceTypeExternalUnknown];
#pragma clang diagnostic pop
    }

    return deviceTypes;
}

static mph_status_t fill_raw_camera(AVCaptureDevice *camera,
                                    mph_camera_raw_device_t *out_raw_device) {
    if (camera == nil || out_raw_device == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_camera_raw_device_init(out_raw_device);
    copy_nsstring(camera.uniqueID, out_raw_device->unique_id, sizeof(out_raw_device->unique_id));
    if (out_raw_device->unique_id[0] == '\0') {
        return MPH_STATUS_NOT_FOUND;
    }

    copy_nsstring(camera.localizedName, out_raw_device->localized_name,
                  sizeof(out_raw_device->localized_name));
    copy_nsstring(camera.manufacturer, out_raw_device->manufacturer,
                  sizeof(out_raw_device->manufacturer));
    copy_nsstring(camera.deviceType, out_raw_device->device_type,
                  sizeof(out_raw_device->device_type));

    out_raw_device->transport = camera_transport_from_avfoundation(camera.transportType);
    out_raw_device->is_connected = true;
    return MPH_STATUS_OK;
}

mph_status_t mph_camera_enumerate_devices(mph_device_list_t *out_devices) {
    if (out_devices == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    @autoreleasepool {
        AVCaptureDeviceDiscoverySession *session = [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:camera_discovery_device_types()
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];
        if (session == nil) {
            mph_log_message(MPH_LOG_LEVEL_ERROR, "camera",
                            "AVCaptureDeviceDiscoverySession creation failed");
            return MPH_STATUS_INTERNAL_ERROR;
        }

        for (AVCaptureDevice *camera in session.devices) {
            mph_camera_raw_device_t raw_device;
            mph_status_t status = fill_raw_camera(camera, &raw_device);
            if (!mph_status_is_ok(status)) {
                continue;
            }

            mph_device_t device;
            status = mph_camera_map_raw_device(&raw_device, &device);
            if (!mph_status_is_ok(status)) {
                return status;
            }

            status = mph_device_list_append(out_devices, &device);
            if (!mph_status_is_ok(status)) {
                return status;
            }
        }
    }

    return MPH_STATUS_OK;
}
