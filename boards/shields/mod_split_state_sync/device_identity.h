/* SPDX-License-Identifier: MIT */
#pragma once

/* Stable logical identities, independent of BLE address and pairing slot.
 * CONFIG_SPLIT_STATE_SYNC_DEVICE_{DONGLE,LEFT,RIGHT} also allow CMake and C
 * code to include functionality only for the corresponding firmware target. */
enum sync_device_id {
    SYNC_DEVICE_UNKNOWN = 0,
    SYNC_DEVICE_DONGLE = 1,
    SYNC_DEVICE_LEFT = 2,
    SYNC_DEVICE_RIGHT = 3,
};

enum sync_device_id sync_device_local_id(void);
