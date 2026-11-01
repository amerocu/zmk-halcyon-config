/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "mod_lock.h"
#include "icons.h"
#include "util.h"

#include <dt-bindings/zmk/modifiers.h>

#define MOD_MASK_CTRL (MOD_LCTL | MOD_RCTL)
#define MOD_MASK_ALT (MOD_LALT | MOD_RALT)
#define MOD_MASK_GUI (MOD_LGUI | MOD_RGUI)
#define MOD_MASK_SHIFT (MOD_LSFT | MOD_RSFT)

static void draw_slot(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                      const lv_image_dsc_t *icon, bool active) {
    lv_coord_t iy = y + (h - icon->header.h) / 2;
    lv_coord_t ix = x + (w - icon->header.w) / 2;

    if (active) {
        lv_draw_rect_dsc_t rect_dsc;
        init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
        canvas_draw_rect(canvas, x, y, w, h, &rect_dsc);
    }

    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    canvas_draw_img(canvas, ix, iy, icon, &img_dsc);
}

void draw_modlock(lv_obj_t *canvas, const void *context) {
    const struct mod_lock_state *state = context;
    canvas = begin_canvas(canvas);

    /* This entire 20px-high section sits at the bottom of the display. */
    static const lv_coord_t mod_x[4] = {0, 22, 44, 66};
    const lv_image_dsc_t *small_icons[4] = {
        &mod_cmd_small, &mod_opt_small, &mod_control_small, &mod_shift_small};
    const lv_image_dsc_t *small_inverted[4] = {
        &mod_cmd_small_white, &mod_opt_small_white,
        &mod_control_small_white, &mod_shift_small_white};
    const uint8_t mod_masks[4] = {MOD_MASK_GUI, MOD_MASK_ALT, MOD_MASK_CTRL, MOD_MASK_SHIFT};
    for (int i = 0; i < 4; i++) {
        bool active = (state->mods & mod_masks[i]) != 0;
        draw_slot(canvas, mod_x[i], 0, 22, MODIFIER_HEIGHT,
                  active ? small_inverted[i] : small_icons[i], active);
    }

    rotate_canvas(canvas);
}
