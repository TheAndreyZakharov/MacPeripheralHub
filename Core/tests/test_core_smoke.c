#include "PeripheralCore.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define FIXTURE_FIELD_COUNT 12
#define DISPLAY_FIXTURE_FIELD_COUNT 16
#define CAMERA_FIXTURE_FIELD_COUNT 8
#define HID_FIXTURE_FIELD_COUNT 12
#define USB_FIXTURE_FIELD_COUNT 18
#define BLUETOOTH_FIXTURE_FIELD_COUNT 10
#define INVENTORY_SNAPSHOT_FIELD_COUNT 5

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    size_t event_count;
    uint32_t last_flags;
    mph_status_t last_status;
    size_t last_plan_action_count;
} watcher_test_context_t;

static int sqlite_scalar_int(const char *path, const char *sql) {
    sqlite3 *connection = NULL;
    assert(sqlite3_open_v2(path, &connection, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);

    sqlite3_stmt *statement = NULL;
    assert(sqlite3_prepare_v2(connection, sql, -1, &statement, NULL) == SQLITE_OK);
    int value = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int(statement, 0);
    } else {
        assert(false);
    }
    sqlite3_finalize(statement);
    sqlite3_close(connection);
    return value;
}

static void watcher_test_deadline(struct timespec *out_time) {
    struct timeval now;
    gettimeofday(&now, NULL);
    out_time->tv_sec = now.tv_sec + 2;
    out_time->tv_nsec = (long)now.tv_usec * 1000L;
}

static void watcher_test_callback(const mph_audio_watcher_event_t *event, void *context) {
    watcher_test_context_t *test_context = (watcher_test_context_t *)context;
    assert(event != NULL);
    assert(test_context != NULL);

    pthread_mutex_lock(&test_context->mutex);
    test_context->event_count += 1;
    test_context->last_flags = event->flags;
    test_context->last_status = event->status;
    test_context->last_plan_action_count = event->plan.action_count;
    pthread_cond_signal(&test_context->condition);
    pthread_mutex_unlock(&test_context->mutex);
}

static bool watcher_wait_for_events(watcher_test_context_t *test_context, size_t expected_count) {
    struct timespec deadline;
    watcher_test_deadline(&deadline);

    pthread_mutex_lock(&test_context->mutex);
    while (test_context->event_count < expected_count) {
        int wait_status =
            pthread_cond_timedwait(&test_context->condition, &test_context->mutex, &deadline);
        if (wait_status == ETIMEDOUT) {
            break;
        }
    }
    bool reached = test_context->event_count >= expected_count;
    pthread_mutex_unlock(&test_context->mutex);
    return reached;
}

static void test_device_list(void) {
    mph_device_t device;
    mph_device_init(&device);

    assert(mph_device_id_from_parts(&device.id, "audio", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&device, "USB Microphone") == MPH_STATUS_OK);
    device.category = MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    device.transport = MPH_DEVICE_TRANSPORT_USB;
    mph_device_set_connection_state(&device, MPH_DEVICE_CONNECTION_CONNECTED);
    device.audio.sample_rate_hz = 48000.0;
    device.audio.channel_count = 2;

    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_device_list_append(list, &device) == MPH_STATUS_OK);
    assert(mph_device_list_count(list) == 1);

    const mph_device_t *stored = mph_device_list_find_by_id(list, &device.id);
    assert(stored != NULL);
    assert(strcmp(stored->display_name, "USB Microphone") == 0);
    assert(strcmp(mph_device_category_name(stored->category), "audio_input") == 0);
    assert(strcmp(mph_device_transport_name(stored->transport), "usb") == 0);
    assert(strcmp(mph_device_connection_state_name(stored->connection_state), "connected") == 0);

    mph_device_list_destroy(list);
}

static void test_swift_bridge_cstrings(void) {
    mph_device_t device;
    mph_device_init(&device);
    assert(mph_device_id_from_parts(&device.id, "coreaudio.input", "studio-mic") ==
           MPH_STATUS_OK);
    assert(mph_device_set_display_name(&device, "Studio Mic") == MPH_STATUS_OK);
    assert(mph_device_set_vendor_name(&device, "Test Vendor") == MPH_STATUS_OK);
    assert(mph_device_set_model_name(&device, "MV7") == MPH_STATUS_OK);
    assert(mph_device_set_serial_number(&device, "SERIAL-1") == MPH_STATUS_OK);
    assert(mph_device_set_camera_unique_id(&device, "camera-uid") == MPH_STATUS_OK);
    assert(mph_device_set_bluetooth_address(&device, "aa:bb:cc:dd:ee:ff") == MPH_STATUS_OK);

    assert(strcmp(mph_swift_device_id_cstr(&device), "coreaudio.input:studio-mic") == 0);
    assert(strcmp(mph_swift_device_display_name_cstr(&device), "Studio Mic") == 0);
    assert(strcmp(mph_swift_device_vendor_name_cstr(&device), "Test Vendor") == 0);
    assert(strcmp(mph_swift_device_model_name_cstr(&device), "MV7") == 0);
    assert(strcmp(mph_swift_device_serial_number_cstr(&device), "SERIAL-1") == 0);
    assert(strcmp(mph_swift_device_camera_unique_id_cstr(&device), "camera-uid") == 0);
    assert(strcmp(mph_swift_device_bluetooth_address_cstr(&device), "aa:bb:cc:dd:ee:ff") == 0);

    mph_profile_t profile;
    mph_profile_init(&profile);
    assert(mph_profile_configure(&profile, "studio", "Studio") == MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_DEFAULT_INPUT, &device.id) ==
           MPH_STATUS_OK);

    assert(strcmp(mph_swift_profile_id_cstr(&profile), "studio") == 0);
    assert(strcmp(mph_swift_profile_name_cstr(&profile), "Studio") == 0);
    assert(strcmp(mph_swift_selection_profile_id_cstr(&profile.selection), "studio") == 0);
    assert(strcmp(mph_swift_selection_role_device_id_cstr(&profile.selection,
                                                          MPH_DEVICE_ROLE_DEFAULT_INPUT),
                  "coreaudio.input:studio-mic") == 0);
}

static size_t split_fields(char *line, char **fields, size_t capacity) {
    size_t count = 0;
    char *field_start = line;

    for (char *cursor = line; *cursor != '\0'; cursor += 1) {
        if (*cursor == '|' || *cursor == '\n' || *cursor == '\r') {
            *cursor = '\0';
            if (count < capacity) {
                fields[count] = field_start;
                count += 1;
            }
            field_start = cursor + 1;
        }
    }

    if (count < capacity && field_start[0] != '\0') {
        fields[count] = field_start;
        count += 1;
    }

    return count;
}

static size_t split_fixture_line(char *line, char *fields[FIXTURE_FIELD_COUNT]) {
    return split_fields(line, fields, FIXTURE_FIELD_COUNT);
}

static uint32_t fixture_u32(const char *value) {
    return value != NULL && value[0] != '\0' ? (uint32_t)strtoul(value, NULL, 10) : 0;
}

static double fixture_double(const char *value) {
    return value != NULL && value[0] != '\0' ? strtod(value, NULL) : 0.0;
}

static bool fixture_bool(const char *value) {
    return value != NULL && strcmp(value, "1") == 0;
}

static void test_device_modeling_from_fixture(void) {
    FILE *fixture = fopen("Core/fixtures/artificial_devices.tsv", "r");
    assert(fixture != NULL);

    char line[1024];
    size_t parsed_devices = 0;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        if (line[0] == '#') {
            continue;
        }

        char *fields[FIXTURE_FIELD_COUNT] = {0};
        size_t field_count = split_fixture_line(line, fields);
        assert(field_count == FIXTURE_FIELD_COUNT);

        mph_device_t device;
        mph_device_init(&device);
        assert(mph_device_set_display_name(&device, fields[0]) == MPH_STATUS_OK);
        assert(mph_device_set_vendor_name(&device, fields[1]) == MPH_STATUS_OK);
        assert(mph_device_set_model_name(&device, fields[2]) == MPH_STATUS_OK);
        assert(mph_device_set_serial_number(&device, fields[3]) == MPH_STATUS_OK);
        device.transport = mph_device_transport_from_name(fields[5]);
        device.hid.usage_page = fixture_u32(fields[6]);
        device.hid.usage = fixture_u32(fields[7]);
        device.hid.vendor_id = fixture_u32(fields[8]);
        device.hid.product_id = fixture_u32(fields[9]);
        device.usb.vendor_id = fixture_u32(fields[8]);
        device.usb.product_id = fixture_u32(fields[9]);
        assert(mph_device_set_bluetooth_address(&device, fields[10]) == MPH_STATUS_OK);
        assert(mph_device_set_camera_unique_id(&device, fields[11]) == MPH_STATUS_OK);

        mph_device_category_t expected_category = mph_device_category_from_name(fields[4]);
        assert(expected_category != MPH_DEVICE_CATEGORY_UNKNOWN);
        assert(device.transport ==
               mph_device_transport_from_name(mph_device_transport_name(device.transport)));
        assert(mph_device_infer_category(&device) == expected_category);
        mph_device_apply_inferred_category(&device);
        assert(device.category == expected_category);

        char fingerprint[MPH_DEVICE_FINGERPRINT_CAPACITY];
        assert(mph_device_fingerprint(&device, fingerprint, sizeof(fingerprint)) == MPH_STATUS_OK);
        assert(strlen(fingerprint) > 0);

        parsed_devices += 1;
    }

    fclose(fixture);
    assert(parsed_devices == 9);
}

