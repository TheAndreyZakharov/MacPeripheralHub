#include "mph_device_list.h"

#include <stdlib.h>

struct mph_device_list {
    mph_device_t *items;
    size_t count;
    size_t capacity;
};

static mph_status_t reserve_devices(mph_device_list_t *list, size_t desired_capacity) {
    if (list == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    if (desired_capacity <= list->capacity) {
        return MPH_STATUS_OK;
    }

    size_t next_capacity = list->capacity == 0 ? 8 : list->capacity;
    while (next_capacity < desired_capacity) {
        next_capacity *= 2;
    }

    mph_device_t *next_items = realloc(list->items, next_capacity * sizeof(*next_items));
    if (next_items == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    list->items = next_items;
    list->capacity = next_capacity;
    return MPH_STATUS_OK;
}

mph_device_list_t *mph_device_list_create(void) {
    return calloc(1, sizeof(mph_device_list_t));
}

void mph_device_list_destroy(mph_device_list_t *list) {
    if (list == NULL) {
        return;
    }

    free(list->items);
    free(list);
}

mph_status_t mph_device_list_append(mph_device_list_t *list, const mph_device_t *device) {
    if (list == NULL || device == NULL || mph_device_id_is_empty(&device->id)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = reserve_devices(list, list->count + 1);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    list->items[list->count] = *device;
    list->count += 1;
    return MPH_STATUS_OK;
}

size_t mph_device_list_count(const mph_device_list_t *list) {
    return list != NULL ? list->count : 0;
}

const mph_device_t *mph_device_list_get(const mph_device_list_t *list, size_t index) {
    if (list == NULL || index >= list->count) {
        return NULL;
    }

    return &list->items[index];
}

mph_device_t *mph_device_list_get_mutable(mph_device_list_t *list, size_t index) {
    if (list == NULL || index >= list->count) {
        return NULL;
    }

    return &list->items[index];
}

mph_status_t mph_device_list_replace_at(mph_device_list_t *list, size_t index,
                                        const mph_device_t *device) {
    if (list == NULL || device == NULL || index >= list->count ||
        mph_device_id_is_empty(&device->id)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    list->items[index] = *device;
    return MPH_STATUS_OK;
}

const mph_device_t *mph_device_list_find_by_id(const mph_device_list_t *list,
                                               const mph_device_id_t *device_id) {
    if (list == NULL || mph_device_id_is_empty(device_id)) {
        return NULL;
    }

    for (size_t index = 0; index < list->count; index += 1) {
        if (mph_device_id_equal(&list->items[index].id, device_id)) {
            return &list->items[index];
        }
    }

    return NULL;
}
