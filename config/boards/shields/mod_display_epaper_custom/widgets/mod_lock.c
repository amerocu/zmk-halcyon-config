/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "mod_lock.h"
#include "icons.h"
#include "util.h"

#include <string.h>

#include <dt-bindings/zmk/hid_indicators.h>
#include <dt-bindings/zmk/modifiers.h>

#define MOD_MASK_CTRL (MOD_LCTL | MOD_RCTL)
#define MOD_MASK_ALT (MOD_LALT | MOD_RALT)
#define MOD_MASK_GUI (MOD_LGUI | MOD_RGUI)
#define MOD_MASK_SHIFT (MOD_LSFT | MOD_RSFT)

#define ICON_MAX_DATA (60)

static void draw_slot(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                      const lv_image_dsc_t *icon, bool active) {
    lv_coord_t ix = x + (w - icon->header.w) / 2;
    lv_coord_t iy = y + (h - icon->header.h) / 2;

    if (active) {
        lv_draw_rect_dsc_t rect_dsc;
        init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
        canvas_draw_rect(canvas, x, y, w, h, &rect_dsc);

        size_t px_len = icon->data_size - 8;
        uint8_t tmp[8 + ICON_MAX_DATA];
        memcpy(tmp, icon->data, 8);
        for (size_t i = 0; i < px_len; i++) {
            tmp[8 + i] = ((const uint8_t *)icon->data)[8 + i] ^ 0xFF;
        }
        lv_image_dsc_t inv = *icon;
        inv.data = tmp;
        inv.data_size = icon->data_size;

        lv_draw_image_dsc_t img_dsc;
        lv_draw_image_dsc_init(&img_dsc);
        canvas_draw_img(canvas, ix, iy, &inv, &img_dsc);
    } else {
        lv_draw_image_dsc_t img_dsc;
        lv_draw_image_dsc_init(&img_dsc);
        canvas_draw_img(canvas, ix, iy, icon, &img_dsc);
    }
}

void draw_modlock(lv_obj_t *canvas, const struct mod_lock_state *state) {
    lv_draw_rect_dsc_t rect_bg_dsc;
    init_rect_dsc(&rect_bg_dsc, LVGL_BACKGROUND);
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_bg_dsc);

    static const lv_coord_t mod_x[4] = {0, 22, 44, 66};
    const lv_image_dsc_t *mod_icons[4] = {
        &icon_ctrl, &icon_alt, state->mac ? &icon_cmd : &icon_diamond, &icon_shift};
    const uint8_t mod_masks[4] = {MOD_MASK_CTRL, MOD_MASK_ALT, MOD_MASK_GUI, MOD_MASK_SHIFT};
    const lv_coord_t mod_y = 14, mod_h = 24;

    for (int i = 0; i < 4; i++) {
        bool active = (state->mods & mod_masks[i]) != 0;
        draw_slot(canvas, mod_x[i], mod_y, 22, mod_h, mod_icons[i], active);
    }

    static const lv_coord_t lock_x[3] = {0, 29, 58};
    static const lv_coord_t lock_w[3] = {29, 29, 30};
    const lv_image_dsc_t *lock_icons[3] = {
        &icon_caps_word, &icon_caps_lock, state->mac ? &icon_apple : &icon_diamond};
    const bool lock_active[3] = {
        state->caps_word, (state->leds & HID_INDICATOR_CAPS_LOCK) != 0, false};
    const lv_coord_t lock_y = 52, lock_h = 24;

    for (int i = 0; i < 3; i++) {
        draw_slot(canvas, lock_x[i], lock_y, lock_w[i], lock_h, lock_icons[i], lock_active[i]);
    }

    rotate_canvas(canvas);
}