static void test_device_normalization_and_matching(void) {
    char normalized[MPH_DEVICE_TEXT_CAPACITY];
    assert(mph_device_normalize_name("  USB-Microphone__Pro  ", normalized, sizeof(normalized)) ==
           MPH_STATUS_OK);
    assert(strcmp(normalized, "usb microphone pro") == 0);

    mph_device_t known;
    mph_device_t candidate;
    mph_device_init(&known);
    mph_device_init(&candidate);

    assert(mph_device_set_display_name(&known, "USB Microphone Pro") == MPH_STATUS_OK);
    assert(mph_device_set_vendor_name(&known, "Shure") == MPH_STATUS_OK);
    assert(mph_device_set_model_name(&known, "MV7") == MPH_STATUS_OK);
    assert(mph_device_set_serial_number(&known, "MIC-001") == MPH_STATUS_OK);
    known.category = MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    known.transport = MPH_DEVICE_TRANSPORT_USB;

    assert(mph_device_set_display_name(&candidate, "usb-microphone pro") == MPH_STATUS_OK);
    assert(mph_device_set_vendor_name(&candidate, "shure") == MPH_STATUS_OK);
    assert(mph_device_set_model_name(&candidate, "mv7") == MPH_STATUS_OK);
    assert(mph_device_set_serial_number(&candidate, "MIC-001") == MPH_STATUS_OK);
    candidate.category = MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    candidate.transport = MPH_DEVICE_TRANSPORT_USB;

    assert(mph_device_match_score(&known, &candidate) >= 90);
    assert(mph_device_is_probable_match(&known, &candidate));

    mph_device_t generic_known;
    mph_device_t generic_candidate;
    mph_device_init(&generic_known);
    mph_device_init(&generic_candidate);
    assert(mph_device_set_display_name(&generic_known, "USB Device") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&generic_candidate, "USB Device") == MPH_STATUS_OK);
    generic_known.category = MPH_DEVICE_CATEGORY_USB;
    generic_candidate.category = MPH_DEVICE_CATEGORY_USB;
    assert(!mph_device_is_probable_match(&generic_known, &generic_candidate));
}

static void test_serialization_names_roundtrip(void) {
    for (int category = MPH_DEVICE_CATEGORY_DISPLAY; category <= MPH_DEVICE_CATEGORY_UNKNOWN;
         category += 1) {
        const char *name = mph_device_category_name((mph_device_category_t)category);
        assert(mph_device_category_from_name(name) == (mph_device_category_t)category);
    }
    assert(mph_device_category_from_name("definitely_not_a_category") ==
           MPH_DEVICE_CATEGORY_UNKNOWN);

    for (int transport = MPH_DEVICE_TRANSPORT_BUILT_IN;
         transport <= MPH_DEVICE_TRANSPORT_UNKNOWN; transport += 1) {
        const char *name = mph_device_transport_name((mph_device_transport_t)transport);
        assert(mph_device_transport_from_name(name) == (mph_device_transport_t)transport);
    }
    assert(mph_device_transport_from_name("definitely_not_a_transport") ==
           MPH_DEVICE_TRANSPORT_UNKNOWN);

    assert(strcmp(mph_device_role_name(MPH_DEVICE_ROLE_DEFAULT_INPUT), "default_input") == 0);
    assert(strcmp(mph_device_role_name(MPH_DEVICE_ROLE_DEFAULT_OUTPUT), "default_output") == 0);
    assert(strcmp(mph_device_role_name(MPH_DEVICE_ROLE_SYSTEM_OUTPUT), "system_output") == 0);
    assert(strcmp(mph_device_role_name(MPH_DEVICE_ROLE_PREFERRED_CAMERA), "preferred_camera") ==
           0);
    assert(strcmp(mph_reconcile_action_name(MPH_RECONCILE_ACTION_NOOP), "noop") == 0);
    assert(strcmp(mph_reconcile_action_name(MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT),
                  "set_default_input") == 0);
    assert(strcmp(mph_status_name(MPH_STATUS_CAPACITY_EXCEEDED), "capacity_exceeded") == 0);
}

static void test_core_audio_mapper(void) {
    mph_core_audio_raw_device_t raw_device;
    mph_core_audio_raw_device_init(&raw_device);
    snprintf(raw_device.uid, sizeof(raw_device.uid), "%s", "UnitTestAudioUID");
    snprintf(raw_device.name, sizeof(raw_device.name), "%s", "Unit Test Audio Interface");
    snprintf(raw_device.manufacturer, sizeof(raw_device.manufacturer), "%s", "MacPeripheralHub");
    raw_device.transport = MPH_DEVICE_TRANSPORT_USB;
    raw_device.sample_rate_hz = 48000.0;
    raw_device.input_channel_count = 2;
    raw_device.output_channel_count = 4;
    raw_device.is_alive = true;
    raw_device.is_default_input = true;
    raw_device.is_default_output = true;
    raw_device.is_default_system_output = false;

    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_core_audio_map_raw_device(&raw_device, list) == MPH_STATUS_OK);
    assert(mph_device_list_count(list) == 3);

    const mph_device_t *input = mph_device_list_get(list, 0);
    const mph_device_t *output = mph_device_list_get(list, 1);
    const mph_device_t *system_output = mph_device_list_get(list, 2);
    assert(input != NULL);
    assert(output != NULL);
    assert(system_output != NULL);
    assert(input->category == MPH_DEVICE_CATEGORY_AUDIO_INPUT);
    assert(output->category == MPH_DEVICE_CATEGORY_AUDIO_OUTPUT);
    assert(system_output->category == MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT);
    assert(input->audio.channel_count == 2);
    assert(output->audio.channel_count == 4);
    assert(system_output->audio.channel_count == 4);
    assert(input->audio.is_default_input);
    assert(output->audio.is_default_output);
    assert(!system_output->audio.is_default_system_output);
    assert(input->connection_state == MPH_DEVICE_CONNECTION_CONNECTED);

    mph_device_id_t expected_input_id;
    assert(mph_core_audio_role_device_id(&expected_input_id, MPH_CORE_AUDIO_ROLE_INPUT,
                                         raw_device.uid) == MPH_STATUS_OK);
    assert(mph_device_id_equal(&input->id, &expected_input_id));

    mph_core_audio_raw_device_t missing_channels;
    mph_core_audio_raw_device_init(&missing_channels);
    snprintf(missing_channels.uid, sizeof(missing_channels.uid), "%s", "NoChannels");
    assert(mph_core_audio_map_raw_device(&missing_channels, list) == MPH_STATUS_NOT_FOUND);

    mph_device_list_destroy(list);
}

static void test_display_mapper(void) {
    FILE *fixture = fopen("Core/fixtures/display_devices.tsv", "r");
    assert(fixture != NULL);

    char line[1024];
    size_t parsed_devices = 0;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        if (line[0] == '#') {
            continue;
        }

        char *fields[DISPLAY_FIXTURE_FIELD_COUNT] = {0};
        size_t field_count = split_fields(line, fields, DISPLAY_FIXTURE_FIELD_COUNT);
        assert(field_count == DISPLAY_FIXTURE_FIELD_COUNT);

        mph_display_raw_device_t raw_device;
        mph_display_raw_device_init(&raw_device);
        raw_device.display_id = fixture_u32(fields[0]);
        snprintf(raw_device.name, sizeof(raw_device.name), "%s", fields[1]);
        snprintf(raw_device.vendor_name, sizeof(raw_device.vendor_name), "%s", fields[2]);
        snprintf(raw_device.model_name, sizeof(raw_device.model_name), "%s", fields[3]);
        snprintf(raw_device.serial_number, sizeof(raw_device.serial_number), "%s", fields[4]);
        raw_device.vendor_id = fixture_u32(fields[5]);
        raw_device.product_id = fixture_u32(fields[6]);
        raw_device.serial_numeric = fixture_u32(fields[7]);
        raw_device.width_px = fixture_u32(fields[8]);
        raw_device.height_px = fixture_u32(fields[9]);
        raw_device.refresh_rate_hz = fixture_double(fields[10]);
        raw_device.is_main = fixture_bool(fields[11]);
        raw_device.is_builtin = fixture_bool(fields[12]);
        raw_device.is_online = fixture_bool(fields[13]);
        raw_device.transport = mph_device_transport_from_name(fields[14]);

        mph_device_transport_t expected_transport = mph_device_transport_from_name(fields[15]);
        assert(mph_display_infer_transport(&raw_device) == expected_transport);

        mph_device_t device;
        assert(mph_display_map_raw_device(&raw_device, &device) == MPH_STATUS_OK);
        assert(!mph_device_id_is_empty(&device.id));
        assert(device.category == MPH_DEVICE_CATEGORY_DISPLAY);
        assert(device.transport == expected_transport);
        assert(device.connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
        assert(device.is_connected);
        assert(device.display.width_px == raw_device.width_px);
        assert(device.display.height_px == raw_device.height_px);
        assert(device.display.refresh_rate_hz == raw_device.refresh_rate_hz);
        assert(device.display.is_main == raw_device.is_main);
        assert(strcmp(device.display_name, raw_device.name) == 0);
        assert(strcmp(device.vendor_name, raw_device.vendor_name) == 0);
        assert(strcmp(device.model_name, raw_device.model_name) == 0);
        assert(strcmp(device.serial_number, raw_device.serial_number) == 0);

        parsed_devices += 1;
    }

    fclose(fixture);
    assert(parsed_devices == 4);
}

