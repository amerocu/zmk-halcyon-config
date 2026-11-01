/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>

#include "peripheral_status.h"
#include "mod_lock.h"
#include "split_state.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static const char *const layer_names[] = {"QWE", "Sym", "Nav", "Fun", "Adj", "Mac", "SyM"};
#define LAYER_NAME_COUNT ARRAY_SIZE(layer_names)

struct peripheral_status_state {
    bool connected;
};

static void draw_top(lv_obj_t *canvas, const void *context) {
    const struct zmk_widget_status *widget = context;
    const struct status_state *state = &widget->state;
    canvas = begin_canvas(canvas);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    draw_battery(canvas, state);
#if IS_ENABLED(CONFIG_SPLIT_STATE_SYNC_DEVICE_LEFT)
    draw_battery_icon(canvas, 35, widget->right_battery.level, widget->right_battery.charging,
                      widget->right_battery.connected, widget->right_battery.valid);
#endif
    canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc,
                     state->connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);

    rotate_canvas(canvas);
}

static void draw_layer(lv_obj_t *canvas, const void *context) {
    const struct zmk_widget_status *widget = context;
    const uint8_t layer_index = widget->layer;
    canvas = begin_canvas(canvas);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER);
    const char *name = (layer_index < LAYER_NAME_COUNT) ? layer_names[layer_index] : NULL;
    if (name == NULL) {
        char text[10] = {};
        sprintf(text, "LAYER %i", layer_index);
        canvas_draw_text(canvas, 0, 0, 88, &label_dsc, text);
    } else {
        canvas_draw_text(canvas, 0, 0, 88, &label_dsc, name);
    }

    rotate_canvas(canvas);
}

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
    bool changed = widget->state.battery != state.level;
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    changed |= widget->state.charging != state.usb_present;
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    if (changed) {
        redraw_section(&widget->sections[SECTION_STATUS]);
    }
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    return (struct battery_status_state){
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

#if IS_ENABLED(CONFIG_SPLIT_STATE_SYNC_DEVICE_LEFT)
static void right_battery_update_cb(struct sync_battery_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (!sync_battery_equal(widget->right_battery, state)) {
            LOG_DBG("split sync: redraw battery status connected=%d valid=%d level=%u charging=%d",
                    state.connected, state.valid, state.level, state.charging);
            widget->right_battery = state;
            redraw_section(&widget->sections[SECTION_STATUS]);
        }
    }
}

static struct sync_battery_state right_battery_get_state(const zmk_event_t *eh) {
    return sync_battery_get();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_right_battery, struct sync_battery_state,
                            right_battery_update_cb, right_battery_get_state)
ZMK_SUBSCRIPTION(widget_right_battery, zmk_sync_battery_state_changed);
#endif

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

static void set_connection_status(struct zmk_widget_status *widget,
                                  struct peripheral_status_state state) {
    bool changed = widget->state.connected != state.connected;
    widget->state.connected = state.connected;
    if (changed) {
        redraw_section(&widget->sections[SECTION_STATUS]);
    }
}

static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

static void sync_draw_work_cb(struct k_work *work) {
    const struct peripheral_state state = synced_state;
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (widget->layer != state.layer) {
            widget->layer = state.layer;
            redraw_section(&widget->sections[SECTION_LAYER]);
        }
        if (widget->mod_lock.mods != state.mods) {
            widget->mod_lock.mods = state.mods;
            redraw_section(&widget->sections[SECTION_MODIFIERS]);
        }
    }
}

static K_WORK_DEFINE(sync_draw_work, sync_draw_work_cb);

void sync_state_draw(void) {
    k_work_submit_to_queue(zmk_display_work_q(), &sync_draw_work);
}

void zmk_widget_status_redraw(struct zmk_widget_status *widget) {
    redraw_screen(widget->sections, ARRAY_SIZE(widget->sections), lv_obj_get_screen(widget->obj));
}

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_remove_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(widget->obj, SCREEN_LENGTH, CANVAS_SIZE);
    lv_obj_set_style_bg_color(widget->obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_COVER, LV_PART_MAIN);

    memset(widget->sections, 0, sizeof(widget->sections));
    init_section(&widget->sections[SECTION_STATUS], widget->obj, widget->cbuf,
                 STATUS_HEIGHT, STATUS_OFFSET, draw_top, widget);
    init_section(&widget->sections[SECTION_LAYER], widget->obj, widget->cbuf2,
                 LAYER_HEIGHT, LAYER_OFFSET, draw_layer, widget);
    init_section(&widget->sections[SECTION_MODIFIERS], widget->obj, widget->cbuf3,
                 MODIFIER_HEIGHT, MODIFIER_OFFSET, draw_modlock, &widget->mod_lock);

    widget->layer = synced_state.layer;
    widget->mod_lock.mods = synced_state.mods;

    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_peripheral_status_init();
#if IS_ENABLED(CONFIG_SPLIT_STATE_SYNC_DEVICE_LEFT)
    widget_right_battery_init();
#endif
    zmk_widget_status_redraw(widget);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
