#include "mph_result.h"

#include <stdio.h>

const char *mph_status_name(mph_status_t status) {
    switch (status) {
    case MPH_STATUS_OK:
        return "ok";
    case MPH_STATUS_INVALID_ARGUMENT:
        return "invalid_argument";
    case MPH_STATUS_NOT_FOUND:
        return "not_found";
    case MPH_STATUS_NO_MEMORY:
        return "no_memory";
    case MPH_STATUS_CAPACITY_EXCEEDED:
        return "capacity_exceeded";
    case MPH_STATUS_CONFLICT:
        return "conflict";
    case MPH_STATUS_INTERNAL_ERROR:
        return "internal_error";
    }

    return "unknown_status";
}

bool mph_status_is_ok(mph_status_t status) {
    return status == MPH_STATUS_OK;
}

void mph_error_init(mph_error_t *error) {
    mph_error_clear(error);
}

void mph_error_clear(mph_error_t *error) {
    if (error == NULL) {
        return;
    }

    error->status = MPH_STATUS_OK;
    error->message[0] = '\0';
}

void mph_error_set(mph_error_t *error, mph_status_t status, const char *message) {
    if (error == NULL) {
        return;
    }

    error->status = status;
    snprintf(error->message, MPH_ERROR_MESSAGE_CAPACITY, "%s", message != NULL ? message : "");
}
