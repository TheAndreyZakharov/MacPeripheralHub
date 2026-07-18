#include "mph_profile_store.h"

#include <stdlib.h>
#include <string.h>

struct mph_profile_store {
    mph_profile_t *items;
    size_t count;
    size_t capacity;
};

static mph_status_t reserve_profiles(mph_profile_store_t *store, size_t desired_capacity) {
    if (store == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    if (desired_capacity <= store->capacity) {
        return MPH_STATUS_OK;
    }

    size_t next_capacity = store->capacity == 0 ? 4 : store->capacity;
    while (next_capacity < desired_capacity) {
        next_capacity *= 2;
    }

    mph_profile_t *next_items = realloc(store->items, next_capacity * sizeof(*next_items));
    if (next_items == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    store->items = next_items;
    store->capacity = next_capacity;
    return MPH_STATUS_OK;
}

static size_t find_profile_index(const mph_profile_store_t *store, const char *profile_id,
                                 bool *found) {
    if (found != NULL) {
        *found = false;
    }

    if (store == NULL || profile_id == NULL) {
        return 0;
    }

    for (size_t index = 0; index < store->count; index += 1) {
        if (strcmp(store->items[index].id, profile_id) == 0) {
            if (found != NULL) {
                *found = true;
            }
            return index;
        }
    }

    return 0;
}

mph_profile_store_t *mph_profile_store_create(void) {
    return calloc(1, sizeof(mph_profile_store_t));
}

void mph_profile_store_destroy(mph_profile_store_t *store) {
    if (store == NULL) {
        return;
    }

    free(store->items);
    free(store);
}

mph_status_t mph_profile_store_save(mph_profile_store_t *store, const mph_profile_t *profile) {
    if (store == NULL || !mph_profile_is_valid(profile)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    bool found = false;
    size_t index = find_profile_index(store, profile->id, &found);
    if (found) {
        store->items[index] = *profile;
        return MPH_STATUS_OK;
    }

    mph_status_t status = reserve_profiles(store, store->count + 1);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    store->items[store->count] = *profile;
    store->count += 1;
    return MPH_STATUS_OK;
}

mph_status_t mph_profile_store_delete(mph_profile_store_t *store, const char *profile_id) {
    bool found = false;
    size_t index = find_profile_index(store, profile_id, &found);
    if (!found) {
        return MPH_STATUS_NOT_FOUND;
    }

    for (size_t cursor = index + 1; cursor < store->count; cursor += 1) {
        store->items[cursor - 1] = store->items[cursor];
    }
    store->count -= 1;
    return MPH_STATUS_OK;
}

size_t mph_profile_store_count(const mph_profile_store_t *store) {
    return store != NULL ? store->count : 0;
}

const mph_profile_t *mph_profile_store_get(const mph_profile_store_t *store, size_t index) {
    if (store == NULL || index >= store->count) {
        return NULL;
    }

    return &store->items[index];
}

const mph_profile_t *mph_profile_store_find(const mph_profile_store_t *store,
                                            const char *profile_id) {
    bool found = false;
    size_t index = find_profile_index(store, profile_id, &found);
    return found ? &store->items[index] : NULL;
}
