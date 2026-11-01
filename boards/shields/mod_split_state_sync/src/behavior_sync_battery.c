/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_behavior_sync_battery

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include "sync_battery.h"

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/split/bluetooth/peripheral.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    LOG_DBG("split sync: syncbat received local_id=%u device=%u packed=0x%04x",
            sync_device_local_id(), binding->param1, binding->param2);
    if (sync_device_local_id() != SYNC_DEVICE_LEFT || binding->param1 != SYNC_DEVICE_RIGHT) {
        return 0;
    }
#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    if (!zmk_split_bt_peripheral_is_connected()) {
        LOG_DBG("split sync: ignoring syncbat while disconnected");
        return 0;
    }
#endif
    uint32_t packed = binding->param2;
    if ((packed & SYNC_BATTERY_LEVEL_MASK) > 100 || (packed & ~SYNC_BATTERY_PAYLOAD_MASK)) {
        LOG_WRN("split sync: invalid syncbat payload=0x%04x", packed);
        return -EINVAL;
    }
    sync_battery_set((struct sync_battery_state){
        .level = packed & SYNC_BATTERY_LEVEL_MASK,
        .connected = (packed & SYNC_BATTERY_CONNECTED) != 0,
        .valid = (packed & SYNC_BATTERY_VALID) != 0,
        .charging = (packed & SYNC_BATTERY_CHARGING) != 0,
    });
#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    /* A disconnect can race a command already queued on the peripheral. */
    if (!zmk_split_bt_peripheral_is_connected()) {
        sync_battery_set((struct sync_battery_state){0});
    }
#endif
    return 0;
}

static const struct behavior_driver_api api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = on_pressed,
};

#define DEFINE_SYNC_BATTERY(n)                                                                      \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_SYNC_BATTERY)