static void test_camera_mapper(void) {
    assert(strcmp(mph_camera_permission_status_name(MPH_CAMERA_PERMISSION_UNKNOWN), "unknown") ==
           0);
    assert(strcmp(mph_camera_permission_status_name(MPH_CAMERA_PERMISSION_NOT_DETERMINED),
                  "not_determined") == 0);
    assert(strcmp(mph_camera_permission_status_name(MPH_CAMERA_PERMISSION_RESTRICTED),
                  "restricted") == 0);
    assert(strcmp(mph_camera_permission_status_name(MPH_CAMERA_PERMISSION_DENIED), "denied") == 0);
    assert(strcmp(mph_camera_permission_status_name(MPH_CAMERA_PERMISSION_AUTHORIZED),
                  "authorized") == 0);
    assert(strcmp(mph_camera_permission_status_name((mph_camera_permission_status_t)999),
                  "unknown") == 0);

    FILE *fixture = fopen("Core/fixtures/camera_devices.tsv", "r");
    assert(fixture != NULL);

    char line[1024];
    size_t parsed_devices = 0;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        if (line[0] == '#') {
            continue;
        }

        char *fields[CAMERA_FIXTURE_FIELD_COUNT] = {0};
        size_t field_count = split_fields(line, fields, CAMERA_FIXTURE_FIELD_COUNT);
        assert(field_count == CAMERA_FIXTURE_FIELD_COUNT);

        mph_camera_raw_device_t raw_device;
        mph_camera_raw_device_init(&raw_device);
        snprintf(raw_device.unique_id, sizeof(raw_device.unique_id), "%s", fields[0]);
        snprintf(raw_device.localized_name, sizeof(raw_device.localized_name), "%s", fields[1]);
        snprintf(raw_device.manufacturer, sizeof(raw_device.manufacturer), "%s", fields[2]);
        snprintf(raw_device.model_id, sizeof(raw_device.model_id), "%s", fields[3]);
        snprintf(raw_device.device_type, sizeof(raw_device.device_type), "%s", fields[4]);
        raw_device.transport = mph_device_transport_from_name(fields[5]);
        raw_device.is_connected = fixture_bool(fields[6]);

        mph_device_transport_t expected_transport = mph_device_transport_from_name(fields[7]);
        assert(mph_camera_infer_transport(&raw_device) == expected_transport);

        mph_device_t device;
        assert(mph_camera_map_raw_device(&raw_device, &device) == MPH_STATUS_OK);
        assert(!mph_device_id_is_empty(&device.id));
        assert(device.category == MPH_DEVICE_CATEGORY_CAMERA);
        assert(device.transport == expected_transport);
        assert(device.connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
        assert(device.is_connected);
        assert(strcmp(device.display_name, raw_device.localized_name) == 0);
        assert(strcmp(device.vendor_name, raw_device.manufacturer) == 0);
        assert(strcmp(device.model_name, raw_device.model_id) == 0);
        assert(strcmp(device.camera.unique_id, raw_device.unique_id) == 0);
        assert(!device.camera.supports_global_default);
        assert(!mph_camera_global_default_supported());

        parsed_devices += 1;
    }

    fclose(fixture);
    assert(parsed_devices == 4);
}

static void test_hid_mapper(void) {
    FILE *fixture = fopen("Core/fixtures/hid_devices.tsv", "r");
    assert(fixture != NULL);

    char line[1024];
    size_t parsed_devices = 0;
    size_t mapped_devices = 0;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        if (line[0] == '#') {
            continue;
        }

        char *fields[HID_FIXTURE_FIELD_COUNT] = {0};
        size_t field_count = split_fields(line, fields, HID_FIXTURE_FIELD_COUNT);
        assert(field_count == HID_FIXTURE_FIELD_COUNT);

        mph_hid_raw_device_t raw_device;
        mph_hid_raw_device_init(&raw_device);
        snprintf(raw_device.product_name, sizeof(raw_device.product_name), "%s", fields[0]);
        snprintf(raw_device.manufacturer, sizeof(raw_device.manufacturer), "%s", fields[1]);
        snprintf(raw_device.serial_number, sizeof(raw_device.serial_number), "%s", fields[2]);
        snprintf(raw_device.transport_name, sizeof(raw_device.transport_name), "%s", fields[3]);
        raw_device.vendor_id = fixture_u32(fields[4]);
        raw_device.product_id = fixture_u32(fields[5]);
        raw_device.usage_page = fixture_u32(fields[6]);
        raw_device.usage = fixture_u32(fields[7]);
        raw_device.registry_id = strtoull(fields[8], NULL, 10);
        raw_device.is_connected = fixture_bool(fields[9]);

        mph_device_category_t expected_category = mph_device_category_from_name(fields[10]);
        mph_device_transport_t expected_transport = mph_device_transport_from_name(fields[11]);
        assert(mph_hid_infer_category(&raw_device) == expected_category);
        assert(mph_hid_infer_transport(&raw_device) == expected_transport);

        mph_device_t device;
        mph_status_t status = mph_hid_map_raw_device(&raw_device, &device);
        if (expected_category == MPH_DEVICE_CATEGORY_UNKNOWN) {
            assert(status == MPH_STATUS_NOT_FOUND);
            parsed_devices += 1;
            continue;
        }

        assert(status == MPH_STATUS_OK);
        assert(!mph_device_id_is_empty(&device.id));
        assert(device.category == expected_category);
        assert(device.transport == expected_transport);
        assert(device.connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
        assert(device.is_connected);
        assert(strcmp(device.display_name, raw_device.product_name) == 0);
        assert(strcmp(device.vendor_name, raw_device.manufacturer) == 0);
        assert(strcmp(device.serial_number, raw_device.serial_number) == 0);
        assert(device.hid.vendor_id == raw_device.vendor_id);
        assert(device.hid.product_id == raw_device.product_id);
        assert(device.hid.usage_page == raw_device.usage_page);
        assert(device.hid.usage == raw_device.usage);

        mapped_devices += 1;
        parsed_devices += 1;
    }

    fclose(fixture);
    assert(parsed_devices == 5);
    assert(mapped_devices == 4);
}

static void test_usb_mapper_and_dedup(void) {
    FILE *fixture = fopen("Core/fixtures/usb_devices.tsv", "r");
    assert(fixture != NULL);

    mph_device_list_t *dedup_list = mph_device_list_create();
    assert(dedup_list != NULL);

    char line[1024];
    size_t parsed_devices = 0;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        if (line[0] == '#') {
            continue;
        }

        char *fields[USB_FIXTURE_FIELD_COUNT] = {0};
        size_t field_count = split_fields(line, fields, USB_FIXTURE_FIELD_COUNT);
        assert(field_count == USB_FIXTURE_FIELD_COUNT);

        mph_usb_raw_device_t raw_device;
        mph_usb_raw_device_init(&raw_device);
        snprintf(raw_device.product_name, sizeof(raw_device.product_name), "%s", fields[0]);
        snprintf(raw_device.vendor_name, sizeof(raw_device.vendor_name), "%s", fields[1]);
        snprintf(raw_device.serial_number, sizeof(raw_device.serial_number), "%s", fields[2]);
        raw_device.vendor_id = fixture_u32(fields[3]);
        raw_device.product_id = fixture_u32(fields[4]);
        raw_device.device_class = fixture_u32(fields[5]);
        raw_device.device_subclass = fixture_u32(fields[6]);
        raw_device.device_protocol = fixture_u32(fields[7]);
        raw_device.interface_class_mask = fixture_u32(fields[8]);
        raw_device.speed_mbps = fixture_u32(fields[9]);
        raw_device.power_ma = fixture_u32(fields[10]);
        raw_device.location_id = fixture_u32(fields[11]);
        raw_device.registry_id = strtoull(fields[12], NULL, 10);
        raw_device.parent_registry_id = strtoull(fields[13], NULL, 10);
        raw_device.depth = fixture_u32(fields[14]);
        raw_device.is_connected = fixture_bool(fields[15]);

        mph_device_category_t expected_category = mph_device_category_from_name(fields[16]);
        assert(mph_usb_infer_category(&raw_device) == expected_category);

        mph_device_t device;
        assert(mph_usb_map_raw_device(&raw_device, &device) == MPH_STATUS_OK);
        assert(!mph_device_id_is_empty(&device.id));
        assert(device.category == expected_category);
        assert(device.transport == MPH_DEVICE_TRANSPORT_USB);
        assert(device.connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
        assert(device.is_connected);
        assert(device.usb.vendor_id == raw_device.vendor_id);
        assert(device.usb.product_id == raw_device.product_id);
        assert(device.usb.speed_mbps == raw_device.speed_mbps);
        assert(device.usb.power_ma == raw_device.power_ma);
        assert(device.display_name[0] != '\0');

        if (strcmp(fields[17], "none") != 0) {
            mph_device_t existing;
            mph_device_init(&existing);
            existing.category = mph_device_category_from_name(fields[17]);
            existing.transport = MPH_DEVICE_TRANSPORT_USB;
            assert(mph_device_set_display_name(&existing, raw_device.product_name) ==
                   MPH_STATUS_OK);
            assert(mph_device_set_serial_number(&existing, raw_device.serial_number) ==
                   MPH_STATUS_OK);
            existing.hid.vendor_id = raw_device.vendor_id;
            existing.hid.product_id = raw_device.product_id;
            existing.usb.vendor_id = raw_device.vendor_id;
            existing.usb.product_id = raw_device.product_id;
            assert(mph_usb_matches_device(&raw_device, &existing));
        }

        size_t before_append = mph_device_list_count(dedup_list);
        assert(mph_usb_append_mapped_device(dedup_list, &raw_device) == MPH_STATUS_OK);
        assert(mph_device_list_count(dedup_list) == before_append + 1);
        assert(mph_usb_append_mapped_device(dedup_list, &raw_device) == MPH_STATUS_OK);
        assert(mph_device_list_count(dedup_list) == before_append + 1);

        parsed_devices += 1;
    }

    fclose(fixture);
    mph_device_list_destroy(dedup_list);
    assert(parsed_devices == 6);
}

