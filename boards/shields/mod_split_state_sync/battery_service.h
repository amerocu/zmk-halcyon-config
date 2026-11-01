/* SPDX-License-Identifier: MIT */
#pragma once

#include <zephyr/bluetooth/uuid.h>

#define SYNC_BATTERY_SERVICE_UUID_VAL                                                              \
    BT_UUID_128_ENCODE(0x442a6210, 0x7c18, 0x4e20, 0x9585, 0x93f45ec7b031)
#define SYNC_BATTERY_POWER_UUID_VAL                                                                \
    BT_UUID_128_ENCODE(0x442a6211, 0x7c18, 0x4e20, 0x9585, 0x93f45ec7b031)

/* One-byte power state, matching the local display's USB-powered bolt. */
#define SYNC_BATTERY_USB_POWERED 0x01
