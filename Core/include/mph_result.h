#ifndef MPH_RESULT_H
#define MPH_RESULT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPH_ERROR_MESSAGE_CAPACITY 256

typedef enum {
    MPH_STATUS_OK = 0,
    MPH_STATUS_INVALID_ARGUMENT,
    MPH_STATUS_NOT_FOUND,
    MPH_STATUS_NO_MEMORY,
    MPH_STATUS_CAPACITY_EXCEEDED,
    MPH_STATUS_CONFLICT,
    MPH_STATUS_INTERNAL_ERROR
} mph_status_t;

typedef struct {
    mph_status_t status;
    char message[MPH_ERROR_MESSAGE_CAPACITY];
} mph_error_t;

const char *mph_status_name(mph_status_t status);
bool mph_status_is_ok(mph_status_t status);
void mph_error_init(mph_error_t *error);
void mph_error_clear(mph_error_t *error);
void mph_error_set(mph_error_t *error, mph_status_t status, const char *message);

#ifdef __cplusplus
}
#endif

#endif
