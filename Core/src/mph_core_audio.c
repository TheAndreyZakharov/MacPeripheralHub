#include "mph_core_audio.h"

#include "mph_log.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MPH_CORE_AUDIO_ID_HASH_CAPACITY 32

static const char *role_namespace(mph_core_audio_role_t role) {
    switch (role) {
    case MPH_CORE_AUDIO_ROLE_INPUT:
        return "coreaudio.input";
    case MPH_CORE_AUDIO_ROLE_OUTPUT:
        return "coreaudio.output";
    case MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT:
        return "coreaudio.system_output";
    }

    return "coreaudio.unknown";
}

static mph_device_category_t role_category(mph_core_audio_role_t role) {
    switch (role) {
    case MPH_CORE_AUDIO_ROLE_INPUT:
        return MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    case MPH_CORE_AUDIO_ROLE_OUTPUT:
        return MPH_DEVICE_CATEGORY_AUDIO_OUTPUT;
    case MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT:
        return MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT;
    }

    return MPH_DEVICE_CATEGORY_UNKNOWN;
}

static bool raw_device_has_role(const mph_core_audio_raw_device_t *raw_device,
                                mph_core_audio_role_t role) {
    if (raw_device == NULL) {
        return false;
    }

    switch (role) {
    case MPH_CORE_AUDIO_ROLE_INPUT:
        return raw_device->input_channel_count > 0;
    case MPH_CORE_AUDIO_ROLE_OUTPUT:
    case MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT:
        return raw_device->output_channel_count > 0;
    }

    return false;
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

static void log_core_audio_error(const char *operation, OSStatus status) {
    char message[MPH_ERROR_MESSAGE_CAPACITY];
    snprintf(message, sizeof(message), "%s failed with OSStatus %d", operation, (int)status);
    mph_log_message(MPH_LOG_LEVEL_ERROR, "core_audio", message);
}

static bool audio_object_has_property(AudioObjectID object_id, AudioObjectPropertySelector selector,
                                      AudioObjectPropertyScope scope) {
    AudioObjectPropertyAddress address = {
        selector,
        scope,
        kAudioObjectPropertyElementMain,
    };
    return AudioObjectHasProperty(object_id, &address);
}

static OSStatus audio_object_get_data_size(AudioObjectID object_id,
                                           AudioObjectPropertySelector selector,
                                           AudioObjectPropertyScope scope, UInt32 *out_size) {
    AudioObjectPropertyAddress address = {
        selector,
        scope,
        kAudioObjectPropertyElementMain,
    };
    return AudioObjectGetPropertyDataSize(object_id, &address, 0, NULL, out_size);
}

static OSStatus audio_object_get_data(AudioObjectID object_id, AudioObjectPropertySelector selector,
                                      AudioObjectPropertyScope scope, UInt32 *io_size,
                                      void *out_data) {
    AudioObjectPropertyAddress address = {
        selector,
        scope,
        kAudioObjectPropertyElementMain,
    };
    return AudioObjectGetPropertyData(object_id, &address, 0, NULL, io_size, out_data);
}

static OSStatus audio_object_set_data(AudioObjectID object_id, AudioObjectPropertySelector selector,
                                      AudioObjectPropertyScope scope, UInt32 size,
                                      const void *data) {
    AudioObjectPropertyAddress address = {
        selector,
        scope,
        kAudioObjectPropertyElementMain,
    };
    return AudioObjectSetPropertyData(object_id, &address, 0, NULL, size, data);
}

static mph_status_t copy_core_audio_string(AudioObjectID object_id,
                                           AudioObjectPropertySelector selector, char *buffer,
                                           size_t capacity) {
    if (buffer == NULL || capacity == 0) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }
    buffer[0] = '\0';

    if (!audio_object_has_property(object_id, selector, kAudioObjectPropertyScopeGlobal)) {
        return MPH_STATUS_NOT_FOUND;
    }

    CFStringRef value = NULL;
    UInt32 size = sizeof(value);
    OSStatus status =
        audio_object_get_data(object_id, selector, kAudioObjectPropertyScopeGlobal, &size, &value);
    if (status != noErr || value == NULL) {
        return MPH_STATUS_NOT_FOUND;
    }

    bool copied = CFStringGetCString(value, buffer, (CFIndex)capacity, kCFStringEncodingUTF8);
    CFRelease(value);
    if (!copied) {
        buffer[0] = '\0';
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return MPH_STATUS_OK;
}

static double audio_device_sample_rate(AudioObjectID device_id) {
    Float64 sample_rate = 0.0;
    UInt32 size = sizeof(sample_rate);
    OSStatus status = audio_object_get_data(device_id, kAudioDevicePropertyNominalSampleRate,
                                            kAudioObjectPropertyScopeGlobal, &size, &sample_rate);
    return status == noErr ? (double)sample_rate : 0.0;
}

static bool audio_device_is_alive(AudioObjectID device_id) {
    UInt32 is_alive = 0;
    UInt32 size = sizeof(is_alive);
    OSStatus status = audio_object_get_data(device_id, kAudioDevicePropertyDeviceIsAlive,
                                            kAudioObjectPropertyScopeGlobal, &size, &is_alive);
    return status == noErr && is_alive != 0;
}

static uint32_t audio_device_channel_count(AudioObjectID device_id,
                                           AudioObjectPropertyScope scope) {
    if (!audio_object_has_property(device_id, kAudioDevicePropertyStreamConfiguration, scope)) {
        return 0;
    }

    UInt32 size = 0;
    OSStatus status = audio_object_get_data_size(device_id, kAudioDevicePropertyStreamConfiguration,
                                                 scope, &size);
    if (status != noErr || size == 0) {
        return 0;
    }

    AudioBufferList *buffer_list = (AudioBufferList *)calloc(1, size);
    if (buffer_list == NULL) {
        return 0;
    }

    status = audio_object_get_data(device_id, kAudioDevicePropertyStreamConfiguration, scope, &size,
                                   buffer_list);
    if (status != noErr) {
        free(buffer_list);
        return 0;
    }

    uint32_t channel_count = 0;
    for (UInt32 index = 0; index < buffer_list->mNumberBuffers; index += 1) {
        channel_count += buffer_list->mBuffers[index].mNumberChannels;
    }

    free(buffer_list);
    return channel_count;
}

static mph_device_transport_t map_core_audio_transport(UInt32 transport_type) {
    switch (transport_type) {
    case kAudioDeviceTransportTypeBuiltIn:
        return MPH_DEVICE_TRANSPORT_BUILT_IN;
    case kAudioDeviceTransportTypeAggregate:
        return MPH_DEVICE_TRANSPORT_AGGREGATE;
    case kAudioDeviceTransportTypeVirtual:
        return MPH_DEVICE_TRANSPORT_VIRTUAL;
    case kAudioDeviceTransportTypeUSB:
        return MPH_DEVICE_TRANSPORT_USB;
    case kAudioDeviceTransportTypeBluetooth:
    case kAudioDeviceTransportTypeBluetoothLE:
        return MPH_DEVICE_TRANSPORT_BLUETOOTH;
    case kAudioDeviceTransportTypeHDMI:
        return MPH_DEVICE_TRANSPORT_HDMI;
    case kAudioDeviceTransportTypeDisplayPort:
        return MPH_DEVICE_TRANSPORT_DISPLAY_PORT;
    case kAudioDeviceTransportTypeThunderbolt:
        return MPH_DEVICE_TRANSPORT_THUNDERBOLT;
    default:
        return MPH_DEVICE_TRANSPORT_UNKNOWN;
    }
}

static mph_device_transport_t audio_device_transport(AudioObjectID device_id) {
    UInt32 transport_type = 0;
    UInt32 size = sizeof(transport_type);
    OSStatus status =
        audio_object_get_data(device_id, kAudioDevicePropertyTransportType,
                              kAudioObjectPropertyScopeGlobal, &size, &transport_type);
    return status == noErr ? map_core_audio_transport(transport_type)
                           : MPH_DEVICE_TRANSPORT_UNKNOWN;
}

static AudioObjectID default_audio_device(AudioObjectPropertySelector selector) {
    AudioObjectID device_id = kAudioObjectUnknown;
    UInt32 size = sizeof(device_id);
    OSStatus status = audio_object_get_data(kAudioObjectSystemObject, selector,
                                            kAudioObjectPropertyScopeGlobal, &size, &device_id);
    return status == noErr ? device_id : kAudioObjectUnknown;
}

static bool audio_device_id_equal(AudioObjectID left, AudioObjectID right) {
    return left != kAudioObjectUnknown && right != kAudioObjectUnknown && left == right;
}

static bool text_has_prefix(const char *value, const char *prefix) {
    return value != NULL && prefix != NULL && strncmp(value, prefix, strlen(prefix)) == 0;
}

static bool raw_device_is_temporary_default_aggregate(
    const mph_core_audio_raw_device_t *raw_device) {
    return raw_device != NULL &&
           (text_has_prefix(raw_device->uid, "CADefaultDeviceAggregate-") ||
            text_has_prefix(raw_device->name, "CADefaultDeviceAggregate-"));
}

static mph_status_t fill_raw_device(AudioObjectID device_id, AudioObjectID default_input,
                                    AudioObjectID default_output, AudioObjectID default_system,
                                    mph_core_audio_raw_device_t *out_raw) {
    if (out_raw == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_core_audio_raw_device_init(out_raw);
    mph_status_t status = copy_core_audio_string(device_id, kAudioDevicePropertyDeviceUID,
                                                 out_raw->uid, sizeof(out_raw->uid));
    if (!mph_status_is_ok(status) || out_raw->uid[0] == '\0') {
        return MPH_STATUS_NOT_FOUND;
    }

    (void)copy_core_audio_string(device_id, kAudioObjectPropertyName, out_raw->name,
                                 sizeof(out_raw->name));
    (void)copy_core_audio_string(device_id, kAudioObjectPropertyManufacturer, out_raw->manufacturer,
                                 sizeof(out_raw->manufacturer));
    out_raw->transport = audio_device_transport(device_id);
    out_raw->sample_rate_hz = audio_device_sample_rate(device_id);
    out_raw->input_channel_count =
        audio_device_channel_count(device_id, kAudioDevicePropertyScopeInput);
    out_raw->output_channel_count =
        audio_device_channel_count(device_id, kAudioDevicePropertyScopeOutput);
    out_raw->is_alive = audio_device_is_alive(device_id);
    out_raw->is_default_input = audio_device_id_equal(device_id, default_input);
    out_raw->is_default_output = audio_device_id_equal(device_id, default_output);
    out_raw->is_default_system_output = audio_device_id_equal(device_id, default_system);
    return MPH_STATUS_OK;
}

static mph_status_t append_role_device(const mph_core_audio_raw_device_t *raw_device,
                                       mph_core_audio_role_t role, mph_device_list_t *out_devices) {
    if (raw_device == NULL || out_devices == NULL || !raw_device_has_role(raw_device, role)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_t device;
    mph_device_init(&device);
    mph_status_t status = mph_core_audio_role_device_id(&device.id, role, raw_device->uid);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    device.category = role_category(role);
    device.transport = raw_device->transport;
    mph_device_set_connection_state(&device, raw_device->is_alive
                                                 ? MPH_DEVICE_CONNECTION_CONNECTED
                                                 : MPH_DEVICE_CONNECTION_UNAVAILABLE);
    if (raw_device->name[0] != '\0') {
        status = mph_device_set_display_name(&device, raw_device->name);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }
    if (raw_device->manufacturer[0] != '\0') {
        status = mph_device_set_vendor_name(&device, raw_device->manufacturer);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    device.audio.sample_rate_hz = raw_device->sample_rate_hz;
    device.audio.channel_count = role == MPH_CORE_AUDIO_ROLE_INPUT
                                     ? raw_device->input_channel_count
                                     : raw_device->output_channel_count;
    device.audio.is_default_input =
        role == MPH_CORE_AUDIO_ROLE_INPUT && raw_device->is_default_input;
    device.audio.is_default_output =
        role == MPH_CORE_AUDIO_ROLE_OUTPUT && raw_device->is_default_output;
    device.audio.is_default_system_output =
        role == MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT && raw_device->is_default_system_output;

    return mph_device_list_append(out_devices, &device);
}

static mph_status_t get_default_role_device(AudioObjectPropertySelector selector,
                                            mph_core_audio_role_t role,
                                            mph_device_id_t *out_device_id, bool *out_found) {
    if (out_device_id == NULL || out_found == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_device_id_clear(out_device_id);
    *out_found = false;

    AudioObjectID device_id = default_audio_device(selector);
    if (device_id == kAudioObjectUnknown) {
        return MPH_STATUS_OK;
    }

    char uid[MPH_CORE_AUDIO_TEXT_CAPACITY];
    mph_status_t status =
        copy_core_audio_string(device_id, kAudioDevicePropertyDeviceUID, uid, sizeof(uid));
    if (!mph_status_is_ok(status)) {
        return MPH_STATUS_OK;
    }

    status = mph_core_audio_role_device_id(out_device_id, role, uid);
    if (mph_status_is_ok(status)) {
        *out_found = true;
    }
    return status;
}

static bool device_id_matches_uid(const mph_device_id_t *target_id, mph_core_audio_role_t role,
                                  const char *uid) {
    if (target_id == NULL || uid == NULL || uid[0] == '\0') {
        return false;
    }

    mph_device_id_t role_id;
    if (mph_status_is_ok(mph_core_audio_role_device_id(&role_id, role, uid)) &&
        mph_device_id_equal(target_id, &role_id)) {
        return true;
    }

    return strcmp(mph_device_id_cstr(target_id), uid) == 0;
}

static mph_status_t resolve_audio_object_for_role(const mph_device_id_t *target_id,
                                                  mph_core_audio_role_t role,
                                                  AudioObjectID *out_audio_object_id) {
    if (mph_device_id_is_empty(target_id) || out_audio_object_id == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    *out_audio_object_id = kAudioObjectUnknown;
    UInt32 size = 0;
    OSStatus status =
        audio_object_get_data_size(kAudioObjectSystemObject, kAudioHardwarePropertyDevices,
                                   kAudioObjectPropertyScopeGlobal, &size);
    if (status != noErr) {
        log_core_audio_error("AudioObjectGetPropertyDataSize(devices)", status);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    if (size == 0) {
        return MPH_STATUS_NOT_FOUND;
    }

    AudioObjectID *device_ids = (AudioObjectID *)calloc(1, size);
    if (device_ids == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    status = audio_object_get_data(kAudioObjectSystemObject, kAudioHardwarePropertyDevices,
                                   kAudioObjectPropertyScopeGlobal, &size, device_ids);
    if (status != noErr) {
        free(device_ids);
        log_core_audio_error("AudioObjectGetPropertyData(devices)", status);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    size_t device_count = size / sizeof(AudioObjectID);
    for (size_t index = 0; index < device_count; index += 1) {
        mph_core_audio_raw_device_t raw_device;
        if (!mph_status_is_ok(fill_raw_device(device_ids[index], kAudioObjectUnknown,
                                              kAudioObjectUnknown, kAudioObjectUnknown,
                                              &raw_device))) {
            continue;
        }

        if (raw_device_has_role(&raw_device, role) &&
            device_id_matches_uid(target_id, role, raw_device.uid)) {
            *out_audio_object_id = device_ids[index];
            free(device_ids);
            return MPH_STATUS_OK;
        }
    }

    free(device_ids);
    return MPH_STATUS_NOT_FOUND;
}

static mph_status_t set_default_role_device(const mph_device_id_t *device_id,
                                            mph_core_audio_role_t role,
                                            AudioObjectPropertySelector selector) {
    AudioObjectID audio_object_id = kAudioObjectUnknown;
    mph_status_t status = resolve_audio_object_for_role(device_id, role, &audio_object_id);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    OSStatus os_status =
        audio_object_set_data(kAudioObjectSystemObject, selector, kAudioObjectPropertyScopeGlobal,
                              sizeof(audio_object_id), &audio_object_id);
    if (os_status != noErr) {
        log_core_audio_error("AudioObjectSetPropertyData(default device)", os_status);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    return MPH_STATUS_OK;
}

void mph_core_audio_raw_device_init(mph_core_audio_raw_device_t *raw_device) {
    if (raw_device == NULL) {
        return;
    }

    raw_device->uid[0] = '\0';
    raw_device->name[0] = '\0';
    raw_device->manufacturer[0] = '\0';
    raw_device->transport = MPH_DEVICE_TRANSPORT_UNKNOWN;
    raw_device->sample_rate_hz = 0.0;
    raw_device->input_channel_count = 0;
    raw_device->output_channel_count = 0;
    raw_device->is_alive = false;
    raw_device->is_default_input = false;
    raw_device->is_default_output = false;
    raw_device->is_default_system_output = false;
}

mph_status_t mph_core_audio_role_device_id(mph_device_id_t *out_device_id,
                                           mph_core_audio_role_t role, const char *core_audio_uid) {
    if (out_device_id == NULL || core_audio_uid == NULL || core_audio_uid[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status =
        mph_device_id_from_parts(out_device_id, role_namespace(role), core_audio_uid);
    if (status != MPH_STATUS_CAPACITY_EXCEEDED) {
        return status;
    }

    char hashed_uid[MPH_CORE_AUDIO_ID_HASH_CAPACITY];
    snprintf(hashed_uid, sizeof(hashed_uid), "uid-%016" PRIx64, fnv1a_hash(core_audio_uid));
    return mph_device_id_from_parts(out_device_id, role_namespace(role), hashed_uid);
}

mph_status_t mph_core_audio_map_raw_device(const mph_core_audio_raw_device_t *raw_device,
                                           mph_device_list_t *out_devices) {
    if (raw_device == NULL || out_devices == NULL || raw_device->uid[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    if (raw_device_is_temporary_default_aggregate(raw_device)) {
        return MPH_STATUS_NOT_FOUND;
    }

    bool mapped = false;
    if (raw_device_has_role(raw_device, MPH_CORE_AUDIO_ROLE_INPUT)) {
        mph_status_t status =
            append_role_device(raw_device, MPH_CORE_AUDIO_ROLE_INPUT, out_devices);
        if (!mph_status_is_ok(status)) {
            return status;
        }
        mapped = true;
    }

    if (raw_device_has_role(raw_device, MPH_CORE_AUDIO_ROLE_OUTPUT)) {
        mph_status_t status =
            append_role_device(raw_device, MPH_CORE_AUDIO_ROLE_OUTPUT, out_devices);
        if (!mph_status_is_ok(status)) {
            return status;
        }
        mapped = true;
    }

    if (raw_device_has_role(raw_device, MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT)) {
        mph_status_t status =
            append_role_device(raw_device, MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT, out_devices);
        if (!mph_status_is_ok(status)) {
            return status;
        }
        mapped = true;
    }

    return mapped ? MPH_STATUS_OK : MPH_STATUS_NOT_FOUND;
}

mph_status_t mph_core_audio_enumerate_devices(mph_device_list_t *out_devices) {
    if (out_devices == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    UInt32 size = 0;
    OSStatus status =
        audio_object_get_data_size(kAudioObjectSystemObject, kAudioHardwarePropertyDevices,
                                   kAudioObjectPropertyScopeGlobal, &size);
    if (status != noErr) {
        log_core_audio_error("AudioObjectGetPropertyDataSize(devices)", status);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    if (size == 0) {
        return MPH_STATUS_OK;
    }

    AudioObjectID *device_ids = (AudioObjectID *)calloc(1, size);
    if (device_ids == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    status = audio_object_get_data(kAudioObjectSystemObject, kAudioHardwarePropertyDevices,
                                   kAudioObjectPropertyScopeGlobal, &size, device_ids);
    if (status != noErr) {
        free(device_ids);
        log_core_audio_error("AudioObjectGetPropertyData(devices)", status);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    AudioObjectID default_input = default_audio_device(kAudioHardwarePropertyDefaultInputDevice);
    AudioObjectID default_output = default_audio_device(kAudioHardwarePropertyDefaultOutputDevice);
    AudioObjectID default_system =
        default_audio_device(kAudioHardwarePropertyDefaultSystemOutputDevice);

    size_t device_count = size / sizeof(AudioObjectID);
    for (size_t index = 0; index < device_count; index += 1) {
        mph_core_audio_raw_device_t raw_device;
        if (!mph_status_is_ok(fill_raw_device(device_ids[index], default_input, default_output,
                                              default_system, &raw_device))) {
            continue;
        }

        mph_status_t map_status = mph_core_audio_map_raw_device(&raw_device, out_devices);
        if (map_status != MPH_STATUS_OK && map_status != MPH_STATUS_NOT_FOUND) {
            free(device_ids);
            return map_status;
        }
    }

    free(device_ids);
    return MPH_STATUS_OK;
}

mph_status_t mph_core_audio_get_default_input(mph_device_id_t *out_device_id, bool *out_found) {
    return get_default_role_device(kAudioHardwarePropertyDefaultInputDevice,
                                   MPH_CORE_AUDIO_ROLE_INPUT, out_device_id, out_found);
}

mph_status_t mph_core_audio_get_default_output(mph_device_id_t *out_device_id, bool *out_found) {
    return get_default_role_device(kAudioHardwarePropertyDefaultOutputDevice,
                                   MPH_CORE_AUDIO_ROLE_OUTPUT, out_device_id, out_found);
}

mph_status_t mph_core_audio_get_default_system_output(mph_device_id_t *out_device_id,
                                                      bool *out_found) {
    return get_default_role_device(kAudioHardwarePropertyDefaultSystemOutputDevice,
                                   MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT, out_device_id, out_found);
}

mph_status_t mph_core_audio_set_default_input(const mph_device_id_t *device_id) {
    return set_default_role_device(device_id, MPH_CORE_AUDIO_ROLE_INPUT,
                                   kAudioHardwarePropertyDefaultInputDevice);
}

mph_status_t mph_core_audio_set_default_output(const mph_device_id_t *device_id) {
    return set_default_role_device(device_id, MPH_CORE_AUDIO_ROLE_OUTPUT,
                                   kAudioHardwarePropertyDefaultOutputDevice);
}

mph_status_t mph_core_audio_set_default_system_output(const mph_device_id_t *device_id) {
    return set_default_role_device(device_id, MPH_CORE_AUDIO_ROLE_SYSTEM_OUTPUT,
                                   kAudioHardwarePropertyDefaultSystemOutputDevice);
}
