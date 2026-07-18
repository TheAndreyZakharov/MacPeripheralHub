#include "mph_audio_watcher.h"

#include "mph_core_audio.h"
#include "mph_device_list.h"
#include "mph_log.h"
#include "mph_time.h"

#include <CoreAudio/CoreAudio.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define MPH_AUDIO_WATCHER_DEFAULT_SCAN_INTERVAL_MS 5000

struct mph_audio_watcher {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t worker;
    bool mutex_initialized;
    bool condition_initialized;
    bool worker_started;
    bool running;
    bool listeners_registered;
    uint32_t pending_flags;
    mph_audio_watcher_config_t config;
    mph_reconcile_state_t reconcile_state;
};

static AudioObjectPropertyAddress property_address(AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };
    return address;
}

static uint32_t event_flags_for_selector(AudioObjectPropertySelector selector) {
    switch (selector) {
    case kAudioHardwarePropertyDefaultInputDevice:
        return MPH_AUDIO_WATCHER_EVENT_DEFAULT_INPUT_CHANGED;
    case kAudioHardwarePropertyDefaultOutputDevice:
        return MPH_AUDIO_WATCHER_EVENT_DEFAULT_OUTPUT_CHANGED;
    case kAudioHardwarePropertyDefaultSystemOutputDevice:
        return MPH_AUDIO_WATCHER_EVENT_DEFAULT_SYSTEM_OUTPUT_CHANGED;
    case kAudioHardwarePropertyDevices:
        return MPH_AUDIO_WATCHER_EVENT_DEVICES_CHANGED;
    default:
        return MPH_AUDIO_WATCHER_EVENT_NONE;
    }
}

static void log_watcher_message(mph_log_level_t level, const char *message) {
    mph_log_message(level, "audio_watcher", message);
}

static void log_watcher_status(mph_log_level_t level, const char *operation, mph_status_t status) {
    char message[MPH_ERROR_MESSAGE_CAPACITY];
    snprintf(message, sizeof(message), "%s: %s", operation, mph_status_name(status));
    log_watcher_message(level, message);
}

static void log_core_audio_status(const char *operation, OSStatus status) {
    char message[MPH_ERROR_MESSAGE_CAPACITY];
    snprintf(message, sizeof(message), "%s failed with OSStatus %d", operation, (int)status);
    log_watcher_message(MPH_LOG_LEVEL_ERROR, message);
}

static void future_realtime(uint64_t delay_ms, struct timespec *out_time) {
    struct timeval now;
    gettimeofday(&now, NULL);

    uint64_t seconds = delay_ms / 1000ULL;
    uint64_t millis = delay_ms % 1000ULL;
    out_time->tv_sec = now.tv_sec + (time_t)seconds;
    out_time->tv_nsec = ((long)now.tv_usec * 1000L) + (long)(millis * 1000000ULL);
    if (out_time->tv_nsec >= 1000000000L) {
        out_time->tv_sec += 1;
        out_time->tv_nsec -= 1000000000L;
    }
}

static OSStatus audio_property_listener(AudioObjectID object_id, UInt32 address_count,
                                        const AudioObjectPropertyAddress addresses[],
                                        void *client_data) {
    (void)object_id;
    mph_audio_watcher_t *watcher = (mph_audio_watcher_t *)client_data;
    if (watcher == NULL) {
        return noErr;
    }

    uint32_t flags = 0;
    for (UInt32 index = 0; index < address_count; index += 1) {
        flags |= event_flags_for_selector(addresses[index].mSelector);
    }

    if (flags != 0) {
        (void)mph_audio_watcher_request_scan(watcher, flags);
    }

    return noErr;
}

static OSStatus add_listener(mph_audio_watcher_t *watcher, AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress address = property_address(selector);
    return AudioObjectAddPropertyListener(kAudioObjectSystemObject, &address,
                                          audio_property_listener, watcher);
}

static OSStatus remove_listener(mph_audio_watcher_t *watcher,
                                AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress address = property_address(selector);
    return AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &address,
                                             audio_property_listener, watcher);
}

