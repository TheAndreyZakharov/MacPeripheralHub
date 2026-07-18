#ifndef MPH_DB_H
#define MPH_DB_H

#include "mph_device.h"
#include "mph_profile.h"
#include "mph_profile_store.h"
#include "mph_selection.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_DB_PATH_CAPACITY 1024

typedef struct mph_db mph_db_t;

mph_status_t mph_db_default_path(char *buffer, size_t capacity);
mph_status_t mph_db_open(mph_db_t **out_db, const char *path);
mph_status_t mph_db_open_application_support(mph_db_t **out_db);
void mph_db_close(mph_db_t *db);
const char *mph_db_path(const mph_db_t *db);
mph_status_t mph_db_migrate(mph_db_t *db);
int mph_db_schema_version(const mph_db_t *db);

mph_status_t mph_db_save_profile(mph_db_t *db, const mph_profile_t *profile);
mph_status_t mph_db_load_profile(mph_db_t *db, const char *profile_id, mph_profile_t *out_profile,
                                 bool *out_found);
mph_status_t mph_db_load_profiles(mph_db_t *db, mph_profile_store_t *store);
mph_status_t mph_db_delete_profile(mph_db_t *db, const char *profile_id);

mph_status_t mph_db_save_known_device(mph_db_t *db, const mph_device_t *device);
mph_status_t mph_db_save_active_selection(mph_db_t *db, const mph_selection_t *selection);
mph_status_t mph_db_load_active_selection(mph_db_t *db, mph_selection_t *out_selection,
                                          bool *out_found);

#ifdef __cplusplus
}
#endif

#endif
