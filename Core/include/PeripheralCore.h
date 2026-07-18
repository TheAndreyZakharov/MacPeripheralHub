#ifndef PERIPHERAL_CORE_H
#define PERIPHERAL_CORE_H

/*
 PeripheralCore ownership rules:
 - Functions with _create return owned pointers; release them with the matching _destroy function.
 - Functions with _init initialize caller-owned stack or embedded values.
 - Functions with _append and _save copy the passed value into the target collection/store.
 - Functions with _get and _find return borrowed pointers that remain valid until mutation or
 destroy.
 - String pointers returned by *_cstr/name/version functions are borrowed and must not be freed.
 */

#include "mph_audio_watcher.h"
#include "mph_camera.h"
#include "mph_core.h"
#include "mph_core_audio.h"
#include "mph_db.h"
#include "mph_device.h"
#include "mph_device_id.h"
#include "mph_device_list.h"
#include "mph_display.h"
#include "mph_hid.h"
#include "mph_log.h"
#include "mph_profile.h"
#include "mph_profile_store.h"
#include "mph_reconcile.h"
#include "mph_result.h"
#include "mph_selection.h"
#include "mph_time.h"
#include "mph_usb.h"

#endif
