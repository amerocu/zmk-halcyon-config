/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_behavior_poweroff

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/pm.h>

#include "poweroff.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static atomic_t requested;

bool split_poweroff_requested(void) { return atomic_get(&requested) != 0; }

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void shutdown_cb(struct k_work *work) {
    /* Soft-off does not emit an activity event itself. Let local power
     * listeners finish the BYE refresh and release VIK pins first. This runs
     * outside the BLE receive callback, on the system queue. */
    raise_zmk_activity_state_changed(
        (struct zmk_activity_state_changed){.state = ZMK_ACTIVITY_SLEEP});
    int err = zmk_pm_soft_off();
    LOG_ERR("Manual shutdown failed (%d)", err);
}

K_WORK_DEFINE(shutdown_work, shutdown_cb);
#endif

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    if (atomic_cas(&requested, 0, 1)) {
        int err = k_work_submit(&shutdown_work);
        if (err < 0) {
            atomic_set(&requested, 0);
            LOG_ERR("Could not queue manual shutdown (%d)", err);
            return err;
        }
    }
#endif
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    /* Wait for release: shutting the source half off on press would prevent
     * its release reaching the dongle and the other half. */
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api api = {
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
    .binding_pressed = on_pressed,
    .binding_released = on_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define DEFINE_POWEROFF(n)                                                                         \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_POWEROFF)
