#include "mph_inventory.h"

#include "mph_bluetooth.h"
#include "mph_camera.h"
#include "mph_core_audio.h"
#include "mph_display.h"
#include "mph_hid.h"
#include "mph_usb.h"

#include <stdio.h>
#include <string.h>

static bool text_is_empty(const char *value) {
    return value == NULL || value[0] == '\0';
}

static bool text_equal(const char *left, const char *right) {
    return !text_is_empty(left) && !text_is_empty(right) && strcmp(left, right) == 0;
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

static bool hid_ids_equal(const mph_device_t *left, const mph_device_t *right) {
    return left->hid.vendor_id != 0 && left->hid.product_id != 0 &&
           left->hid.vendor_id == right->hid.vendor_id &&
           left->hid.product_id == right->hid.product_id &&
           left->hid.usage_page == right->hid.usage_page && left->hid.usage == right->hid.usage;
}

static bool usb_ids_equal(const mph_device_t *left, const mph_device_t *right) {
    return left->usb.vendor_id != 0 && left->usb.product_id != 0 &&
           left->usb.vendor_id == right->usb.vendor_id &&
           left->usb.product_id == right->usb.product_id;
}

static bool usb_matches_hid(const mph_device_t *left, const mph_device_t *right) {
    return left->usb.vendor_id != 0 && left->usb.product_id != 0 &&
           left->usb.vendor_id == right->hid.vendor_id &&
           left->usb.product_id == right->hid.product_id;
}

static bool usb_or_bluetooth_shell_category(mph_device_category_t category) {
    return category == MPH_DEVICE_CATEGORY_USB || category == MPH_DEVICE_CATEGORY_BLUETOOTH;
}

static bool linkable_peripheral_category(mph_device_category_t category) {
    return mph_device_category_is_audio(category) || category == MPH_DEVICE_CATEGORY_CAMERA ||
           category == MPH_DEVICE_CATEGORY_KEYBOARD || category == MPH_DEVICE_CATEGORY_MOUSE ||
           category == MPH_DEVICE_CATEGORY_TRACKPAD || category == MPH_DEVICE_CATEGORY_USB ||
           category == MPH_DEVICE_CATEGORY_BLUETOOTH;
}

static bool transport_shell_matches_named_device(const mph_device_t *left,
                                                 const mph_device_t *right,
                                                 mph_device_transport_t transport) {
    if (left->transport != transport || right->transport != transport ||
        !normalized_text_equal(left->display_name, right->display_name)) {
        return false;
    }

    return (usb_or_bluetooth_shell_category(left->category) &&
            linkable_peripheral_category(right->category)) ||
           (usb_or_bluetooth_shell_category(right->category) &&
            linkable_peripheral_category(left->category));
}

static bool inventory_devices_match(const mph_device_t *existing, const mph_device_t *candidate) {
    if (existing == NULL || candidate == NULL) {
        return false;
    }

    if (!mph_device_id_is_empty(&existing->id) &&
        mph_device_id_equal(&existing->id, &candidate->id)) {
        return true;
    }

    if (text_equal(existing->serial_number, candidate->serial_number) ||
        text_equal(existing->camera.unique_id, candidate->camera.unique_id) ||
        text_equal(existing->bluetooth.address, candidate->bluetooth.address)) {
        return true;
    }

    if (hid_ids_equal(existing, candidate) || usb_ids_equal(existing, candidate) ||
        usb_matches_hid(existing, candidate) || usb_matches_hid(candidate, existing)) {
        return true;
    }

    if (mph_device_is_probable_match(existing, candidate)) {
        return true;
    }

    return transport_shell_matches_named_device(existing, candidate, MPH_DEVICE_TRANSPORT_USB) ||
           transport_shell_matches_named_device(existing, candidate,
                                                MPH_DEVICE_TRANSPORT_BLUETOOTH);
}

static void copy_missing_text(char *destination, size_t capacity, const char *source) {
    if (destination == NULL || capacity == 0 || !text_is_empty(destination) ||
        text_is_empty(source)) {
        return;
    }

    snprintf(destination, capacity, "%s", source);
}

static void merge_device_metadata(mph_device_t *existing, const mph_device_t *candidate) {
    if (existing == NULL || candidate == NULL) {
        return;
    }

    if (existing->category == MPH_DEVICE_CATEGORY_UNKNOWN &&
        candidate->category != MPH_DEVICE_CATEGORY_UNKNOWN) {
        existing->category = candidate->category;
    }
    if (existing->transport == MPH_DEVICE_TRANSPORT_UNKNOWN &&
        candidate->transport != MPH_DEVICE_TRANSPORT_UNKNOWN) {
        existing->transport = candidate->transport;
    }

    copy_missing_text(existing->display_name, sizeof(existing->display_name),
                      candidate->display_name);
    copy_missing_text(existing->vendor_name, sizeof(existing->vendor_name), candidate->vendor_name);
    copy_missing_text(existing->model_name, sizeof(existing->model_name), candidate->model_name);
    copy_missing_text(existing->serial_number, sizeof(existing->serial_number),
                      candidate->serial_number);
    copy_missing_text(existing->camera.unique_id, sizeof(existing->camera.unique_id),
                      candidate->camera.unique_id);
    copy_missing_text(existing->bluetooth.address, sizeof(existing->bluetooth.address),
                      candidate->bluetooth.address);

    if (candidate->connection_state == MPH_DEVICE_CONNECTION_CONNECTED) {
        mph_device_set_connection_state(existing, MPH_DEVICE_CONNECTION_CONNECTED);
    } else if (existing->connection_state == MPH_DEVICE_CONNECTION_UNKNOWN) {
        mph_device_set_connection_state(existing, candidate->connection_state);
    }

    if (existing->audio.sample_rate_hz == 0.0) {
        existing->audio.sample_rate_hz = candidate->audio.sample_rate_hz;
    }
    if (existing->audio.channel_count == 0) {
        existing->audio.channel_count = candidate->audio.channel_count;
    }
    existing->audio.is_default_input =
        existing->audio.is_default_input || candidate->audio.is_default_input;
    existing->audio.is_default_output =
        existing->audio.is_default_output || candidate->audio.is_default_output;
    existing->audio.is_default_system_output =
        existing->audio.is_default_system_output || candidate->audio.is_default_system_output;

    if (existing->display.width_px == 0) {
        existing->display.width_px = candidate->display.width_px;
    }
    if (existing->display.height_px == 0) {
        existing->display.height_px = candidate->display.height_px;
    }
    if (existing->display.refresh_rate_hz == 0.0) {
        existing->display.refresh_rate_hz = candidate->display.refresh_rate_hz;
    }
    existing->display.is_main = existing->display.is_main || candidate->display.is_main;
    existing->camera.supports_global_default =
        existing->camera.supports_global_default || candidate->camera.supports_global_default;

    if (existing->hid.vendor_id == 0) {
        existing->hid.vendor_id = candidate->hid.vendor_id;
    }
    if (existing->hid.product_id == 0) {
        existing->hid.product_id = candidate->hid.product_id;
    }
    if (existing->hid.usage_page == 0) {
        existing->hid.usage_page = candidate->hid.usage_page;
    }
    if (existing->hid.usage == 0) {
        existing->hid.usage = candidate->hid.usage;
    }

    if (existing->usb.vendor_id == 0) {
        existing->usb.vendor_id = candidate->usb.vendor_id;
    }
    if (existing->usb.product_id == 0) {
        existing->usb.product_id = candidate->usb.product_id;
    }
    if (existing->usb.speed_mbps == 0) {
        existing->usb.speed_mbps = candidate->usb.speed_mbps;
    }
    if (existing->usb.power_ma == 0) {
        existing->usb.power_ma = candidate->usb.power_ma;
    }

    existing->bluetooth.is_paired = existing->bluetooth.is_paired || candidate->bluetooth.is_paired;
    existing->bluetooth.is_connected =
        existing->bluetooth.is_connected || candidate->bluetooth.is_connected;
    if (existing->bluetooth.class_of_device == 0) {
        existing->bluetooth.class_of_device = candidate->bluetooth.class_of_device;
    }
}

mph_status_t mph_inventory_merge_device(mph_device_list_t *inventory,
                                        const mph_device_t *candidate_device) {
    if (inventory == NULL || candidate_device == NULL ||
        mph_device_id_is_empty(&candidate_device->id)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < mph_device_list_count(inventory); index += 1) {
        mph_device_t *existing = mph_device_list_get_mutable(inventory, index);
        if (inventory_devices_match(existing, candidate_device)) {
            merge_device_metadata(existing, candidate_device);
            return MPH_STATUS_OK;
        }
    }

    return mph_device_list_append(inventory, candidate_device);
}

static int compare_normalized_text(const char *left, const char *right) {
    char normalized_left[MPH_DEVICE_TEXT_CAPACITY];
    char normalized_right[MPH_DEVICE_TEXT_CAPACITY];
    mph_device_normalize_name(left, normalized_left, sizeof(normalized_left));
    mph_device_normalize_name(right, normalized_right, sizeof(normalized_right));
    return strcmp(normalized_left, normalized_right);
}

static int compare_devices(const mph_device_t *left, const mph_device_t *right) {
    if (left->category != right->category) {
        return left->category < right->category ? -1 : 1;
    }

    int name_order = compare_normalized_text(left->display_name, right->display_name);
    if (name_order != 0) {
        return name_order;
    }

    return strcmp(mph_device_id_cstr(&left->id), mph_device_id_cstr(&right->id));
}

void mph_inventory_sort(mph_device_list_t *inventory) {
    if (inventory == NULL) {
        return;
    }

    size_t count = mph_device_list_count(inventory);
    for (size_t left_index = 0; left_index < count; left_index += 1) {
        for (size_t right_index = left_index + 1; right_index < count; right_index += 1) {
            mph_device_t *left = mph_device_list_get_mutable(inventory, left_index);
            mph_device_t *right = mph_device_list_get_mutable(inventory, right_index);
            if (left != NULL && right != NULL && compare_devices(left, right) > 0) {
                mph_device_t temporary = *left;
                *left = *right;
                *right = temporary;
            }
        }
    }
}

static mph_status_t merge_source_devices(mph_device_list_t *inventory,
                                         mph_status_t (*enumerate)(mph_device_list_t *)) {
    mph_device_list_t *source = mph_device_list_create();
    if (source == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    mph_status_t status = enumerate(source);
    for (size_t index = 0; mph_status_is_ok(status) && index < mph_device_list_count(source);
         index += 1) {
        const mph_device_t *device = mph_device_list_get(source, index);
        status = mph_inventory_merge_device(inventory, device);
    }

    mph_device_list_destroy(source);
    return status;
}

mph_status_t mph_inventory_store_known_devices(mph_db_t *db, const mph_device_list_t *inventory) {
    if (db == NULL || inventory == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = mph_db_mark_known_devices_disconnected(db);
    for (size_t index = 0; mph_status_is_ok(status) && index < mph_device_list_count(inventory);
         index += 1) {
        const mph_device_t *device = mph_device_list_get(inventory, index);
        status = mph_db_save_known_device(db, device);
    }

    return status;
}

mph_status_t mph_inventory_collect(mph_device_list_t *out_inventory) {
    if (out_inventory == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = merge_source_devices(out_inventory, mph_core_audio_enumerate_devices);
    if (mph_status_is_ok(status)) {
        status = merge_source_devices(out_inventory, mph_display_enumerate_devices);
    }
    if (mph_status_is_ok(status)) {
        status = merge_source_devices(out_inventory, mph_camera_enumerate_devices);
    }
    if (mph_status_is_ok(status)) {
        status = merge_source_devices(out_inventory, mph_hid_enumerate_devices);
    }
    if (mph_status_is_ok(status)) {
        status = merge_source_devices(out_inventory, mph_usb_enumerate_devices);
    }
    if (mph_status_is_ok(status)) {
        status = merge_source_devices(out_inventory, mph_bluetooth_enumerate_devices);
    }
    if (mph_status_is_ok(status)) {
        mph_inventory_sort(out_inventory);
    }

    return status;
}

mph_status_t mph_inventory_collect_and_store(mph_db_t *db, mph_device_list_t *out_inventory) {
    if (db == NULL || out_inventory == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = mph_inventory_collect(out_inventory);
    if (mph_status_is_ok(status)) {
        status = mph_inventory_store_known_devices(db, out_inventory);
    }

    return status;
}