static void test_bluetooth_mapper_and_merge(void) {
    FILE *fixture = fopen("Core/fixtures/bluetooth_devices.tsv", "r");
    assert(fixture != NULL);

    mph_device_list_t *dedup_list = mph_device_list_create();
    assert(dedup_list != NULL);

    char line[1024];
    size_t parsed_devices = 0;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        if (line[0] == '#') {
            continue;
        }

        char *fields[BLUETOOTH_FIXTURE_FIELD_COUNT] = {0};
        size_t field_count = split_fields(line, fields, BLUETOOTH_FIXTURE_FIELD_COUNT);
        assert(field_count == BLUETOOTH_FIXTURE_FIELD_COUNT);

        mph_bluetooth_raw_device_t raw_device;
        mph_bluetooth_raw_device_init(&raw_device);
        snprintf(raw_device.name, sizeof(raw_device.name), "%s", fields[0]);
        snprintf(raw_device.address, sizeof(raw_device.address), "%s", fields[1]);
        raw_device.class_of_device = fixture_u32(fields[2]);
        raw_device.major_device_class = fixture_u32(fields[3]);
        raw_device.minor_device_class = fixture_u32(fields[4]);
        raw_device.major_service_class = fixture_u32(fields[5]);
        raw_device.is_paired = fixture_bool(fields[6]);
        raw_device.is_connected = fixture_bool(fields[7]);

        bool expected_audio = fixture_bool(fields[8]);
        bool expected_hid = fixture_bool(fields[9]);
        assert(mph_bluetooth_is_audio_device(&raw_device) == expected_audio);
        assert(mph_bluetooth_is_hid_device(&raw_device) == expected_hid);
        assert(mph_bluetooth_infer_category(&raw_device) == MPH_DEVICE_CATEGORY_BLUETOOTH);

        char normalized_address[MPH_BLUETOOTH_ADDRESS_CAPACITY];
        assert(mph_bluetooth_normalize_address(raw_device.address, normalized_address,
                                               sizeof(normalized_address)) == MPH_STATUS_OK);
        assert(strchr(normalized_address, '-') == NULL);

        mph_device_t device;
        assert(mph_bluetooth_map_raw_device(&raw_device, &device) == MPH_STATUS_OK);
        assert(!mph_device_id_is_empty(&device.id));
        assert(device.category == MPH_DEVICE_CATEGORY_BLUETOOTH);
        assert(device.transport == MPH_DEVICE_TRANSPORT_BLUETOOTH);
        assert(device.connection_state == (raw_device.is_connected
                                               ? MPH_DEVICE_CONNECTION_CONNECTED
                                               : MPH_DEVICE_CONNECTION_DISCONNECTED));
        assert(strcmp(device.bluetooth.address, normalized_address) == 0);
        assert(device.bluetooth.is_paired == raw_device.is_paired);
        assert(device.bluetooth.is_connected == raw_device.is_connected);
        assert(device.bluetooth.class_of_device == raw_device.class_of_device);

        mph_device_t address_match;
        mph_device_init(&address_match);
        address_match.category = MPH_DEVICE_CATEGORY_BLUETOOTH;
        address_match.transport = MPH_DEVICE_TRANSPORT_BLUETOOTH;
        assert(mph_device_set_bluetooth_address(&address_match, normalized_address) ==
               MPH_STATUS_OK);
        assert(mph_bluetooth_matches_device(&raw_device, &address_match));

        if (expected_audio || expected_hid) {
            mph_device_t linked_device;
            mph_device_init(&linked_device);
            linked_device.category =
                expected_audio ? MPH_DEVICE_CATEGORY_AUDIO_OUTPUT : MPH_DEVICE_CATEGORY_MOUSE;
            linked_device.transport = MPH_DEVICE_TRANSPORT_BLUETOOTH;
            assert(mph_device_set_display_name(&linked_device, raw_device.name) == MPH_STATUS_OK);
            assert(mph_bluetooth_matches_device(&raw_device, &linked_device));
        }

        size_t before_append = mph_device_list_count(dedup_list);
        assert(mph_bluetooth_append_mapped_device(dedup_list, &raw_device) == MPH_STATUS_OK);
        assert(mph_device_list_count(dedup_list) == before_append + 1);
        assert(mph_bluetooth_append_mapped_device(dedup_list, &raw_device) == MPH_STATUS_OK);
        assert(mph_device_list_count(dedup_list) == before_append + 1);

        parsed_devices += 1;
    }

    mph_bluetooth_raw_device_t renamed_device;
    mph_bluetooth_raw_device_init(&renamed_device);
    snprintf(renamed_device.name, sizeof(renamed_device.name), "%s", "Renamed AirPods");
    snprintf(renamed_device.address, sizeof(renamed_device.address), "%s", "aa:bb:cc:dd:ee:01");
    renamed_device.is_paired = true;
    renamed_device.is_connected = true;
    size_t before_alias = mph_device_list_count(dedup_list);
    assert(mph_bluetooth_append_mapped_device(dedup_list, &renamed_device) == MPH_STATUS_OK);
    assert(mph_device_list_count(dedup_list) == before_alias);

    fclose(fixture);
    mph_device_list_destroy(dedup_list);
    assert(parsed_devices == 5);
}

static void assert_merge_device(mph_device_list_t *inventory, mph_device_t *device) {
    assert(device != NULL);
    assert(mph_inventory_merge_device(inventory, device) == MPH_STATUS_OK);
}