static mph_status_t register_listeners(mph_audio_watcher_t *watcher) {
    if (watcher == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    const AudioObjectPropertySelector selectors[] = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioHardwarePropertyDefaultSystemOutputDevice,
        kAudioHardwarePropertyDevices,
    };

    for (size_t index = 0; index < sizeof(selectors) / sizeof(selectors[0]); index += 1) {
        OSStatus status = add_listener(watcher, selectors[index]);
        if (status != noErr) {
            log_core_audio_status("AudioObjectAddPropertyListener", status);
            for (size_t rollback_index = 0; rollback_index < index; rollback_index += 1) {
                (void)remove_listener(watcher, selectors[rollback_index]);
            }
            return MPH_STATUS_INTERNAL_ERROR;
        }
    }

    watcher->listeners_registered = true;
    log_watcher_message(MPH_LOG_LEVEL_INFO, "CoreAudio property listeners registered");
    return MPH_STATUS_OK;
}

static void unregister_listeners(mph_audio_watcher_t *watcher) {
    if (watcher == NULL || !watcher->listeners_registered) {
        return;
    }

    const AudioObjectPropertySelector selectors[] = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioHardwarePropertyDefaultSystemOutputDevice,
        kAudioHardwarePropertyDevices,
    };

    for (size_t index = 0; index < sizeof(selectors) / sizeof(selectors[0]); index += 1) {
        OSStatus status = remove_listener(watcher, selectors[index]);
        if (status != noErr) {
            log_core_audio_status("AudioObjectRemovePropertyListener", status);
        }
    }

    watcher->listeners_registered = false;
    log_watcher_message(MPH_LOG_LEVEL_INFO, "CoreAudio property listeners removed");
}

