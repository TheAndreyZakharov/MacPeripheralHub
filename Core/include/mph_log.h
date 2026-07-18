#ifndef MPH_LOG_H
#define MPH_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MPH_LOG_LEVEL_DEBUG = 0,
    MPH_LOG_LEVEL_INFO,
    MPH_LOG_LEVEL_WARN,
    MPH_LOG_LEVEL_ERROR
} mph_log_level_t;

typedef void (*mph_log_callback_t)(mph_log_level_t level, const char *component, const char *message,
                                   void *context);

const char *mph_log_level_name(mph_log_level_t level);
void mph_log_set_callback(mph_log_callback_t callback, void *context);
void mph_log_message(mph_log_level_t level, const char *component, const char *message);

#ifdef __cplusplus
}
#endif

#endif
