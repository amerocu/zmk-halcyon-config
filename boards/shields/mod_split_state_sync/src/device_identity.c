/* SPDX-License-Identifier: MIT */
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include "device_identity.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

enum sync_device_id sync_device_local_id(void) {
    if (IS_ENABLED(CONFIG_SPLIT_STATE_SYNC_DEVICE_DONGLE)) {
        return SYNC_DEVICE_DONGLE;
    }
    return IS_ENABLED(CONFIG_SPLIT_STATE_SYNC_DEVICE_LEFT) ? SYNC_DEVICE_LEFT : SYNC_DEVICE_RIGHT;
}

static int identity_info_init(void) {
    LOG_INF("split sync: local device=%s DIS serial=%s", CONFIG_SPLIT_STATE_SYNC_DEVICE_NAME,
            CONFIG_BT_DIS_SERIAL_NUMBER_STR);
    return 0;
}

SYS_INIT(identity_info_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