static void test_inventory_snapshot_and_storage(void) {
    mph_device_list_t *inventory = mph_device_list_create();
    assert(inventory != NULL);

    mph_device_t display;
    mph_device_init(&display);
    assert(mph_device_id_from_parts(&display.id, "display", "studio-display") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&display, "Studio Display") == MPH_STATUS_OK);
    display.category = MPH_DEVICE_CATEGORY_DISPLAY;
    display.transport = MPH_DEVICE_TRANSPORT_THUNDERBOLT;
    display.display.width_px = 5120;
    display.display.height_px = 2880;
    display.display.refresh_rate_hz = 60.0;
    mph_device_set_connection_state(&display, MPH_DEVICE_CONNECTION_CONNECTED);
    assert_merge_device(inventory, &display);

    mph_device_t airpods_audio;
    mph_device_init(&airpods_audio);
    assert(mph_device_id_from_parts(&airpods_audio.id, "coreaudio.output", "airpods-pro") ==
           MPH_STATUS_OK);
    assert(mph_device_set_display_name(&airpods_audio, "AirPods Pro") == MPH_STATUS_OK);
    airpods_audio.category = MPH_DEVICE_CATEGORY_AUDIO_OUTPUT;
    airpods_audio.transport = MPH_DEVICE_TRANSPORT_BLUETOOTH;
    airpods_audio.audio.channel_count = 2;
    airpods_audio.audio.sample_rate_hz = 48000.0;
    mph_device_set_connection_state(&airpods_audio, MPH_DEVICE_CONNECTION_CONNECTED);
    assert_merge_device(inventory, &airpods_audio);

    mph_bluetooth_raw_device_t airpods_bt_raw;
    mph_bluetooth_raw_device_init(&airpods_bt_raw);
    snprintf(airpods_bt_raw.name, sizeof(airpods_bt_raw.name), "%s", "AirPods Pro");
    snprintf(airpods_bt_raw.address, sizeof(airpods_bt_raw.address), "%s", "AA-BB-CC-DD-EE-01");
    airpods_bt_raw.major_device_class = 4;
    airpods_bt_raw.is_paired = true;
    airpods_bt_raw.is_connected = true;
    mph_device_t airpods_bt;
    assert(mph_bluetooth_map_raw_device(&airpods_bt_raw, &airpods_bt) == MPH_STATUS_OK);
    size_t before_airpods_merge = mph_device_list_count(inventory);
    assert_merge_device(inventory, &airpods_bt);
    assert(mph_device_list_count(inventory) == before_airpods_merge);
    const mph_device_t *merged_airpods = mph_device_list_find_by_id(inventory, &airpods_audio.id);
    assert(merged_airpods != NULL);
    assert(strcmp(merged_airpods->bluetooth.address, "aa:bb:cc:dd:ee:01") == 0);
    assert(merged_airpods->bluetooth.is_paired);

    mph_device_t mouse_hid;
    mph_device_init(&mouse_hid);
    assert(mph_device_id_from_parts(&mouse_hid.id, "hid", "magic-mouse") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&mouse_hid, "Magic Mouse") == MPH_STATUS_OK);
    mouse_hid.category = MPH_DEVICE_CATEGORY_MOUSE;
    mouse_hid.transport = MPH_DEVICE_TRANSPORT_BLUETOOTH;
    mouse_hid.hid.vendor_id = 1452;
    mouse_hid.hid.product_id = 613;
    mouse_hid.hid.usage_page = 1;
    mouse_hid.hid.usage = 2;
    mph_device_set_connection_state(&mouse_hid, MPH_DEVICE_CONNECTION_CONNECTED);
    assert_merge_device(inventory, &mouse_hid);

    mph_bluetooth_raw_device_t mouse_bt_raw;
    mph_bluetooth_raw_device_init(&mouse_bt_raw);
    snprintf(mouse_bt_raw.name, sizeof(mouse_bt_raw.name), "%s", "Magic Mouse");
    snprintf(mouse_bt_raw.address, sizeof(mouse_bt_raw.address), "%s", "AA-BB-CC-DD-EE-02");
    mouse_bt_raw.major_device_class = 5;
    mouse_bt_raw.is_paired = true;
    mouse_bt_raw.is_connected = true;
    mph_device_t mouse_bt;
    assert(mph_bluetooth_map_raw_device(&mouse_bt_raw, &mouse_bt) == MPH_STATUS_OK);
    size_t before_mouse_merge = mph_device_list_count(inventory);
    assert_merge_device(inventory, &mouse_bt);
    assert(mph_device_list_count(inventory) == before_mouse_merge);
    const mph_device_t *merged_mouse = mph_device_list_find_by_id(inventory, &mouse_hid.id);
    assert(merged_mouse != NULL);
    assert(strcmp(merged_mouse->bluetooth.address, "aa:bb:cc:dd:ee:02") == 0);

    mph_device_t keyboard_hid;
    mph_device_init(&keyboard_hid);
    assert(mph_device_id_from_parts(&keyboard_hid.id, "hid", "wired-keyboard") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&keyboard_hid, "Wired Keyboard") == MPH_STATUS_OK);
    keyboard_hid.category = MPH_DEVICE_CATEGORY_KEYBOARD;
    keyboard_hid.transport = MPH_DEVICE_TRANSPORT_USB;
    keyboard_hid.hid.vendor_id = 1452;
    keyboard_hid.hid.product_id = 597;
    keyboard_hid.hid.usage_page = 1;
    keyboard_hid.hid.usage = 6;
    mph_device_set_connection_state(&keyboard_hid, MPH_DEVICE_CONNECTION_CONNECTED);
    assert_merge_device(inventory, &keyboard_hid);

    mph_device_t keyboard_usb;
    mph_device_init(&keyboard_usb);
    assert(mph_device_id_from_parts(&keyboard_usb.id, "usb", "wired-keyboard-shell") ==
           MPH_STATUS_OK);
    assert(mph_device_set_display_name(&keyboard_usb, "Wired Keyboard") == MPH_STATUS_OK);
    keyboard_usb.category = MPH_DEVICE_CATEGORY_USB;
    keyboard_usb.transport = MPH_DEVICE_TRANSPORT_USB;
    keyboard_usb.usb.vendor_id = 1452;
    keyboard_usb.usb.product_id = 597;
    keyboard_usb.usb.speed_mbps = 12;
    keyboard_usb.usb.power_ma = 100;
    mph_device_set_connection_state(&keyboard_usb, MPH_DEVICE_CONNECTION_CONNECTED);
    size_t before_keyboard_merge = mph_device_list_count(inventory);
    assert_merge_device(inventory, &keyboard_usb);
    assert(mph_device_list_count(inventory) == before_keyboard_merge);
    const mph_device_t *merged_keyboard = mph_device_list_find_by_id(inventory, &keyboard_hid.id);
    assert(merged_keyboard != NULL);
    assert(merged_keyboard->usb.vendor_id == 1452);
    assert(merged_keyboard->usb.product_id == 597);
    assert(merged_keyboard->usb.speed_mbps == 12);

    mph_bluetooth_raw_device_t iphone_raw;
    mph_bluetooth_raw_device_init(&iphone_raw);
    snprintf(iphone_raw.name, sizeof(iphone_raw.name), "%s", "iPhone");
    snprintf(iphone_raw.address, sizeof(iphone_raw.address), "%s", "AA-BB-CC-DD-EE-04");
    iphone_raw.class_of_device = 512;
    iphone_raw.is_paired = true;
    iphone_raw.is_connected = false;
    mph_device_t iphone;
    assert(mph_bluetooth_map_raw_device(&iphone_raw, &iphone) == MPH_STATUS_OK);
    assert_merge_device(inventory, &iphone);

    mph_device_t hub;
    mph_device_init(&hub);
    assert(mph_device_id_from_parts(&hub.id, "usb", "studio-hub") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&hub, "USB-C Hub") == MPH_STATUS_OK);
    hub.category = MPH_DEVICE_CATEGORY_HUB;
    hub.transport = MPH_DEVICE_TRANSPORT_USB;
    hub.usb.vendor_id = 10429;
    hub.usb.product_id = 2048;
    hub.usb.speed_mbps = 5000;
    mph_device_set_connection_state(&hub, MPH_DEVICE_CONNECTION_CONNECTED);
    assert_merge_device(inventory, &hub);

    mph_inventory_sort(inventory);

    FILE *fixture = fopen("Core/fixtures/inventory_snapshot.tsv", "r");
    assert(fixture != NULL);
    char line[1024];
    size_t expected_index = 0;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        if (line[0] == '#') {
            continue;
        }

        char *fields[INVENTORY_SNAPSHOT_FIELD_COUNT] = {0};
        size_t field_count = split_fields(line, fields, INVENTORY_SNAPSHOT_FIELD_COUNT);
        assert(field_count == INVENTORY_SNAPSHOT_FIELD_COUNT);
        const mph_device_t *device = mph_device_list_get(inventory, expected_index);
        assert(device != NULL);
        assert(strcmp(mph_device_id_cstr(&device->id), fields[0]) == 0);
        assert(strcmp(device->display_name, fields[1]) == 0);
        assert(strcmp(mph_device_category_name(device->category), fields[2]) == 0);
        assert(strcmp(mph_device_transport_name(device->transport), fields[3]) == 0);
        assert(device->is_connected == fixture_bool(fields[4]));
        expected_index += 1;
    }
    fclose(fixture);
    assert(expected_index == mph_device_list_count(inventory));

    char db_path[256];
    snprintf(db_path, sizeof(db_path), "/tmp/mph_inventory_db_test_%ld.sqlite3", (long)getpid());
    unlink(db_path);

    mph_db_t *db = NULL;
    assert(mph_db_open(&db, db_path) == MPH_STATUS_OK);
    assert(db != NULL);
    assert(mph_db_migrate(db) == MPH_STATUS_OK);

    mph_device_t stale_device;
    mph_device_init(&stale_device);
    assert(mph_device_id_from_parts(&stale_device.id, "bluetooth", "old-keyboard") ==
           MPH_STATUS_OK);
    assert(mph_device_set_display_name(&stale_device, "Old Keyboard") == MPH_STATUS_OK);
    stale_device.category = MPH_DEVICE_CATEGORY_KEYBOARD;
    stale_device.transport = MPH_DEVICE_TRANSPORT_BLUETOOTH;
    mph_device_set_connection_state(&stale_device, MPH_DEVICE_CONNECTION_CONNECTED);
    assert(mph_db_save_known_device(db, &stale_device) == MPH_STATUS_OK);

    assert(mph_inventory_store_known_devices(db, inventory) == MPH_STATUS_OK);
    mph_device_list_t *known_devices = mph_device_list_create();
    assert(known_devices != NULL);
    assert(mph_db_load_known_devices(db, known_devices) == MPH_STATUS_OK);
    assert(mph_device_list_count(known_devices) == mph_device_list_count(inventory) + 1);

    const mph_device_t *stale_loaded = mph_device_list_find_by_id(known_devices, &stale_device.id);
    assert(stale_loaded != NULL);
    assert(stale_loaded->connection_state == MPH_DEVICE_CONNECTION_DISCONNECTED);
    for (size_t index = 0; index < mph_device_list_count(inventory); index += 1) {
        const mph_device_t *current = mph_device_list_get(inventory, index);
        const mph_device_t *loaded = mph_device_list_find_by_id(known_devices, &current->id);
        assert(loaded != NULL);
        assert(loaded->connection_state == current->connection_state);
    }

    mph_device_list_destroy(known_devices);
    mph_db_close(db);
    unlink(db_path);
    mph_device_list_destroy(inventory);
}

static void test_core_audio_live_smoke(void) {
    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_core_audio_enumerate_devices(list) == MPH_STATUS_OK);

    for (size_t index = 0; index < mph_device_list_count(list); index += 1) {
        const mph_device_t *device = mph_device_list_get(list, index);
        assert(device != NULL);
        assert(!mph_device_id_is_empty(&device->id));
        assert(mph_device_category_is_audio(device->category));
    }

    mph_device_id_t default_id;
    bool found = true;
    assert(mph_core_audio_get_default_input(&default_id, &found) == MPH_STATUS_OK);
    assert(found == false || !mph_device_id_is_empty(&default_id));
    assert(mph_core_audio_get_default_output(&default_id, &found) == MPH_STATUS_OK);
    assert(found == false || !mph_device_id_is_empty(&default_id));
    assert(mph_core_audio_get_default_system_output(&default_id, &found) == MPH_STATUS_OK);
    assert(found == false || !mph_device_id_is_empty(&default_id));

    mph_device_id_t missing_id;
    assert(mph_device_id_from_parts(&missing_id, "coreaudio.input", "missing-device-for-test") ==
           MPH_STATUS_OK);
    assert(mph_core_audio_set_default_input(&missing_id) == MPH_STATUS_NOT_FOUND);

    mph_device_list_destroy(list);
}

static void test_display_live_smoke(void) {
    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_display_enumerate_devices(list) == MPH_STATUS_OK);

    bool has_main_display = false;
    for (size_t index = 0; index < mph_device_list_count(list); index += 1) {
        const mph_device_t *device = mph_device_list_get(list, index);
        assert(device != NULL);
        assert(!mph_device_id_is_empty(&device->id));
        assert(device->category == MPH_DEVICE_CATEGORY_DISPLAY);
        assert(device->connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
        assert(device->display.width_px > 0);
        assert(device->display.height_px > 0);
        has_main_display = has_main_display || device->display.is_main;
    }
    assert(mph_device_list_count(list) == 0 || has_main_display);

    mph_device_list_destroy(list);
}

static void test_camera_live_smoke(void) {
    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_camera_enumerate_devices(list) == MPH_STATUS_OK);

    for (size_t index = 0; index < mph_device_list_count(list); index += 1) {
        const mph_device_t *device = mph_device_list_get(list, index);
        assert(device != NULL);
        assert(!mph_device_id_is_empty(&device->id));
        assert(device->category == MPH_DEVICE_CATEGORY_CAMERA);
        assert(device->connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
        assert(device->display_name[0] != '\0');
        assert(device->camera.unique_id[0] != '\0');
        assert(!device->camera.supports_global_default);
    }

    mph_device_list_destroy(list);
}

