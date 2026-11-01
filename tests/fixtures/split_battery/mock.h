/* SPDX-License-Identifier: MIT */
/* Minimal host adapters for exercising the production split battery C code. */
#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#define CONFIG_BT_MAX_CONN 4
#define CONFIG_SPLIT_STATE_SYNC_DEVICE_LEFT 1
#define CONFIG_SPLIT_STATE_SYNC_DEVICE_DONGLE 0
#define CONFIG_ZMK_SPLIT_ROLE_CENTRAL 0
#define IS_ENABLED(x) (x)
#define BIT(x) (1U << (x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define CONTAINER_OF(p, t, m) ((t *)((char *)(p) - offsetof(t, m)))
#define K_NO_WAIT 0
#define K_SECONDS(x) ((x) * 1000)
#define K_MSEC(x) (x)
#define SYS_INIT(...)
struct k_spinlock { int unused; };
typedef int k_spinlock_key_t;
static inline int k_spin_lock(struct k_spinlock *l) { return 0; }
static inline void k_spin_unlock(struct k_spinlock *l, int key) {}
struct k_work { int unused; };
struct k_work_delayable { int delay; };
#define K_WORK_DELAYABLE_DEFINE(n, cb) struct k_work_delayable n
static inline int k_work_reschedule(struct k_work_delayable *w, int delay) {
    w->delay = delay; return 0;
}
static int64_t mock_time;
static inline int64_t k_uptime_get(void) { return mock_time; }
#define LOG_MODULE_DECLARE(...)
#define LOG_INF(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define LOG_DBG(...) ((void)0)
#define LOG_HEXDUMP_DBG(...) ((void)0)
typedef void zmk_event_t;
#define ZMK_EV_EVENT_BUBBLE 0
static unsigned event_count;
#define ZMK_EVENT_DECLARE(t) \
    static inline const struct t *as_##t(const void *e) { return e; } \
    static inline int raise_##t(struct t e) { event_count++; return 0; }
#define ZMK_EVENT_IMPL(t)
#define ZMK_LISTENER(...)
#define ZMK_SUBSCRIPTION(...)
struct zmk_peripheral_battery_state_changed { uint8_t source, state_of_charge; };
ZMK_EVENT_DECLARE(zmk_peripheral_battery_state_changed)
struct zmk_split_peripheral_status_changed { bool connected; };
ZMK_EVENT_DECLARE(zmk_split_peripheral_status_changed)

#define BT_CONN_ROLE_CENTRAL 0
#define BT_GATT_ITER_STOP 0
struct bt_uuid { uint8_t type; };
struct bt_uuid_16 { struct bt_uuid uuid; uint16_t val; };
struct bt_uuid_128 { struct bt_uuid uuid; uint8_t val[16]; };
#define MOCK_LE16(v) (uint8_t)(v), (uint8_t)((v) >> 8)
#define MOCK_LE32(v) MOCK_LE16(v), MOCK_LE16((v) >> 16)
#define BT_UUID_128_ENCODE(w32, w16a, w16b, w16c, w48) \
    MOCK_LE32(w48), MOCK_LE16((w48) >> 32), MOCK_LE16(w16c), \
    MOCK_LE16(w16b), MOCK_LE16(w16a), MOCK_LE32(w32)
#define BT_UUID_INIT_128(...) {.uuid = {2}, .val = {__VA_ARGS__}}
/* Match Zephyr's compound-literal storage semantics, not just its value. */
#define BT_UUID_INIT_16(value) {.uuid = {0}, .val = (value)}
#define BT_UUID_DECLARE_16(value) \
    ((const struct bt_uuid *) ((const struct bt_uuid_16[]) {BT_UUID_INIT_16(value)}))
#define BT_UUID_DIS_SERIAL_NUMBER_VAL 0x2a25
#define BT_UUID_DIS_SERIAL_NUMBER BT_UUID_DECLARE_16(BT_UUID_DIS_SERIAL_NUMBER_VAL)
struct bt_conn { int index, source, role, refs; };
struct bt_conn_info { int role; };
struct bt_gatt_read_params {
    uint8_t (*func)(struct bt_conn *, uint8_t, struct bt_gatt_read_params *, const void *, uint16_t);
    size_t handle_count;
    struct { uint16_t start_handle, end_handle; const struct bt_uuid *uuid; } by_uuid;
};
struct bt_conn_cb {
    void (*connected)(struct bt_conn *, uint8_t);
    void (*disconnected)(struct bt_conn *, uint8_t);
};
#define BT_CONN_CB_DEFINE(n) struct bt_conn_cb n
static inline int bt_conn_get_info(struct bt_conn *c, struct bt_conn_info *i) {
    i->role = c->role; return 0;
}
static inline struct bt_conn *bt_conn_ref(struct bt_conn *c) { c->refs++; return c; }
static inline void bt_conn_unref(struct bt_conn *c) { assert(c->refs > 0); c->refs--; }
static inline unsigned bt_conn_index(struct bt_conn *c) { return c->index; }
static inline const struct bt_conn *bt_conn_get_dst(struct bt_conn *c) { return c; }
static inline int zmk_ble_put_peripheral_addr(const struct bt_conn *c) { return c->source; }
static int read_result;
static inline int bt_gatt_read(struct bt_conn *c, struct bt_gatt_read_params *p) {
    /* ASan must catch any expired UUID pointer when the BLE stack encodes it. */
    if (p->by_uuid.uuid->type == 0) {
        assert(((const struct bt_uuid_16 *)p->by_uuid.uuid)->val == BT_UUID_DIS_SERIAL_NUMBER_VAL);
    } else {
        const struct bt_uuid_128 *uuid = (const struct bt_uuid_128 *)p->by_uuid.uuid;
        assert(uuid->uuid.type == 2 && uuid->val[0] == 0x31 && uuid->val[12] == 0x11);
    }
    return read_result;
}
static bool local_connected = true;
static inline bool zmk_split_bt_peripheral_is_connected(void) { return local_connected; }
static bool mock_usb_powered;
static inline bool zmk_usb_is_powered(void) { return mock_usb_powered; }
struct bt_gatt_attr { int unused; };
#define BT_GATT_SERVICE_DEFINE(...)
static inline ssize_t bt_gatt_attr_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      void *buffer, uint16_t length, uint16_t offset,
                                      const void *value, uint16_t value_length) {
    if (offset > value_length) { return -EINVAL; }
    size_t count = value_length - offset;
    if (count > length) { count = length; }
    memcpy(buffer, (const uint8_t *)value + offset, count);
    return count;
}

#define ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT 2
struct zmk_behavior_binding { const char *behavior_dev; uint32_t param1, param2; };
struct zmk_behavior_binding_event { int64_t timestamp; };
static unsigned sends;
static int sent_destination;
static struct zmk_behavior_binding sent_binding;
static inline int zmk_split_central_invoke_behavior(int dest, struct zmk_behavior_binding *b,
                                                    struct zmk_behavior_binding_event e, bool p) {
    sends++; sent_destination = dest; sent_binding = *b; return 0;
}
#define BEHAVIOR_LOCALITY_CENTRAL 0
struct behavior_driver_api {
    int locality;
    int (*binding_pressed)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
};
#define DT_INST_FOREACH_STATUS_OKAY(...)
