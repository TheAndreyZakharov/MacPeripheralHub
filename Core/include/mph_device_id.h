#ifndef MPH_DEVICE_ID_H
#define MPH_DEVICE_ID_H

#include "mph_result.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_DEVICE_ID_CAPACITY 128

typedef struct {
    char value[MPH_DEVICE_ID_CAPACITY];
} mph_device_id_t;

void mph_device_id_clear(mph_device_id_t *device_id);
mph_status_t mph_device_id_set(mph_device_id_t *device_id, const char *value);
mph_status_t mph_device_id_from_parts(mph_device_id_t *device_id, const char *namespace_name,
                                      const char *stable_value);
bool mph_device_id_is_empty(const mph_device_id_t *device_id);
bool mph_device_id_equal(const mph_device_id_t *left, const mph_device_id_t *right);
const char *mph_device_id_cstr(const mph_device_id_t *device_id);

#ifdef __cplusplus
}
#endif

#endif
