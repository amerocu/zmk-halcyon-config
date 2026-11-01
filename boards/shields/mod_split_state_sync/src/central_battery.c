/* SPDX-License-Identifier: MIT */
#include <string.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/ble.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/central.h>
#include "battery_service.h"
#include "sync_battery.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define SYNC_POLL_INTERVAL_MS 1000
#define SYNC_SNAPSHOT_INTERVAL_MS 5000

/* A retained connection reference prevents its index (and read parameters)
 * being reused while an asynchronous identity read is still in flight. */
struct sync_peer {
    struct bt_conn *conn;
    struct bt_gatt_read_params read;
    struct sync_battery_state battery;
    enum sync_device_id id;
    uint8_t source;
    bool reading;
    bool rejected;
    int64_t power_read_at;
};

static struct sync_peer peers[CONFIG_BT_MAX_CONN];
static struct k_spinlock peers_lock;
static uint32_t revision;
static uint32_t sent_revision;
static int64_t sent_at;
static int64_t status_at;

/* BT_UUID_DIS_SERIAL_NUMBER expands to a compound literal. Keep its storage
 * at file scope: a literal inside the worker's if-block expires before the
 * bt_gatt_read() call below, and cannot back asynchronous read parameters. */
static const struct bt_uuid_16 identity_uuid = BT_UUID_INIT_16(BT_UUID_DIS_SERIAL_NUMBER_VAL);
static const struct bt_uuid_128 power_uuid = BT_UUID_INIT_128(SYNC_BATTERY_POWER_UUID_VAL);

