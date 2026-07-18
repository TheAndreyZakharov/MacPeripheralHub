#ifndef MPH_PROFILE_STORE_H
#define MPH_PROFILE_STORE_H

#include "mph_profile.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mph_profile_store mph_profile_store_t;

mph_profile_store_t *mph_profile_store_create(void);
void mph_profile_store_destroy(mph_profile_store_t *store);
mph_status_t mph_profile_store_save(mph_profile_store_t *store, const mph_profile_t *profile);
mph_status_t mph_profile_store_delete(mph_profile_store_t *store, const char *profile_id);
size_t mph_profile_store_count(const mph_profile_store_t *store);
const mph_profile_t *mph_profile_store_get(const mph_profile_store_t *store, size_t index);
const mph_profile_t *mph_profile_store_find(const mph_profile_store_t *store,
                                            const char *profile_id);

#ifdef __cplusplus
}
#endif

#endif
