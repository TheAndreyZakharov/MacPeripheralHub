#ifndef MPH_DEVICE_LIST_H
#define MPH_DEVICE_LIST_H

#include "mph_device.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mph_device_list mph_device_list_t;

mph_device_list_t *mph_device_list_create(void);
void mph_device_list_destroy(mph_device_list_t *list);
mph_status_t mph_device_list_append(mph_device_list_t *list, const mph_device_t *device);
size_t mph_device_list_count(const mph_device_list_t *list);
const mph_device_t *mph_device_list_get(const mph_device_list_t *list, size_t index);
mph_device_t *mph_device_list_get_mutable(mph_device_list_t *list, size_t index);
mph_status_t mph_device_list_replace_at(mph_device_list_t *list, size_t index,
                                        const mph_device_t *device);
const mph_device_t *mph_device_list_find_by_id(const mph_device_list_t *list,
                                               const mph_device_id_t *device_id);

#ifdef __cplusplus
}
#endif

#endif