static void relay_work_cb(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(relay_work, relay_work_cb);

static uint8_t power_read(struct bt_conn *conn, uint8_t err,
                         struct bt_gatt_read_params *params, const void *data, uint16_t length) {
    struct sync_peer *peer = CONTAINER_OF(params, struct sync_peer, read);
    bool changed = false;
    bool valid = !err && data && length == 1 &&
                 (((const uint8_t *)data)[0] & ~SYNC_BATTERY_USB_POWERED) == 0;
    k_spinlock_key_t key = k_spin_lock(&peers_lock);
    peer->reading = false;
    if (valid && peer->battery.connected && peer->id == SYNC_DEVICE_RIGHT) {
        bool charging = (((const uint8_t *)data)[0] & SYNC_BATTERY_USB_POWERED) != 0;
        changed = peer->battery.charging != charging;
        peer->battery.charging = charging;
        if (changed) {
            revision++;
        }
    }
    uint8_t source = peer->source;
    bool charging = peer->battery.charging;
    k_spin_unlock(&peers_lock, key);
    LOG_DBG("split sync: power result source=%u att_err=0x%02x valid=%d charging=%d changed=%d",
            source, err, valid, charging, changed);
    if (changed) {
        k_work_reschedule(&relay_work, K_NO_WAIT);
    }
    return BT_GATT_ITER_STOP;
}

static uint8_t identity_read(struct bt_conn *conn, uint8_t err,
                            struct bt_gatt_read_params *params, const void *data, uint16_t length) {
    struct sync_peer *peer = CONTAINER_OF(params, struct sync_peer, read);
    enum sync_device_id id = SYNC_DEVICE_UNKNOWN;
    if (!err && data) {
        if (length == 4 && memcmp(data, "left", 4) == 0) {
            id = SYNC_DEVICE_LEFT;
        } else if (length == 5 && memcmp(data, "right", 5) == 0) {
            id = SYNC_DEVICE_RIGHT;
        }
    }
    k_spinlock_key_t key = k_spin_lock(&peers_lock);
    peer->reading = false;
    bool connected = peer->battery.connected;
    bool resolved = connected && !err && data && length > 0;
    if (resolved) {
        peer->id = id;
        peer->rejected = id == SYNC_DEVICE_UNKNOWN;
        revision++;
    }
    uint8_t source = peer->source;
    k_spin_unlock(&peers_lock, key);
    LOG_DBG("split sync: identity result source=%u att_err=0x%02x length=%u", source, err, length);
    if (resolved && id != SYNC_DEVICE_UNKNOWN) {
        LOG_INF("split sync: source %u identified as %s", source,
                id == SYNC_DEVICE_LEFT ? "left" : "right");
    } else if (resolved) {
        LOG_WRN("split sync: source %u has an unsupported device identity (length=%u)", source,
                length);
        LOG_HEXDUMP_DBG(data, MIN(length, 32), "split sync: unsupported DIS identity");
    } else if (!connected) {
        LOG_DBG("split sync: ignoring late identity result source=%u", source);
    }
    k_work_reschedule(&relay_work,
                      connected && !resolved ? K_MSEC(SYNC_POLL_INTERVAL_MS) : K_NO_WAIT);
    return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, uint8_t err) {
    struct bt_conn_info info;
    if (err || bt_conn_get_info(conn, &info) || info.role != BT_CONN_ROLE_CENTRAL) {
        return;
    }
    /* ZMK reserves the pairing slot before initiating this connection. This
     * public API returns the already saved slot for its identity address. */
    int source = zmk_ble_put_peripheral_addr(bt_conn_get_dst(conn));
    if (source < 0 || source >= ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT) {
        LOG_WRN("split sync: ignoring connection with invalid source=%d", source);
        return;
    }
    struct bt_conn *ref = bt_conn_ref(conn);
    k_spinlock_key_t key = k_spin_lock(&peers_lock);
    struct sync_peer *peer = &peers[bt_conn_index(conn)];
    *peer = (struct sync_peer){
        .conn = ref,
        .source = source,
        .battery.connected = true,
    };
    revision++;
    k_spin_unlock(&peers_lock, key);
    LOG_INF("split sync: connected source=%d conn_index=%u; identity pending", source,
            bt_conn_index(conn));
    /* Let ZMK's initial split-service discovery start first. */
    k_work_reschedule(&relay_work, K_MSEC(SYNC_POLL_INTERVAL_MS));
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    k_spinlock_key_t key = k_spin_lock(&peers_lock);
    struct sync_peer *peer = &peers[bt_conn_index(conn)];
    bool ours = peer->conn == conn;
    uint8_t source = peer->source;
    if (ours) {
        peer->battery = (struct sync_battery_state){0};
        revision++;
    }
    k_spin_unlock(&peers_lock, key);
    if (ours) {
        LOG_INF("split sync: disconnected source=%u reason=0x%02x", source, reason);
        k_work_reschedule(&relay_work, K_NO_WAIT);
    }
}

BT_CONN_CB_DEFINE(sync_identity_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* Called with peers_lock held. Duplicate logical IDs are not routable. */
static struct sync_peer *find_device(enum sync_device_id id) {
    struct sync_peer *found = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(peers); i++) {
        if (peers[i].battery.connected && peers[i].id == id) {
            if (found != NULL) {
                return NULL;
            }
            found = &peers[i];
        }
    }
    return found;
}

static void relay_work_cb(struct k_work *work) {
    int64_t now = k_uptime_get();
    for (size_t i = 0; i < ARRAY_SIZE(peers); i++) {
        struct sync_peer *peer = &peers[i];
        struct bt_conn *release = NULL;
        struct bt_conn *read_conn = NULL;
        k_spinlock_key_t key = k_spin_lock(&peers_lock);
        if (peer->conn && !peer->battery.connected && !peer->reading) {
            release = peer->conn;
            *peer = (struct sync_peer){0};
        } else if (peer->battery.connected && peer->id == SYNC_DEVICE_UNKNOWN &&
                   !peer->reading && !peer->rejected) {
            peer->read = (struct bt_gatt_read_params){
                .func = identity_read,
                .handle_count = 0,
                .by_uuid = {.start_handle = 1, .end_handle = 0xffff,
                            .uuid = &identity_uuid.uuid},
            };
            peer->reading = true;
            read_conn = peer->conn;
        } else if (peer->battery.connected && peer->id == SYNC_DEVICE_RIGHT &&
                   !peer->reading && now - peer->power_read_at >= SYNC_POLL_INTERVAL_MS) {
            peer->read = (struct bt_gatt_read_params){
                .func = power_read,
                .handle_count = 0,
                .by_uuid = {.start_handle = 1, .end_handle = 0xffff, .uuid = &power_uuid.uuid},
            };
            peer->power_read_at = now;
            peer->reading = true;
            read_conn = peer->conn;
        }
        k_spin_unlock(&peers_lock, key);
        if (release) {
            bt_conn_unref(release);
        }
        if (read_conn) {
            LOG_DBG("split sync: reading %s source=%u",
                    peer->read.func == identity_read ? "DIS identity" : "USB power", peer->source);
            int err = bt_gatt_read(read_conn, &peer->read);
            if (err) {
                key = k_spin_lock(&peers_lock);
                peer->reading = false;
                k_spin_unlock(&peers_lock, key);
                LOG_DBG("split sync: status read deferred source=%u (%d)", peer->source, err);
            }
        }
    }

    k_spinlock_key_t key = k_spin_lock(&peers_lock);
    struct sync_peer *left = find_device(SYNC_DEVICE_LEFT);
    struct sync_peer *right = find_device(SYNC_DEVICE_RIGHT);
    struct sync_battery_state battery = right ? right->battery : (struct sync_battery_state){0};
    int destination = left ? left->source : -1;
    int right_source = right ? right->source : -1;
    uint32_t current_revision = revision;
    k_spin_unlock(&peers_lock, key);

    if (now - status_at >= SYNC_SNAPSHOT_INTERVAL_MS) {
        LOG_DBG("split sync: status left_source=%d right_source=%d connected=%d valid=%d "
                "level=%u charging=%d", destination, right_source, battery.connected,
                battery.valid, battery.level, battery.charging);
        status_at = now;
    }
    /* Periodic snapshots recover from an early command reaching a peripheral
     * before run_behavior discovery completes. Duplicates never redraw. */
    if (destination >= 0 &&
        (current_revision != sent_revision || now - sent_at >= SYNC_SNAPSHOT_INTERVAL_MS)) {
        struct zmk_behavior_binding binding = {
            .behavior_dev = "syncbat",
            .param1 = SYNC_DEVICE_RIGHT,
            .param2 = battery.level | (battery.connected ? SYNC_BATTERY_CONNECTED : 0) |
                      (battery.valid ? SYNC_BATTERY_VALID : 0) |
                      (battery.charging ? SYNC_BATTERY_CHARGING : 0),
        };
        struct zmk_behavior_binding_event event = {.timestamp = now};
        int err = zmk_split_central_invoke_behavior(destination, &binding, event, true);
        LOG_DBG("split sync: syncbat queued left_source=%d connected=%d valid=%d level=%u "
                "charging=%d err=%d", destination, battery.connected, battery.valid,
                battery.level, battery.charging, err);
        if (!err) {
            sent_revision = current_revision;
            sent_at = now;
        }
    }
    k_work_reschedule(&relay_work, K_MSEC(SYNC_POLL_INTERVAL_MS));
}

static int battery_listener(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (ev->state_of_charge > 100) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    bool changed = false;
    bool matched = false;
    k_spinlock_key_t key = k_spin_lock(&peers_lock);
    for (size_t i = 0; i < ARRAY_SIZE(peers); i++) {
        struct sync_peer *peer = &peers[i];
        if (peer->battery.connected && peer->source == ev->source) {
            matched = true;
            struct sync_battery_state battery = peer->battery;
            battery.level = ev->state_of_charge;
            battery.valid = true;
            changed = !sync_battery_equal(peer->battery, battery);
            peer->battery = battery;
            if (changed) {
                revision++;
            }
            break;
        }
    }
    k_spin_unlock(&peers_lock, key);
    LOG_DBG("split sync: battery source=%u level=%u matched=%d changed=%d", ev->source,
            ev->state_of_charge, matched, changed);
    if (changed) {
        k_work_reschedule(&relay_work, K_NO_WAIT);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(sync_central_battery, battery_listener);
ZMK_SUBSCRIPTION(sync_central_battery, zmk_peripheral_battery_state_changed);

static int battery_relay_init(void) {
    LOG_INF("split sync: dongle battery relay enabled (DIS identity + syncbat)");
    k_work_reschedule(&relay_work, K_MSEC(SYNC_POLL_INTERVAL_MS));
    return 0;
}

SYS_INIT(battery_relay_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
