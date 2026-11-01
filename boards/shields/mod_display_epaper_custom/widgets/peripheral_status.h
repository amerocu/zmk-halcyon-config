#pragma once
#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"
#include "mod_lock.h"
#include "sync_battery.h"

struct zmk_widget_status {
    sys_snode_t node;
    lv_obj_t *obj;
    struct display_section sections[SECTION_COUNT];
    uint8_t cbuf[SECTION_BUF_SIZE(STATUS_HEIGHT)];
    uint8_t cbuf2[SECTION_BUF_SIZE(LAYER_HEIGHT)];
    uint8_t cbuf3[SECTION_BUF_SIZE(MODIFIER_HEIGHT)];
    struct status_state state;
    struct sync_battery_state right_battery;
    struct mod_lock_state mod_lock;
    uint8_t layer;
};

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent);
/* Call on the display queue. Routine updates use redraw_section() instead. */
void zmk_widget_status_redraw(struct zmk_widget_status *widget);
lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget);
