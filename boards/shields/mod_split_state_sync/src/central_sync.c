/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Central-side listener.  Detects layer / modifier changes and relays them to
 * peripherals by invoking the GLOBAL behaviours &sync_mods / &sync_layer.
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/hid.h>
#include <zmk/behavior.h>

#include "split_state.h"

static uint8_t last_sent_mods;
static uint8_t last_sent_layer = 0xFF;

static void central_sync_update(void) {
    uint8_t mods = zmk_hid_get_explicit_mods();
    uint8_t layer = zmk_keymap_highest_layer_active();

    LOG_DBG("central_sync: fired layer=%d (last=%d) mods=0x%02x (last=0x%02x)", layer,
            last_sent_layer, mods, last_sent_mods);

    if (layer != last_sent_layer) {
        last_sent_layer = layer;
        LOG_INF("central_sync: invoking sync_layer %d", layer);
        struct zmk_behavior_binding b = {
            .behavior_dev = "synclyr",
            .param1 = layer,
        };
        struct zmk_behavior_binding_event e = {
            .position = 0,
            .timestamp = k_uptime_get(),
        };
        zmk_behavior_invoke_binding(&b, e, true);
    }

    if (mods != last_sent_mods) {
        last_sent_mods = mods;
        LOG_INF("central_sync: invoking sync_mods 0x%02x", mods);
        struct zmk_behavior_binding b = {
            .behavior_dev = "syncmod",
            .param1 = mods,
        };
        struct zmk_behavior_binding_event e = {
            .position = 0,
            .timestamp = k_uptime_get(),
        };
        zmk_behavior_invoke_binding(&b, e, true);
    }
}

static int central_sync_listener(const zmk_event_t *eh) {
    central_sync_update();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(central_sync, central_sync_listener);
ZMK_SUBSCRIPTION(central_sync, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(central_sync, zmk_layer_state_changed);