static void test_hid_live_smoke(void) {
    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_hid_enumerate_devices(list) == MPH_STATUS_OK);

    for (size_t index = 0; index < mph_device_list_count(list); index += 1) {
        const mph_device_t *device = mph_device_list_get(list, index);
        assert(device != NULL);
        assert(!mph_device_id_is_empty(&device->id));
        assert(device->category == MPH_DEVICE_CATEGORY_KEYBOARD ||
               device->category == MPH_DEVICE_CATEGORY_MOUSE ||
               device->category == MPH_DEVICE_CATEGORY_TRACKPAD);
        assert(device->connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
        assert(device->display_name[0] != '\0');
        assert(device->hid.usage_page != 0);
        assert(device->hid.usage != 0);
    }

    mph_device_list_destroy(list);
}

static void test_usb_live_smoke(void) {
    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_usb_enumerate_devices(list) == MPH_STATUS_OK);

    for (size_t index = 0; index < mph_device_list_count(list); index += 1) {
        const mph_device_t *device = mph_device_list_get(list, index);
        assert(device != NULL);
        assert(!mph_device_id_is_empty(&device->id));
        assert(device->category == MPH_DEVICE_CATEGORY_USB ||
               device->category == MPH_DEVICE_CATEGORY_HUB ||
               device->category == MPH_DEVICE_CATEGORY_DOCK ||
               device->category == MPH_DEVICE_CATEGORY_AUDIO_INTERFACE ||
               device->category == MPH_DEVICE_CATEGORY_CAMERA ||
               device->category == MPH_DEVICE_CATEGORY_KEYBOARD ||
               device->category == MPH_DEVICE_CATEGORY_MOUSE ||
               device->category == MPH_DEVICE_CATEGORY_TRACKPAD ||
               device->category == MPH_DEVICE_CATEGORY_UNKNOWN);
        assert(device->transport == MPH_DEVICE_TRANSPORT_USB);
        assert(device->connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
        assert(device->display_name[0] != '\0');
    }

    mph_device_list_destroy(list);
}

static void test_bluetooth_live_smoke(void) {
    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_bluetooth_enumerate_devices(list) == MPH_STATUS_OK);

    for (size_t index = 0; index < mph_device_list_count(list); index += 1) {
        const mph_device_t *device = mph_device_list_get(list, index);
        assert(device != NULL);
        assert(!mph_device_id_is_empty(&device->id));
        assert(device->category == MPH_DEVICE_CATEGORY_BLUETOOTH);
        assert(device->transport == MPH_DEVICE_TRANSPORT_BLUETOOTH);
        assert(device->display_name[0] != '\0');
        assert(device->connection_state == MPH_DEVICE_CONNECTION_CONNECTED ||
               device->connection_state == MPH_DEVICE_CONNECTION_DISCONNECTED);
        assert(device->bluetooth.address[0] != '\0' || device->display_name[0] != '\0');
    }

    mph_device_list_destroy(list);
}

static void test_inventory_live_smoke(void) {
    mph_device_list_t *list = mph_device_list_create();
    assert(list != NULL);
    assert(mph_inventory_collect(list) == MPH_STATUS_OK);

    mph_device_category_t previous_category = MPH_DEVICE_CATEGORY_DISPLAY;
    for (size_t index = 0; index < mph_device_list_count(list); index += 1) {
        const mph_device_t *device = mph_device_list_get(list, index);
        assert(device != NULL);
        assert(!mph_device_id_is_empty(&device->id));
        assert(device->display_name[0] != '\0');
        assert(device->category >= previous_category);
        assert(device->connection_state == MPH_DEVICE_CONNECTION_CONNECTED ||
               device->connection_state == MPH_DEVICE_CONNECTION_DISCONNECTED ||
               device->connection_state == MPH_DEVICE_CONNECTION_UNAVAILABLE ||
               device->connection_state == MPH_DEVICE_CONNECTION_UNKNOWN);
        previous_category = device->category;
    }

    mph_device_list_destroy(list);
}

static void test_audio_watcher_live_smoke(void) {
    watcher_test_context_t test_context;
    assert(pthread_mutex_init(&test_context.mutex, NULL) == 0);
    assert(pthread_cond_init(&test_context.condition, NULL) == 0);
    test_context.event_count = 0;
    test_context.last_flags = MPH_AUDIO_WATCHER_EVENT_NONE;
    test_context.last_status = MPH_STATUS_OK;
    test_context.last_plan_action_count = 0;

    mph_audio_watcher_config_t config;
    mph_audio_watcher_config_init(&config);
    config.apply_reconcile_actions = false;
    config.fallback_scan_interval_ms = 250;
    config.callback = watcher_test_callback;
    config.callback_context = &test_context;

    mph_audio_watcher_t *watcher = NULL;
    assert(mph_audio_watcher_create(&watcher, &config) == MPH_STATUS_OK);
    assert(watcher != NULL);
    assert(!mph_audio_watcher_is_running(watcher));
    assert(mph_audio_watcher_start(watcher) == MPH_STATUS_OK);
    assert(mph_audio_watcher_is_running(watcher));
    assert(mph_audio_watcher_request_scan(watcher, MPH_AUDIO_WATCHER_EVENT_MANUAL_SCAN) ==
           MPH_STATUS_OK);
    assert(watcher_wait_for_events(&test_context, 1));

    pthread_mutex_lock(&test_context.mutex);
    assert(test_context.last_status == MPH_STATUS_OK);
    assert((test_context.last_flags &
            (MPH_AUDIO_WATCHER_EVENT_MANUAL_SCAN | MPH_AUDIO_WATCHER_EVENT_PERIODIC_SCAN)) != 0);
    size_t next_event_count = test_context.event_count + 1;
    pthread_mutex_unlock(&test_context.mutex);

    mph_selection_t desired;
    mph_selection_init(&desired);
    desired.mode = MPH_ACTIVE_MODE_MANUAL;
    mph_device_id_t missing_output;
    assert(mph_device_id_from_parts(&missing_output, "coreaudio.output",
                                    "watcher-missing-output") == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&desired, MPH_DEVICE_ROLE_DEFAULT_OUTPUT,
                                         &missing_output) == MPH_STATUS_OK);
    assert(mph_audio_watcher_set_desired_selection(watcher, &desired) == MPH_STATUS_OK);
    assert(watcher_wait_for_events(&test_context, next_event_count));

    pthread_mutex_lock(&test_context.mutex);
    assert(test_context.last_status == MPH_STATUS_OK);
    assert(test_context.last_plan_action_count == 1);
    pthread_mutex_unlock(&test_context.mutex);

    mph_audio_watcher_stop(watcher);
    assert(!mph_audio_watcher_is_running(watcher));
    mph_audio_watcher_destroy(watcher);
    pthread_cond_destroy(&test_context.condition);
    pthread_mutex_destroy(&test_context.mutex);
}

static void test_profile_store(void) {
    mph_device_id_t mic_id;
    assert(mph_device_id_from_parts(&mic_id, "audio", "usb-mic") == MPH_STATUS_OK);
    mph_device_id_t camera_id;
    assert(mph_device_id_from_parts(&camera_id, "camera", "facetime-hd") == MPH_STATUS_OK);

    mph_profile_t profile;
    mph_profile_init(&profile);
    assert(mph_profile_configure(&profile, "studio", "Studio") == MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_DEFAULT_INPUT, &mic_id) ==
           MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_PREFERRED_CAMERA, &camera_id) ==
           MPH_STATUS_OK);

    mph_profile_store_t *store = mph_profile_store_create();
    assert(store != NULL);
    assert(mph_profile_store_save(store, &profile) == MPH_STATUS_OK);
    assert(mph_profile_store_count(store) == 1);

    const mph_profile_t *stored = mph_profile_store_find(store, "studio");
    assert(stored != NULL);
    assert(strcmp(stored->name, "Studio") == 0);
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&stored->selection, MPH_DEVICE_ROLE_DEFAULT_INPUT), &mic_id));
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&stored->selection, MPH_DEVICE_ROLE_PREFERRED_CAMERA),
        &camera_id));

    assert(mph_profile_store_delete(store, "studio") == MPH_STATUS_OK);
    assert(mph_profile_store_count(store) == 0);
    mph_profile_store_destroy(store);
}

static void test_reconcile(void) {
    mph_device_id_t usb_mic;
    mph_device_id_t airpods_mic;
    mph_device_id_t speakers;
    mph_device_id_t display_audio;
    mph_device_id_t camera;
    assert(mph_device_id_from_parts(&usb_mic, "audio", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&airpods_mic, "audio", "airpods-mic") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&speakers, "audio", "speakers") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&display_audio, "audio", "display-audio") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&camera, "camera", "facetime") == MPH_STATUS_OK);

    mph_selection_t desired;
    mph_selection_t current;
    mph_selection_init(&desired);
    mph_selection_init(&current);
    desired.mode = MPH_ACTIVE_MODE_PROFILE;
    current.mode = MPH_ACTIVE_MODE_NONE;

    assert(mph_selection_set_role_device(&desired, MPH_DEVICE_ROLE_DEFAULT_INPUT, &usb_mic) ==
           MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&current, MPH_DEVICE_ROLE_DEFAULT_INPUT, &airpods_mic) ==
           MPH_STATUS_OK);

    mph_reconcile_plan_t plan;
    assert(mph_reconcile_audio_defaults(&desired, &current, &plan) == MPH_STATUS_OK);
    assert(plan.action_count == 1);
    assert(plan.actions[0].type == MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT);
    assert(mph_device_id_equal(&plan.actions[0].target_device_id, &usb_mic));

    assert(mph_selection_set_role_device(&current, MPH_DEVICE_ROLE_DEFAULT_INPUT, &usb_mic) ==
           MPH_STATUS_OK);
    assert(mph_reconcile_audio_defaults(&desired, &current, &plan) == MPH_STATUS_OK);
    assert(plan.action_count == 0);

    assert(mph_selection_set_role_device(&desired, MPH_DEVICE_ROLE_DEFAULT_OUTPUT, &speakers) ==
           MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&desired, MPH_DEVICE_ROLE_SYSTEM_OUTPUT,
                                         &display_audio) == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&desired, MPH_DEVICE_ROLE_PREFERRED_CAMERA, &camera) ==
           MPH_STATUS_OK);
    assert(mph_reconcile_audio_defaults(&desired, &current, &plan) == MPH_STATUS_OK);
    assert(plan.action_count == 2);
    assert(plan.actions[0].type == MPH_RECONCILE_ACTION_SET_DEFAULT_OUTPUT);
    assert(plan.actions[1].type == MPH_RECONCILE_ACTION_SET_SYSTEM_OUTPUT);
    assert(mph_device_id_equal(&plan.actions[0].target_device_id, &speakers));
    assert(mph_device_id_equal(&plan.actions[1].target_device_id, &display_audio));
}

