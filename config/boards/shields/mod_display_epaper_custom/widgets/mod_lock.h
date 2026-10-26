/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#define HLC_MAC_LAYER 5

struct mod_lock_state {
    uint8_t mods;
    uint8_t leds;
    bool caps_word;
    bool mac;
};

void draw_modlock(lv_obj_t *canvas, const struct mod_lock_state *state);
