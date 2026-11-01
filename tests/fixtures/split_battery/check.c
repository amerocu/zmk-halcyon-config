/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "mock.h"
#include "src/device_identity.c"
#include "src/central_battery.c"
#include "src/sync_battery.c"
#include "src/behavior_sync_battery.c"
#include "src/battery_service.c"

static void tick(void) {
    mock_time += 1000;
    relay_work_cb(NULL);
    for (size_t i = 0; i < ARRAY_SIZE(peers); i++) {
        struct sync_peer *peer = &peers[i];
        if (peer->reading && peer->read.func == power_read) {
            uint8_t value;
            assert(read_power(peer->conn, NULL, &value, 1, 0) == 1);
            peer->read.func(peer->conn, 0, &peer->read, &value, 1);
        }
    }
}

static void identify(struct bt_conn *conn, const char *name) {
    struct sync_peer *p = &peers[conn->index];
    assert(p->reading);
    assert(((const struct bt_uuid_16 *)p->read.by_uuid.uuid)->val ==
           BT_UUID_DIS_SERIAL_NUMBER_VAL);
    p->read.func(conn, 0, &p->read, name, strlen(name));
}

static void level(int source, int value) {
    struct zmk_peripheral_battery_state_changed ev = {source, value};
    battery_listener(&ev);
}

static void reset(void) {
    for (size_t i = 0; i < ARRAY_SIZE(peers); i++) {
        assert(!peers[i].conn);
    }
    memset(peers, 0, sizeof(peers));
    sends = 0; revision = sent_revision = 0; sent_at = status_at = mock_time = 0;
    mock_usb_powered = false;
}

static void routing(int left_source, int right_source) {
    struct bt_conn left = {.index = 0, .source = left_source};
    struct bt_conn right = {.index = 1, .source = right_source};
    connected(&left, 0); connected(&right, 0);
    /* Battery data can precede asynchronous identity discovery. */
    level(right_source, 73);
    tick(); identify(&left, "left"); identify(&right, "right"); tick();
    assert(sends == 1 && sent_destination == left_source);
    assert(sent_binding.param1 == SYNC_DEVICE_RIGHT);
    assert(sent_binding.param2 == (73 | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID));
    unsigned before = sends;
    level(right_source, 73); tick(); assert(sends == before);
    /* Charging changes independently of percentage and survives later levels. */
    mock_usb_powered = true;
    tick(); tick();
    assert(sent_binding.param2 ==
           (73 | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID | SYNC_BATTERY_CHARGING));
    level(right_source, 0); tick();
    assert(sent_binding.param2 ==
           (SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID | SYNC_BATTERY_CHARGING));
    mock_usb_powered = false;
    tick(); tick();
    assert(sent_binding.param2 == (SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID));
    disconnected(&right, 0); level(right_source, 0); tick();
    assert(sent_binding.param2 == 0 && right.refs == 0);
    connected(&right, 0); tick(); identify(&right, "right"); tick();
    assert(sent_binding.param2 == SYNC_BATTERY_CONNECTED);
    level(right_source, 61); tick();
    assert(sent_binding.param2 == (61 | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID));
    /* A restarted left receives the cached right reading. */
    disconnected(&left, 0); tick(); connected(&left, 0); tick();
    identify(&left, "left"); tick();
    assert(sent_destination == left_source);
    assert(sent_binding.param2 == (61 | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID));
    disconnected(&left, 0); disconnected(&right, 0); tick();
    assert(!left.refs && !right.refs);
    reset();
}

