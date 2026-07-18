#include "mph_log.h"

#include <stdio.h>

static mph_log_callback_t g_callback = NULL;
static void *g_context = NULL;

const char *mph_log_level_name(mph_log_level_t level) {
    switch (level) {
    case MPH_LOG_LEVEL_DEBUG:
        return "debug";
    case MPH_LOG_LEVEL_INFO:
        return "info";
    case MPH_LOG_LEVEL_WARN:
        return "warn";
    case MPH_LOG_LEVEL_ERROR:
        return "error";
    }

    return "unknown";
}

void mph_log_set_callback(mph_log_callback_t callback, void *context) {
    g_callback = callback;
    g_context = context;
}

void mph_log_message(mph_log_level_t level, const char *component, const char *message) {
    const char *safe_component = component != NULL ? component : "core";
    const char *safe_message = message != NULL ? message : "";

    if (g_callback != NULL) {
        g_callback(level, safe_component, safe_message, g_context);
        return;
    }

    fprintf(stderr, "[%s] %s: %s\n", mph_log_level_name(level), safe_component, safe_message);
}