static void test_reconcile_airpods_reset(void) {
    mph_device_id_t usb_mic;
    mph_device_id_t airpods_mic;
    assert(mph_device_id_from_parts(&usb_mic, "coreaudio.input", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&airpods_mic, "coreaudio.input", "airpods-mic") ==
           MPH_STATUS_OK);

    mph_selection_t desired;
    mph_selection_t current;
    mph_selection_init(&desired);
    mph_selection_init(&current);
    desired.mode = MPH_ACTIVE_MODE_PROFILE;
    current.mode = MPH_ACTIVE_MODE_NONE;
    assert(mph_selection_set_profile_id(&desired, "studio") == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&desired, MPH_DEVICE_ROLE_DEFAULT_INPUT, &usb_mic) ==
           MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&current, MPH_DEVICE_ROLE_DEFAULT_INPUT, &airpods_mic) ==
           MPH_STATUS_OK);

    mph_device_t mic;
    mph_device_init(&mic);
    mic.id = usb_mic;
    assert(mph_device_set_display_name(&mic, "USB Microphone") == MPH_STATUS_OK);
    mic.category = MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    mic.transport = MPH_DEVICE_TRANSPORT_USB;
    mph_device_set_connection_state(&mic, MPH_DEVICE_CONNECTION_CONNECTED);

    mph_device_list_t *available = mph_device_list_create();
    assert(available != NULL);
    assert(mph_device_list_append(available, &mic) == MPH_STATUS_OK);

    mph_reconcile_state_t state;
    mph_reconcile_state_init(&state);
    mph_reconcile_policy_t policy;
    mph_reconcile_policy_default(&policy);
    policy.debounce_interval_ms = 1000;

    mph_reconcile_context_t context;
    mph_reconcile_context_init(&context);
    context.desired = &desired;
    context.current = &current;
    context.available_devices = available;
    context.now_unix_ms = 10000;

    mph_reconcile_plan_t plan;
    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, &policy, &plan) ==
           MPH_STATUS_OK);
    assert(plan.action_count == 1);
    assert(plan.actions[0].type == MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT);
    assert(mph_device_id_equal(&plan.actions[0].target_device_id, &usb_mic));

    assert(strcmp(desired.profile_id, "studio") == 0);
    assert(desired.mode == MPH_ACTIVE_MODE_PROFILE);

    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, &policy, &plan) ==
           MPH_STATUS_OK);
    assert(plan.action_count == 0);

    context.now_unix_ms = 11000;
    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, &policy, &plan) ==
           MPH_STATUS_OK);
    assert(plan.action_count == 1);
    assert(plan.actions[0].type == MPH_RECONCILE_ACTION_SET_DEFAULT_INPUT);

    mph_device_list_destroy(available);
}

static void test_reconcile_missing_device_retry(void) {
    mph_device_id_t usb_output;
    mph_device_id_t airpods_output;
    assert(mph_device_id_from_parts(&usb_output, "coreaudio.output", "studio-monitors") ==
           MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&airpods_output, "coreaudio.output", "airpods") ==
           MPH_STATUS_OK);

    mph_selection_t desired;
    mph_selection_t current;
    mph_selection_init(&desired);
    mph_selection_init(&current);
    desired.mode = MPH_ACTIVE_MODE_PROFILE;
    assert(mph_selection_set_role_device(&desired, MPH_DEVICE_ROLE_DEFAULT_OUTPUT, &usb_output) ==
           MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&current, MPH_DEVICE_ROLE_DEFAULT_OUTPUT,
                                         &airpods_output) == MPH_STATUS_OK);

    mph_device_list_t *available = mph_device_list_create();
    assert(available != NULL);

    mph_reconcile_state_t state;
    mph_reconcile_state_init(&state);
    mph_reconcile_policy_t policy;
    mph_reconcile_policy_default(&policy);
    policy.retry_initial_delay_ms = 1000;
    policy.retry_max_delay_ms = 4000;
    policy.max_retry_count = 2;

    mph_reconcile_context_t context;
    mph_reconcile_context_init(&context);
    context.desired = &desired;
    context.current = &current;
    context.available_devices = available;
    context.now_unix_ms = 20000;

    mph_reconcile_plan_t plan;
    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, &policy, &plan) ==
           MPH_STATUS_OK);
    assert(plan.action_count == 1);
    assert(plan.actions[0].type == MPH_RECONCILE_ACTION_MARK_MISSING);
    assert(state.roles[MPH_DEVICE_ROLE_DEFAULT_OUTPUT].retry_count == 1);
    assert(mph_reconcile_apply_audio_plan(&plan) == MPH_STATUS_OK);

    context.now_unix_ms = 20500;
    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, &policy, &plan) ==
           MPH_STATUS_OK);
    assert(plan.action_count == 0);

    context.now_unix_ms = 21000;
    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, &policy, &plan) ==
           MPH_STATUS_OK);
    assert(plan.action_count == 1);
    assert(plan.actions[0].type == MPH_RECONCILE_ACTION_MARK_MISSING);
    assert(state.roles[MPH_DEVICE_ROLE_DEFAULT_OUTPUT].retry_count == 2);

    context.now_unix_ms = 23000;
    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, &policy, &plan) ==
           MPH_STATUS_OK);
    assert(plan.action_count == 0);

    mph_device_t output;
    mph_device_init(&output);
    output.id = usb_output;
    output.category = MPH_DEVICE_CATEGORY_AUDIO_OUTPUT;
    mph_device_set_connection_state(&output, MPH_DEVICE_CONNECTION_CONNECTED);
    assert(mph_device_list_append(available, &output) == MPH_STATUS_OK);

    context.now_unix_ms = 24000;
    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, &policy, &plan) ==
           MPH_STATUS_OK);
    assert(plan.action_count == 1);
    assert(plan.actions[0].type == MPH_RECONCILE_ACTION_SET_DEFAULT_OUTPUT);
    assert(state.roles[MPH_DEVICE_ROLE_DEFAULT_OUTPUT].retry_count == 0);

    mph_device_list_destroy(available);
}

static void test_reconcile_manual_override(void) {
    mph_device_id_t headphones;
    mph_device_id_t speakers;
    assert(mph_device_id_from_parts(&headphones, "coreaudio.output", "headphones") ==
           MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&speakers, "coreaudio.output", "speakers") == MPH_STATUS_OK);

    mph_selection_t saved_profile;
    mph_selection_t manual;
    mph_selection_t current;
    mph_selection_init(&saved_profile);
    mph_selection_init(&manual);
    mph_selection_init(&current);
    saved_profile.mode = MPH_ACTIVE_MODE_PROFILE;
    manual.mode = MPH_ACTIVE_MODE_MANUAL;
    assert(mph_selection_set_profile_id(&saved_profile, "studio") == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&saved_profile, MPH_DEVICE_ROLE_DEFAULT_OUTPUT,
                                         &speakers) == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&manual, MPH_DEVICE_ROLE_DEFAULT_OUTPUT, &headphones) ==
           MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&current, MPH_DEVICE_ROLE_DEFAULT_OUTPUT, &speakers) ==
           MPH_STATUS_OK);

    mph_device_t headphones_device;
    mph_device_init(&headphones_device);
    headphones_device.id = headphones;
    headphones_device.category = MPH_DEVICE_CATEGORY_AUDIO_OUTPUT;
    mph_device_set_connection_state(&headphones_device, MPH_DEVICE_CONNECTION_CONNECTED);

    mph_device_list_t *available = mph_device_list_create();
    assert(available != NULL);
    assert(mph_device_list_append(available, &headphones_device) == MPH_STATUS_OK);

    mph_reconcile_state_t state;
    mph_reconcile_state_init(&state);
    mph_reconcile_context_t context;
    mph_reconcile_context_init(&context);
    context.desired = &manual;
    context.current = &current;
    context.available_devices = available;
    context.now_unix_ms = 30000;

    mph_reconcile_plan_t plan;
    assert(mph_reconcile_evaluate_audio_defaults(&context, &state, NULL, &plan) == MPH_STATUS_OK);
    assert(plan.action_count == 1);
    assert(plan.actions[0].type == MPH_RECONCILE_ACTION_SET_DEFAULT_OUTPUT);
    assert(mph_device_id_equal(&plan.actions[0].target_device_id, &headphones));

    const mph_device_id_t *profile_output =
        mph_selection_get_role_device(&saved_profile, MPH_DEVICE_ROLE_DEFAULT_OUTPUT);
    assert(mph_device_id_equal(profile_output, &speakers));
    assert(saved_profile.mode == MPH_ACTIVE_MODE_PROFILE);
    assert(manual.mode == MPH_ACTIVE_MODE_MANUAL);

    mph_device_list_destroy(available);
}

