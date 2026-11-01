/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/util.h>
#include <zmk/event_manager.h>
#include "device_identity.h"

struct sync_battery_state {
    uint8_t level;
    bool connected;
    bool valid;    /* A percentage has been received for this connection. */
    bool charging; /* USB-powered, matching the local display's bolt. */
};

#define SYNC_BATTERY_LEVEL_MASK 0xff
#define SYNC_BATTERY_CONNECTED BIT(8)
#define SYNC_BATTERY_VALID BIT(9)
#define SYNC_BATTERY_CHARGING BIT(10)
#define SYNC_BATTERY_PAYLOAD_MASK                                                                  \
    (SYNC_BATTERY_LEVEL_MASK | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID | SYNC_BATTERY_CHARGING)

static inline bool sync_battery_equal(struct sync_battery_state a, struct sync_battery_state b) {
    return a.level == b.level && a.connected == b.connected && a.valid == b.valid &&
           a.charging == b.charging;
}

struct zmk_sync_battery_state_changed {
    struct sync_battery_state state;
};

ZMK_EVENT_DECLARE(zmk_sync_battery_state_changed);

struct sync_battery_state sync_battery_get(void);
void sync_battery_set(struct sync_battery_state state);
