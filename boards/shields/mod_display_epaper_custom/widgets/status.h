/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"
#include "mod_lock.h"

struct zmk_widget_status {
    sys_snode_t node;
    lv_obj_t *obj;
    struct display_section sections[SECTION_COUNT];
    uint8_t cbuf[SECTION_BUF_SIZE(STATUS_HEIGHT)];
    uint8_t cbuf2[SECTION_BUF_SIZE(LAYER_HEIGHT)];
    uint8_t cbuf3[SECTION_BUF_SIZE(PROFILE_HEIGHT)];
    uint8_t cbuf4[SECTION_BUF_SIZE(MODIFIER_HEIGHT)];
    struct status_state state;
    struct mod_lock_state mod_lock;
};

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent);
/* Call on the display queue. Routine updates use redraw_section() instead. */
void zmk_widget_status_redraw(struct zmk_widget_status *widget);
lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget);
