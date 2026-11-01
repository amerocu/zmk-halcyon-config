/* SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sync_battery.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/events/split_peripheral_status_changed.h>
#endif

ZMK_EVENT_IMPL(zmk_sync_battery_state_changed);

static struct k_spinlock battery_lock;
static struct sync_battery_state right_battery;

struct sync_battery_state sync_battery_get(void) {
    k_spinlock_key_t key = k_spin_lock(&battery_lock);
    struct sync_battery_state state = right_battery;
    k_spin_unlock(&battery_lock, key);
    return state;
}

void sync_battery_set(struct sync_battery_state state) {
    if (!state.connected) {
        state.valid = false;
        state.charging = false;
    }
    if (!state.valid) {
        state.level = 0;
    }
    k_spinlock_key_t key = k_spin_lock(&battery_lock);
    bool changed = !sync_battery_equal(right_battery, state);
    right_battery = state;
    k_spin_unlock(&battery_lock, key);
    if (changed) {
        LOG_DBG("split sync: right battery changed connected=%d valid=%d level=%u charging=%d",
                state.connected, state.valid, state.level, state.charging);
        raise_zmk_sync_battery_state_changed(
            (struct zmk_sync_battery_state_changed){.state = state});
    }
}

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static int connection_listener(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);
    if (!ev->connected) {
        LOG_DBG("split sync: dongle link lost; invalidating right battery");
        sync_battery_set((struct sync_battery_state){0});
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(sync_battery_connection, connection_listener);
ZMK_SUBSCRIPTION(sync_battery_connection, zmk_split_peripheral_status_changed);
#endif
