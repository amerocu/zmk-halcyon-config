/* SPDX-License-Identifier: MIT */
#include <zephyr/bluetooth/gatt.h>
#include <zmk/usb.h>
#include "battery_service.h"

static const struct bt_uuid_128 battery_service_uuid = BT_UUID_INIT_128(SYNC_BATTERY_SERVICE_UUID_VAL);
static const struct bt_uuid_128 battery_power_uuid = BT_UUID_INIT_128(SYNC_BATTERY_POWER_UUID_VAL);

static ssize_t read_power(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buffer,
                         uint16_t length, uint16_t offset) {
    uint8_t state = zmk_usb_is_powered() ? SYNC_BATTERY_USB_POWERED : 0;
    return bt_gatt_attr_read(conn, attr, buffer, length, offset, &state, sizeof(state));
}

BT_GATT_SERVICE_DEFINE(
    sync_battery_service, BT_GATT_PRIMARY_SERVICE(&battery_service_uuid.uuid),
    BT_GATT_CHARACTERISTIC(&battery_power_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_power,
                           NULL, NULL));