static void test_sqlite_profile_storage(void) {
    char db_path[256];
    snprintf(db_path, sizeof(db_path), "/tmp/mph_core_db_test_%ld.sqlite3", (long)getpid());
    unlink(db_path);

    mph_db_t *db = NULL;
    assert(mph_db_open(&db, db_path) == MPH_STATUS_OK);
    assert(db != NULL);
    assert(mph_db_migrate(db) == MPH_STATUS_OK);
    assert(mph_db_schema_version(db) == 1);
    assert(mph_db_migrate(db) == MPH_STATUS_OK);
    assert(mph_db_schema_version(db) == 1);
    assert(sqlite_scalar_int(db_path,
                             "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND "
                             "name IN ('schema_migrations', 'profiles', "
                             "'profile_device_roles', 'known_devices', 'active_state');") == 5);
    assert(sqlite_scalar_int(db_path, "SELECT COUNT(*) FROM schema_migrations;") == 1);

    mph_device_id_t mic_id;
    mph_device_id_t output_id;
    mph_device_id_t system_output_id;
    assert(mph_device_id_from_parts(&mic_id, "audio", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&output_id, "audio", "studio-monitors") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&system_output_id, "audio", "display-audio") ==
           MPH_STATUS_OK);
    mph_device_id_t camera_id;
    assert(mph_device_id_from_parts(&camera_id, "camera", "logitech-c920") == MPH_STATUS_OK);

    mph_profile_t profile;
    mph_profile_init(&profile);
    assert(mph_profile_configure(&profile, "studio", "Studio") == MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_DEFAULT_INPUT, &mic_id) ==
           MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_DEFAULT_OUTPUT, &output_id) ==
           MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_SYSTEM_OUTPUT,
                                       &system_output_id) == MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_PREFERRED_CAMERA, &camera_id) ==
           MPH_STATUS_OK);
    assert(mph_db_save_profile(db, &profile) == MPH_STATUS_OK);
    assert(sqlite_scalar_int(db_path,
                             "SELECT COUNT(*) FROM profile_device_roles "
                             "WHERE profile_id = 'studio';") == 4);

    mph_profile_t loaded;
    bool found = false;
    assert(mph_db_load_profile(db, "studio", &loaded, &found) == MPH_STATUS_OK);
    assert(found);
    assert(strcmp(loaded.id, "studio") == 0);
    assert(strcmp(loaded.name, "Studio") == 0);
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded.selection, MPH_DEVICE_ROLE_DEFAULT_INPUT), &mic_id));
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded.selection, MPH_DEVICE_ROLE_DEFAULT_OUTPUT),
        &output_id));
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded.selection, MPH_DEVICE_ROLE_SYSTEM_OUTPUT),
        &system_output_id));
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded.selection, MPH_DEVICE_ROLE_PREFERRED_CAMERA),
        &camera_id));

    mph_device_id_t empty_id;
    mph_device_id_clear(&empty_id);
    assert(mph_profile_set_name(&profile, "Studio Updated") == MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_PREFERRED_CAMERA, &empty_id) ==
           MPH_STATUS_OK);
    assert(mph_db_save_profile(db, &profile) == MPH_STATUS_OK);
    assert(sqlite_scalar_int(db_path,
                             "SELECT COUNT(*) FROM profile_device_roles "
                             "WHERE profile_id = 'studio';") == 3);
    assert(mph_db_load_profile(db, "studio", &loaded, &found) == MPH_STATUS_OK);
    assert(found);
    assert(strcmp(loaded.name, "Studio Updated") == 0);
    assert(mph_device_id_is_empty(
        mph_selection_get_role_device(&loaded.selection, MPH_DEVICE_ROLE_PREFERRED_CAMERA)));

    mph_profile_store_t *store = mph_profile_store_create();
    assert(store != NULL);
    assert(mph_db_load_profiles(db, store) == MPH_STATUS_OK);
    assert(mph_profile_store_count(store) == 1);
    mph_profile_store_destroy(store);

    mph_selection_t active_profile;
    mph_selection_init(&active_profile);
    active_profile.mode = MPH_ACTIVE_MODE_PROFILE;
    active_profile.enforce_audio_defaults = false;
    assert(mph_selection_set_profile_id(&active_profile, "studio") == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&active_profile, MPH_DEVICE_ROLE_DEFAULT_INPUT,
                                         &mic_id) == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&active_profile, MPH_DEVICE_ROLE_DEFAULT_OUTPUT,
                                         &output_id) == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&active_profile, MPH_DEVICE_ROLE_SYSTEM_OUTPUT,
                                         &system_output_id) == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&active_profile, MPH_DEVICE_ROLE_PREFERRED_CAMERA,
                                         &camera_id) == MPH_STATUS_OK);
    assert(mph_db_save_active_selection(db, &active_profile) == MPH_STATUS_OK);

    mph_selection_t loaded_active_profile;
    found = false;
    assert(mph_db_load_active_selection(db, &loaded_active_profile, &found) == MPH_STATUS_OK);
    assert(found);
    assert(loaded_active_profile.mode == MPH_ACTIVE_MODE_PROFILE);
    assert(!loaded_active_profile.enforce_audio_defaults);
    assert(strcmp(loaded_active_profile.profile_id, "studio") == 0);
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded_active_profile, MPH_DEVICE_ROLE_DEFAULT_INPUT),
        &mic_id));
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded_active_profile, MPH_DEVICE_ROLE_DEFAULT_OUTPUT),
        &output_id));
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded_active_profile, MPH_DEVICE_ROLE_SYSTEM_OUTPUT),
        &system_output_id));
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded_active_profile, MPH_DEVICE_ROLE_PREFERRED_CAMERA),
        &camera_id));

    mph_selection_t manual;
    mph_selection_init(&manual);
    manual.mode = MPH_ACTIVE_MODE_MANUAL;
    assert(mph_selection_set_role_device(&manual, MPH_DEVICE_ROLE_DEFAULT_INPUT, &mic_id) ==
           MPH_STATUS_OK);
    assert(mph_db_save_active_selection(db, &manual) == MPH_STATUS_OK);
    assert(mph_selection_set_role_device(&manual, MPH_DEVICE_ROLE_PREFERRED_CAMERA, &camera_id) ==
           MPH_STATUS_OK);
    assert(mph_db_save_active_selection(db, &manual) == MPH_STATUS_OK);

    mph_selection_t loaded_manual;
    found = false;
    assert(mph_db_load_active_selection(db, &loaded_manual, &found) == MPH_STATUS_OK);
    assert(found);
    assert(loaded_manual.mode == MPH_ACTIVE_MODE_MANUAL);
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded_manual, MPH_DEVICE_ROLE_DEFAULT_INPUT), &mic_id));
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded_manual, MPH_DEVICE_ROLE_PREFERRED_CAMERA),
        &camera_id));

    mph_device_t device;
    mph_device_init(&device);
    assert(mph_device_id_from_parts(&device.id, "audio", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&device, "USB Microphone") == MPH_STATUS_OK);
    assert(mph_device_set_vendor_name(&device, "Test Vendor") == MPH_STATUS_OK);
    assert(mph_device_set_model_name(&device, "MV7") == MPH_STATUS_OK);
    assert(mph_device_set_serial_number(&device, "SERIAL-USB-MIC") == MPH_STATUS_OK);
    device.category = MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    device.transport = MPH_DEVICE_TRANSPORT_USB;
    mph_device_set_connection_state(&device, MPH_DEVICE_CONNECTION_CONNECTED);
    assert(mph_db_save_known_device(db, &device) == MPH_STATUS_OK);

    mph_device_list_t *loaded_devices = mph_device_list_create();
    assert(loaded_devices != NULL);
    assert(mph_db_load_known_devices(db, loaded_devices) == MPH_STATUS_OK);
    const mph_device_t *loaded_device = mph_device_list_find_by_id(loaded_devices, &device.id);
    assert(loaded_device != NULL);
    assert(strcmp(loaded_device->display_name, "USB Microphone") == 0);
    assert(strcmp(loaded_device->vendor_name, "Test Vendor") == 0);
    assert(strcmp(loaded_device->model_name, "MV7") == 0);
    assert(strcmp(loaded_device->serial_number, "SERIAL-USB-MIC") == 0);
    assert(loaded_device->category == MPH_DEVICE_CATEGORY_AUDIO_INPUT);
    assert(loaded_device->transport == MPH_DEVICE_TRANSPORT_USB);
    assert(loaded_device->connection_state == MPH_DEVICE_CONNECTION_CONNECTED);
    mph_device_list_destroy(loaded_devices);

    assert(mph_db_delete_profile(db, "studio") == MPH_STATUS_OK);
    found = true;
    assert(mph_db_load_profile(db, "studio", &loaded, &found) == MPH_STATUS_OK);
    assert(!found);
    assert(sqlite_scalar_int(db_path,
                             "SELECT COUNT(*) FROM profile_device_roles "
                             "WHERE profile_id = 'studio';") == 0);
    assert(mph_db_delete_profile(db, "studio") == MPH_STATUS_NOT_FOUND);

    mph_db_close(db);
    unlink(db_path);
}

int main(void) {
    assert(strcmp(mph_core_version(), "1.0.0") == 0);
    test_device_list();
    test_swift_bridge_cstrings();
    test_device_modeling_from_fixture();
    test_device_normalization_and_matching();
    test_serialization_names_roundtrip();
    test_core_audio_mapper();
    test_display_mapper();
    test_camera_mapper();
    test_hid_mapper();
    test_usb_mapper_and_dedup();
    test_bluetooth_mapper_and_merge();
    test_inventory_snapshot_and_storage();
    test_core_audio_live_smoke();
    test_display_live_smoke();
    test_camera_live_smoke();
    test_hid_live_smoke();
    test_usb_live_smoke();
    test_bluetooth_live_smoke();
    test_inventory_live_smoke();
    test_audio_watcher_live_smoke();
    test_profile_store();
    test_reconcile();
    test_reconcile_airpods_reset();
    test_reconcile_missing_device_retry();
    test_reconcile_manual_override();
    test_sqlite_profile_storage();

    printf("PeripheralCore smoke tests passed\n");
    return 0;
}
