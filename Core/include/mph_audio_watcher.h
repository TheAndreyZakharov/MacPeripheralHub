#ifndef MPH_AUDIO_WATCHER_H
#define MPH_AUDIO_WATCHER_H

#include "mph_reconcile.h"
#include "mph_result.h"
#include "mph_selection.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mph_audio_watcher mph_audio_watcher_t;

typedef enum {
    MPH_AUDIO_WATCHER_EVENT_NONE = 0,
    MPH_AUDIO_WATCHER_EVENT_DEFAULT_INPUT_CHANGED = 1u << 0,
    MPH_AUDIO_WATCHER_EVENT_DEFAULT_OUTPUT_CHANGED = 1u << 1,
    MPH_AUDIO_WATCHER_EVENT_DEFAULT_SYSTEM_OUTPUT_CHANGED = 1u << 2,
    MPH_AUDIO_WATCHER_EVENT_DEVICES_CHANGED = 1u << 3,
    MPH_AUDIO_WATCHER_EVENT_PERIODIC_SCAN = 1u << 4,
    MPH_AUDIO_WATCHER_EVENT_MANUAL_SCAN = 1u << 5,
    MPH_AUDIO_WATCHER_EVENT_RECONCILE_APPLIED = 1u << 6,
    MPH_AUDIO_WATCHER_EVENT_RECONCILE_FAILED = 1u << 7
} mph_audio_watcher_event_flag_t;

typedef struct {
    uint32_t flags;
    mph_status_t status;
    mph_reconcile_plan_t plan;
    size_t available_audio_device_count;
    uint64_t timestamp_unix_ms;
} mph_audio_watcher_event_t;

typedef void (*mph_audio_watcher_callback_t)(const mph_audio_watcher_event_t *event, void *context);

typedef struct {
    mph_selection_t desired_selection;
    mph_reconcile_policy_t reconcile_policy;
    uint64_t fallback_scan_interval_ms;
    bool apply_reconcile_actions;
    mph_audio_watcher_callback_t callback;
    void *callback_context;
} mph_audio_watcher_config_t;

void mph_audio_watcher_config_init(mph_audio_watcher_config_t *config);
mph_status_t mph_audio_watcher_create(mph_audio_watcher_t **out_watcher,
                                      const mph_audio_watcher_config_t *config);
void mph_audio_watcher_destroy(mph_audio_watcher_t *watcher);
mph_status_t mph_audio_watcher_start(mph_audio_watcher_t *watcher);
void mph_audio_watcher_stop(mph_audio_watcher_t *watcher);
mph_status_t mph_audio_watcher_set_desired_selection(mph_audio_watcher_t *watcher,
                                                     const mph_selection_t *selection);
mph_status_t mph_audio_watcher_request_scan(mph_audio_watcher_t *watcher, uint32_t event_flags);
bool mph_audio_watcher_is_running(const mph_audio_watcher_t *watcher);

#ifdef __cplusplus
}
#endif

#endif
