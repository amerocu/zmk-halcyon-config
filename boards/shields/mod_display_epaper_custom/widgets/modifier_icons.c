/*
 * Copyright (c) 2024 The ZMK Contributors
 * Copyright (c) 2024 Marcos Chow Castro
 * SPDX-License-Identifier: MIT
 *
 * Artwork and normal/inverted palettes from zmk-nice-oled:
 * https://github.com/mctechnology17/zmk-nice-oled/blob/46f824abb2bd41f1287c5c68abd14122af6042a3/boards/shields/nice_oled/assets/modifiers_sym.c
 * See LICENSE.zmk-nice-oled. Descriptors are adapted to LVGL 9.
 * Original 14x14 I1 pixels below are expanded to 28x28 at compile time.
 * Both variants retain the upstream palettes, with exact 2x2 pixel duplication.
 */

#include <lvgl.h>

#define WHITE 0xff, 0xff, 0xff, 0xff
#define BLACK 0x00, 0x00, 0x00, 0xff

/* Expand eight source bits into sixteen bits, without interpolation. */
#define DOUBLE_BITS(v)                                                                            \
    (((v) & 0x80 ? 0xc000 : 0) | ((v) & 0x40 ? 0x3000 : 0) |                                      \
     ((v) & 0x20 ? 0x0c00 : 0) | ((v) & 0x10 ? 0x0300 : 0) |                                      \
     ((v) & 0x08 ? 0x00c0 : 0) | ((v) & 0x04 ? 0x0030 : 0) |                                      \
     ((v) & 0x02 ? 0x000c : 0) | ((v) & 0x01 ? 0x0003 : 0))
#define DOUBLE_BYTE(v) (DOUBLE_BITS(v) >> 8), (DOUBLE_BITS(v) & 0xff)
#define DOUBLE_ROW(a, b) DOUBLE_BYTE(a), DOUBLE_BYTE(b), DOUBLE_BYTE(a), DOUBLE_BYTE(b)
#define DOUBLE_PIXELS(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab) \
    DOUBLE_ROW(a, b), DOUBLE_ROW(c, d), DOUBLE_ROW(e, f), DOUBLE_ROW(g, h),                           \
    DOUBLE_ROW(i, j), DOUBLE_ROW(k, l), DOUBLE_ROW(m, n), DOUBLE_ROW(o, p),                           \
    DOUBLE_ROW(q, r), DOUBLE_ROW(s, t), DOUBLE_ROW(u, v), DOUBLE_ROW(w, x),                           \
    DOUBLE_ROW(y, z), DOUBLE_ROW(aa, ab)

#define IMAGE(name)                                                                                \
    const lv_image_dsc_t name = {                                                                  \
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I1,                         \
                   .w = 28, .h = 28, .stride = 4},                                                 \
        .data_size = sizeof(name##_map), .data = name##_map,                                        \
    }

/* Upstream's white variants have identical pixels and reversed palettes. */
#define MODIFIER(name, ...)                                                                        \
    static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t name##_map[] = {           \
        WHITE, BLACK, DOUBLE_PIXELS(__VA_ARGS__)};                                                 \
    static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t name##_white_map[] = {     \
        BLACK, WHITE, DOUBLE_PIXELS(__VA_ARGS__)};                                                 \
    IMAGE(name);                                                                                  \
    IMAGE(name##_white)

MODIFIER(mod_control,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x80, 0x0c, 0xc0, 0x18, 0x60, 0x30, 0x30,
    0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

MODIFIER(mod_shift,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x80, 0x0c, 0xc0, 0x18, 0x60, 0x30, 0x30,
    0x78, 0x78, 0x08, 0x40, 0x08, 0x40, 0x08, 0x40, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x00);

MODIFIER(mod_alt,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0xf8, 0x4a, 0x20, 0x4a, 0x20,
    0x7a, 0x20, 0x4a, 0x20, 0x4b, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

MODIFIER(mod_win,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0b, 0xf0, 0x3b, 0xf0, 0x3b, 0xf0, 0x3b, 0xf0,
    0x00, 0x00, 0x3b, 0xf0, 0x3b, 0xf0, 0x3b, 0xf0, 0x03, 0xf0, 0x00, 0x30, 0x00, 0x00);

MODIFIER(mod_cmd,
    0x00, 0x00, 0x00, 0x00, 0x18, 0x60, 0x24, 0x90, 0x24, 0x90, 0x1f, 0xe0, 0x04, 0x80,
    0x04, 0x80, 0x1f, 0xe0, 0x24, 0x90, 0x24, 0x90, 0x18, 0x60, 0x00, 0x00, 0x00, 0x00);

MODIFIER(mod_opt,
    0x00, 0x00, 0x00, 0x00, 0x3c, 0xe0, 0x3c, 0xe0, 0x06, 0x00, 0x06, 0x00, 0x06, 0x00,
    0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x01, 0xe0, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00);