static mph_status_t build_current_audio_selection(mph_selection_t *selection) {
    if (selection == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_selection_init(selection);
    selection->enforce_audio_defaults = false;

    const struct {
        mph_device_role_t role;
        mph_status_t (*getter)(mph_device_id_t *out_device_id, bool *out_found);
    } defaults[] = {
        {MPH_DEVICE_ROLE_DEFAULT_INPUT, mph_core_audio_get_default_input},
        {MPH_DEVICE_ROLE_DEFAULT_OUTPUT, mph_core_audio_get_default_output},
        {MPH_DEVICE_ROLE_SYSTEM_OUTPUT, mph_core_audio_get_default_system_output},
    };

    for (size_t index = 0; index < sizeof(defaults) / sizeof(defaults[0]); index += 1) {
        mph_device_id_t device_id;
        bool found = false;
        mph_status_t status = defaults[index].getter(&device_id, &found);
        if (!mph_status_is_ok(status)) {
            return status;
        }
        if (found) {
            status = mph_selection_set_role_device(selection, defaults[index].role, &device_id);
            if (!mph_status_is_ok(status)) {
                return status;
            }
        }
    }

    return MPH_STATUS_OK;
}

static mph_status_t evaluate_and_apply(mph_audio_watcher_t *watcher, uint32_t flags,
                                       mph_audio_watcher_event_t *out_event) {
    if (watcher == NULL || out_event == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_selection_t desired;
    mph_reconcile_policy_t policy;
    bool apply_actions = false;
    pthread_mutex_lock(&watcher->mutex);
    desired = watcher->config.desired_selection;
    policy = watcher->config.reconcile_policy;
    apply_actions = watcher->config.apply_reconcile_actions;
    pthread_mutex_unlock(&watcher->mutex);

    memset(out_event, 0, sizeof(*out_event));
    out_event->flags = flags;
    out_event->timestamp_unix_ms = mph_time_now_unix_ms();
    mph_reconcile_plan_init(&out_event->plan);

    mph_device_list_t *available_devices = mph_device_list_create();
    if (available_devices == NULL) {
        out_event->status = MPH_STATUS_NO_MEMORY;
        return MPH_STATUS_NO_MEMORY;
    }

    mph_status_t status = mph_core_audio_enumerate_devices(available_devices);
    if (!mph_status_is_ok(status)) {
        out_event->status = status;
        mph_device_list_destroy(available_devices);
        return status;
    }
    out_event->available_audio_device_count = mph_device_list_count(available_devices);

    mph_selection_t current;
    status = build_current_audio_selection(&current);
    if (!mph_status_is_ok(status)) {
        out_event->status = status;
        mph_device_list_destroy(available_devices);
        return status;
    }

    mph_reconcile_context_t reconcile_context;
    mph_reconcile_context_init(&reconcile_context);
    reconcile_context.desired = &desired;
    reconcile_context.current = &current;
    reconcile_context.available_devices = available_devices;
    reconcile_context.now_unix_ms = out_event->timestamp_unix_ms;

    pthread_mutex_lock(&watcher->mutex);
    status = mph_reconcile_evaluate_audio_defaults(&reconcile_context, &watcher->reconcile_state,
                                                   &policy, &out_event->plan);
    pthread_mutex_unlock(&watcher->mutex);
    if (!mph_status_is_ok(status)) {
        out_event->status = status;
        mph_device_list_destroy(available_devices);
        return status;
    }

    if (apply_actions && out_event->plan.action_count > 0) {
        status = mph_reconcile_apply_audio_plan(&out_event->plan);
        if (mph_status_is_ok(status)) {
            out_event->flags |= MPH_AUDIO_WATCHER_EVENT_RECONCILE_APPLIED;
        } else {
            out_event->flags |= MPH_AUDIO_WATCHER_EVENT_RECONCILE_FAILED;
            log_watcher_status(MPH_LOG_LEVEL_ERROR, "reconcile apply failed", status);
        }
    }

    out_event->status = status;
    mph_device_list_destroy(available_devices);
    return status;
}

static void dispatch_event(mph_audio_watcher_t *watcher, const mph_audio_watcher_event_t *event) {
    if (watcher == NULL || event == NULL) {
        return;
    }

    mph_audio_watcher_callback_t callback = NULL;
    void *callback_context = NULL;
    pthread_mutex_lock(&watcher->mutex);
    callback = watcher->config.callback;
    callback_context = watcher->config.callback_context;
    pthread_mutex_unlock(&watcher->mutex);

    if (callback != NULL) {
        callback(event, callback_context);
    }
}

static void *watcher_thread_main(void *context) {
    mph_audio_watcher_t *watcher = (mph_audio_watcher_t *)context;
    log_watcher_message(MPH_LOG_LEVEL_INFO, "audio watcher worker started");

    while (true) {
        pthread_mutex_lock(&watcher->mutex);
        while (watcher->running && watcher->pending_flags == 0) {
            struct timespec wake_time;
            future_realtime(watcher->config.fallback_scan_interval_ms, &wake_time);
            int wait_status =
                pthread_cond_timedwait(&watcher->condition, &watcher->mutex, &wake_time);
            if (wait_status == ETIMEDOUT && watcher->running && watcher->pending_flags == 0) {
                watcher->pending_flags |= MPH_AUDIO_WATCHER_EVENT_PERIODIC_SCAN;
                break;
            }
        }

        if (!watcher->running) {
            pthread_mutex_unlock(&watcher->mutex);
            break;
        }

        uint32_t flags = watcher->pending_flags;
        watcher->pending_flags = 0;
        pthread_mutex_unlock(&watcher->mutex);

        if (flags == 0) {
            continue;
        }

        mph_audio_watcher_event_t event;
        mph_status_t status = evaluate_and_apply(watcher, flags, &event);
        if (!mph_status_is_ok(status)) {
            event.flags |= MPH_AUDIO_WATCHER_EVENT_RECONCILE_FAILED;
            log_watcher_status(MPH_LOG_LEVEL_ERROR, "audio watcher scan failed", status);
        } else {
            log_watcher_message(MPH_LOG_LEVEL_DEBUG, "audio watcher scan completed");
        }
        dispatch_event(watcher, &event);
    }

    log_watcher_message(MPH_LOG_LEVEL_INFO, "audio watcher worker stopped");
    return NULL;
}

void mph_audio_watcher_config_init(mph_audio_watcher_config_t *config) {
    if (config == NULL) {
        return;
    }

    mph_selection_init(&config->desired_selection);
    mph_reconcile_policy_default(&config->reconcile_policy);
    config->fallback_scan_interval_ms = MPH_AUDIO_WATCHER_DEFAULT_SCAN_INTERVAL_MS;
    config->apply_reconcile_actions = true;
    config->callback = NULL;
    config->callback_context = NULL;
}

mph_status_t mph_audio_watcher_create(mph_audio_watcher_t **out_watcher,
                                      const mph_audio_watcher_config_t *config) {
    if (out_watcher == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    *out_watcher = NULL;
    mph_audio_watcher_t *watcher = (mph_audio_watcher_t *)calloc(1, sizeof(*watcher));
    if (watcher == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    if (pthread_mutex_init(&watcher->mutex, NULL) != 0) {
        free(watcher);
        return MPH_STATUS_INTERNAL_ERROR;
    }
    watcher->mutex_initialized = true;

    if (pthread_cond_init(&watcher->condition, NULL) != 0) {
        mph_audio_watcher_destroy(watcher);
        return MPH_STATUS_INTERNAL_ERROR;
    }
    watcher->condition_initialized = true;

    if (config != NULL) {
        watcher->config = *config;
    } else {
        mph_audio_watcher_config_init(&watcher->config);
    }
    if (watcher->config.fallback_scan_interval_ms == 0) {
        watcher->config.fallback_scan_interval_ms = MPH_AUDIO_WATCHER_DEFAULT_SCAN_INTERVAL_MS;
    }
    mph_reconcile_state_init(&watcher->reconcile_state);
    *out_watcher = watcher;
    return MPH_STATUS_OK;
}

void mph_audio_watcher_destroy(mph_audio_watcher_t *watcher) {
    if (watcher == NULL) {
        return;
    }

    mph_audio_watcher_stop(watcher);
    if (watcher->condition_initialized) {
        pthread_cond_destroy(&watcher->condition);
    }
    if (watcher->mutex_initialized) {
        pthread_mutex_destroy(&watcher->mutex);
    }
    free(watcher);
}

mph_status_t mph_audio_watcher_start(mph_audio_watcher_t *watcher) {
    if (watcher == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&watcher->mutex);
    if (watcher->running) {
        pthread_mutex_unlock(&watcher->mutex);
        return MPH_STATUS_OK;
    }
    watcher->running = true;
    watcher->pending_flags |= MPH_AUDIO_WATCHER_EVENT_MANUAL_SCAN;
    pthread_mutex_unlock(&watcher->mutex);

    mph_status_t status = register_listeners(watcher);
    if (!mph_status_is_ok(status)) {
        pthread_mutex_lock(&watcher->mutex);
        watcher->running = false;
        watcher->pending_flags = 0;
        pthread_mutex_unlock(&watcher->mutex);
        return status;
    }

    int create_status = pthread_create(&watcher->worker, NULL, watcher_thread_main, watcher);
    if (create_status != 0) {
        unregister_listeners(watcher);
        pthread_mutex_lock(&watcher->mutex);
        watcher->running = false;
        watcher->pending_flags = 0;
        pthread_mutex_unlock(&watcher->mutex);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    pthread_mutex_lock(&watcher->mutex);
    watcher->worker_started = true;
    pthread_cond_signal(&watcher->condition);
    pthread_mutex_unlock(&watcher->mutex);
    return MPH_STATUS_OK;
}

void mph_audio_watcher_stop(mph_audio_watcher_t *watcher) {
    if (watcher == NULL) {
        return;
    }

    bool join_worker = false;
    pthread_mutex_lock(&watcher->mutex);
    if (watcher->running) {
        watcher->running = false;
        pthread_cond_signal(&watcher->condition);
    }
    join_worker = watcher->worker_started;
    pthread_mutex_unlock(&watcher->mutex);

    if (join_worker) {
        pthread_join(watcher->worker, NULL);
        pthread_mutex_lock(&watcher->mutex);
        watcher->worker_started = false;
        pthread_mutex_unlock(&watcher->mutex);
    }

    unregister_listeners(watcher);
}

mph_status_t mph_audio_watcher_set_desired_selection(mph_audio_watcher_t *watcher,
                                                     const mph_selection_t *selection) {
    if (watcher == NULL || selection == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&watcher->mutex);
    watcher->config.desired_selection = *selection;
    mph_reconcile_state_init(&watcher->reconcile_state);
    watcher->pending_flags |= MPH_AUDIO_WATCHER_EVENT_MANUAL_SCAN;
    pthread_cond_signal(&watcher->condition);
    pthread_mutex_unlock(&watcher->mutex);
    log_watcher_message(MPH_LOG_LEVEL_INFO, "desired audio selection updated");
    return MPH_STATUS_OK;
}

mph_status_t mph_audio_watcher_request_scan(mph_audio_watcher_t *watcher, uint32_t event_flags) {
    if (watcher == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&watcher->mutex);
    watcher->pending_flags |= event_flags != 0 ? event_flags : MPH_AUDIO_WATCHER_EVENT_MANUAL_SCAN;
    pthread_cond_signal(&watcher->condition);
    pthread_mutex_unlock(&watcher->mutex);
    return MPH_STATUS_OK;
}

bool mph_audio_watcher_is_running(const mph_audio_watcher_t *watcher) {
    if (watcher == NULL) {
        return false;
    }

    mph_audio_watcher_t *mutable_watcher = (mph_audio_watcher_t *)watcher;
    pthread_mutex_lock(&mutable_watcher->mutex);
    bool running = mutable_watcher->running;
    pthread_mutex_unlock(&mutable_watcher->mutex);
    return running;
}
