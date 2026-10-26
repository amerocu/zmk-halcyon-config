#define DT_DRV_COMPAT zmk_behavior_sync_layer
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include "sync_state.h"
#define MAC_LAYER 5

static int on_pressed(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e) {
    synced_state.layer = b->param1;
    synced_state.mac = (b->param1 >= MAC_LAYER);
    sync_state_draw();
    return 0;
}
static int on_released(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e) {
    return 0;
}
static const struct behavior_driver_api api = {
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};
#define KP(n) BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);
DT_INST_FOREACH_STATUS_OKAY(KP)