static void test_pending_reads(void) {
    /* A late read completion must not resurrect a disconnected identity. */
    struct bt_conn right = {.index = 1, .source = 1};
    connected(&right, 0); tick(); disconnected(&right, 0); tick();
    assert(right.refs == 1);
    identify(&right, "right"); tick(); assert(right.refs == 0); reset();
    connected(&right, 0); tick();
    peers[1].read.func(&right, 10, &peers[1].read, NULL, 0);
    assert(relay_work.delay == 1000);
    disconnected(&right, 0); tick(); reset();

    /* Empty successful completions must also wait before retrying. */
    connected(&right, 0);
    tick();
    peers[1].read.func(&right, 0, &peers[1].read, NULL, 0);
    assert(relay_work.delay == SYNC_POLL_INTERVAL_MS);
    assert(!peers[1].reading && !peers[1].rejected);
    tick();
    identify(&right, "right");
    disconnected(&right, 0);
    tick();
    reset();

    /* A power read completing after disconnect cannot restore charging. */
    connected(&right, 0);
    tick();
    identify(&right, "right");
    mock_time += SYNC_POLL_INTERVAL_MS;
    relay_work_cb(NULL);
    assert(peers[1].reading && peers[1].read.func == power_read);
    disconnected(&right, 0);
    relay_work_cb(NULL);
    assert(right.refs == 1);
    uint8_t power = SYNC_BATTERY_USB_POWERED;
    peers[1].read.func(&right, 0, &peers[1].read, &power, sizeof(power));
    assert(!peers[1].battery.connected && !peers[1].battery.charging);
    tick();
    assert(right.refs == 0);
    reset();
}

static void test_identity_rejection(void) {
    struct bt_conn right = {.index = 1, .source = 1};
    connected(&right, 0); tick(); identify(&right, "other"); tick();
    assert(peers[1].rejected && !sends);
    disconnected(&right, 0); tick(); reset();
    /* Duplicate logical identities must not be routed arbitrarily. */
    struct bt_conn left = {.index = 0, .source = 0};
    connected(&left, 0); connected(&right, 0); tick();
    identify(&left, "left"); identify(&right, "left"); tick();
    assert(!sends);
    disconnected(&left, 0); disconnected(&right, 0); tick(); reset();
}

static void test_receiver(void) {
    /* Exercise the production receiver and its event deduplication. */
    event_count = 0;
    struct zmk_behavior_binding b = {.param1 = SYNC_DEVICE_RIGHT,
        .param2 = 80 | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID};
    struct zmk_behavior_binding_event e = {0};
    assert(!on_pressed(&b, e)); assert(event_count == 1);
    assert(!on_pressed(&b, e)); assert(event_count == 1);
    b.param2 = SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID;
    assert(!on_pressed(&b, e)); assert(event_count == 2);
    assert(sync_battery_get().connected && sync_battery_get().valid);
    b.param2 = 0; on_pressed(&b, e); assert(event_count == 3);
    assert(!sync_battery_get().connected);
    b.param2 = SYNC_BATTERY_CONNECTED; on_pressed(&b, e); assert(event_count == 4);
    assert(sync_battery_get().connected && !sync_battery_get().valid);
    b.param2 = 80 | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID;
    on_pressed(&b, e); assert(event_count == 5);
    local_connected = false;
    struct zmk_split_peripheral_status_changed ev = {.connected = false};
    connection_listener(&ev); assert(event_count == 6);
    on_pressed(&b, e); assert(event_count == 6);
    assert(!sync_battery_get().connected);
    local_connected = true;
    b.param2 = 101 | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID;
    assert(on_pressed(&b, e) == -EINVAL && event_count == 6);
    b.param2 = BIT(31);
    assert(on_pressed(&b, e) == -EINVAL && event_count == 6);
    /* Same percentage with USB plugged/unplugged must emit just one event
     * per transition; repeated snapshots must remain silent. */
    b.param2 = 80 | SYNC_BATTERY_CONNECTED | SYNC_BATTERY_VALID;
    on_pressed(&b, e); assert(event_count == 7);
    b.param2 |= SYNC_BATTERY_CHARGING;
    on_pressed(&b, e); assert(event_count == 8);
    assert(sync_battery_get().charging);
    on_pressed(&b, e); assert(event_count == 8);
    b.param2 &= ~SYNC_BATTERY_CHARGING;
    on_pressed(&b, e); assert(event_count == 9);
    assert(!sync_battery_get().charging);
    b.param2 |= SYNC_BATTERY_CHARGING;
    on_pressed(&b, e); assert(event_count == 10);
    local_connected = false;
    connection_listener(&ev); assert(event_count == 11);
    assert(!sync_battery_get().charging && !sync_battery_get().connected);
}

int main(void) {
    routing(0, 1);
    routing(1, 0);
    test_pending_reads();
    test_identity_rejection();
    test_receiver();
    puts("PASS: UUID lifetime, routing, reconnects, identity discovery and receiver deduplication");
}
