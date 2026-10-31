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
    lv_coord_t ix = x + (w - icon->header.w) / 2;
    lv_coord_t iy = y + (h - icon->header.h) / 2;

    if (active) {
        lv_draw_rect_dsc_t rect_dsc;
        init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
        canvas_draw_rect(canvas, x, y, w, h, &rect_dsc);

    }

    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    canvas_draw_img(canvas, ix, iy, icon, &img_dsc);
}

void draw_modlock(lv_obj_t *canvas, const struct mod_lock_state *state) {
    lv_draw_rect_dsc_t rect_bg_dsc;
    init_rect_dsc(&rect_bg_dsc, LVGL_BACKGROUND);
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_bg_dsc);

    /* Two 36px slots per axis, with a 4px gap and 6px outer margins. */
    static const lv_coord_t mod_x[4] = {6, 46, 6, 46};
    static const lv_coord_t mod_y[4] = {6, 6, 46, 46};
    const lv_image_dsc_t *mod_icons[4] = {
        &mod_control, &mod_opt, &mod_cmd, &mod_shift};
    const lv_image_dsc_t *mod_inverted[4] = {
        &mod_control_white, &mod_opt_white, &mod_cmd_white, &mod_shift_white};
    const uint8_t mod_masks[4] = {MOD_MASK_CTRL, MOD_MASK_ALT, MOD_MASK_GUI, MOD_MASK_SHIFT};
    for (int i = 0; i < 4; i++) {
        bool active = (state->mods & mod_masks[i]) != 0;
        draw_slot(canvas, mod_x[i], mod_y[i], 36, 36,
                  active ? mod_inverted[i] : mod_icons[i], active);
    }

    rotate_canvas(canvas);
}
