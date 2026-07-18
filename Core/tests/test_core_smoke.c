#include "PeripheralCore.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void test_device_list(void) {
    mph_device_t device;
    mph_device_init(&device);

    assert(mph_device_id_from_parts(&device.id, "audio", "usb-mic") == MPH_STATUS_OK);
    assert(mph_device_set_display_name(&device, "USB Microphone") == MPH_STATUS_OK);
    device.category = MPH_DEVICE_CATEGORY_AUDIO_INPUT;
    device.transport = MPH_DEVICE_TRANSPORT_USB;
    device.is_connected = true;
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
        mph_selection_get_role_device(&stored->selection, MPH_DEVICE_ROLE_DEFAULT_INPUT),
        &mic_id));

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

int main(void) {
    assert(strcmp(mph_core_version(), "1.0.0") == 0);
    test_device_list();
    test_profile_store();
    test_reconcile();

    printf("PeripheralCore smoke tests passed\n");
    return 0;
}
