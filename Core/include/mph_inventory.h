#ifndef MPH_INVENTORY_H
#define MPH_INVENTORY_H

#include "mph_db.h"
#include "mph_device.h"
#include "mph_device_list.h"
#include "mph_result.h"

#ifdef __cplusplus
extern "C" {
#endif

mph_status_t mph_inventory_merge_device(mph_device_list_t *inventory,
                                        const mph_device_t *candidate_device);
void mph_inventory_sort(mph_device_list_t *inventory);
mph_status_t mph_inventory_store_known_devices(mph_db_t *db, const mph_device_list_t *inventory);
mph_status_t mph_inventory_collect(mph_device_list_t *out_inventory);
mph_status_t mph_inventory_collect_and_store(mph_db_t *db, mph_device_list_t *out_inventory);

#ifdef __cplusplus
}
#endif

#endif
