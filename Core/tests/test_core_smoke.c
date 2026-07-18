#include "PeripheralCore.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FIXTURE_FIELD_COUNT 12

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

static size_t split_fixture_line(char *line, char *fields[FIXTURE_FIELD_COUNT]) {
    size_t count = 0;
    char *field_start = line;

    for (char *cursor = line; *cursor != '\0'; cursor += 1) {
        if (*cursor == '|' || *cursor == '\n' || *cursor == '\r') {
            *cursor = '\0';
            if (count < FIXTURE_FIELD_COUNT) {
                fields[count] = field_start;
                count += 1;
            }
            field_start = cursor + 1;
        }
    }

    if (count < FIXTURE_FIELD_COUNT && field_start[0] != '\0') {
        fields[count] = field_start;
        count += 1;
    }

    return count;
}

static uint32_t fixture_u32(const char *value) {
    return value != NULL && value[0] != '\0' ? (uint32_t)strtoul(value, NULL, 10) : 0;
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

static void test_profile_store(void) {
    mph_device_id_t mic_id;
    assert(mph_device_id_from_parts(&mic_id, "audio", "usb-mic") == MPH_STATUS_OK);

    mph_profile_t profile;
    mph_profile_init(&profile);
    assert(mph_profile_configure(&profile, "studio", "Studio") == MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_DEFAULT_INPUT, &mic_id) ==
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

    assert(mph_profile_store_delete(store, "studio") == MPH_STATUS_OK);
    assert(mph_profile_store_count(store) == 0);
    mph_profile_store_destroy(store);
}

static void test_reconcile(void) {
    mph_device_id_t usb_mic;
    mph_device_id_t airpods_mic;
    assert(mph_device_id_from_parts(&usb_mic, "audio", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&airpods_mic, "audio", "airpods-mic") == MPH_STATUS_OK);

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

    mph_device_id_t mic_id;
    mph_device_id_t output_id;
    assert(mph_device_id_from_parts(&mic_id, "audio", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_id_from_parts(&output_id, "audio", "studio-monitors") == MPH_STATUS_OK);

    mph_profile_t profile;
    mph_profile_init(&profile);
    assert(mph_profile_configure(&profile, "studio", "Studio") == MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_DEFAULT_INPUT, &mic_id) ==
           MPH_STATUS_OK);
    assert(mph_profile_set_role_device(&profile, MPH_DEVICE_ROLE_DEFAULT_OUTPUT, &output_id) ==
           MPH_STATUS_OK);
    assert(mph_db_save_profile(db, &profile) == MPH_STATUS_OK);

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

    mph_profile_store_t *store = mph_profile_store_create();
    assert(store != NULL);
    assert(mph_db_load_profiles(db, store) == MPH_STATUS_OK);
    assert(mph_profile_store_count(store) == 1);
    mph_profile_store_destroy(store);

    mph_selection_t manual;
    mph_selection_init(&manual);
    manual.mode = MPH_ACTIVE_MODE_MANUAL;
    assert(mph_selection_set_role_device(&manual, MPH_DEVICE_ROLE_DEFAULT_INPUT, &mic_id) ==
           MPH_STATUS_OK);
    assert(mph_db_save_active_selection(db, &manual) == MPH_STATUS_OK);

    mph_selection_t loaded_manual;
    found = false;
    assert(mph_db_load_active_selection(db, &loaded_manual, &found) == MPH_STATUS_OK);
    assert(found);
    assert(loaded_manual.mode == MPH_ACTIVE_MODE_MANUAL);
    assert(mph_device_id_equal(
        mph_selection_get_role_device(&loaded_manual, MPH_DEVICE_ROLE_DEFAULT_INPUT), &mic_id));

    mph_device_t device;
    mph_device_init(&device);
    assert(mph_device_id_from_parts(&device.id, "audio", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&device, "USB Microphone") == MPH_STATUS_OK);
    assert(mph_device_set_vendor_name(&device, "Test Vendor") == MPH_STATUS_OK);
    device.category = MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    device.transport = MPH_DEVICE_TRANSPORT_USB;
    device.is_connected = true;
    assert(mph_db_save_known_device(db, &device) == MPH_STATUS_OK);

    assert(mph_db_delete_profile(db, "studio") == MPH_STATUS_OK);
    found = true;
    assert(mph_db_load_profile(db, "studio", &loaded, &found) == MPH_STATUS_OK);
    assert(!found);

    mph_db_close(db);
    unlink(db_path);
}

int main(void) {
    assert(strcmp(mph_core_version(), "1.0.0") == 0);
    test_device_list();
    test_device_modeling_from_fixture();
    test_device_normalization_and_matching();
    test_core_audio_mapper();
    test_core_audio_live_smoke();
    test_profile_store();
    test_reconcile();
    test_sqlite_profile_storage();

    printf("PeripheralCore smoke tests passed\n");
    return 0;
}
