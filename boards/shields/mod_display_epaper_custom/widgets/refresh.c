/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "refresh.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define PANEL_WIDTH DT_PROP(DISPLAY_NODE, width)
#define PANEL_HEIGHT ROUND_DOWN(DT_PROP(DISPLAY_NODE, height), 8)

static const struct device *const panel = DEVICE_DT_GET(DISPLAY_NODE);
static struct display_capabilities capabilities;
static uint8_t pixels[DIV_ROUND_UP(PANEL_WIDTH, 8) * PANEL_HEIGHT];
static bool full_requested;
static bool full_in_progress;

/* LVGL I1 is row-packed, with an eight-byte palette. The SSD16xx's normal
 * orientation is vertically tiled instead. Preserve Zephyr's mono mapping:
 * index 1 is white, represented by a set bit in MONO10 or a clear bit in MONO01. */
static void convert_pixels(const uint8_t *source, uint16_t width, uint16_t height) {
    const bool mono10 = capabilities.current_pixel_format == PIXEL_FORMAT_MONO10;
    const bool vtiled = capabilities.screen_info & SCREEN_INFO_MONO_VTILED;
    const bool msb_first = capabilities.screen_info & SCREEN_INFO_MONO_MSB_FIRST;
    const uint32_t stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_I1);
    memset(pixels, mono10 ? 0x00 : 0xff, sizeof(pixels));
    source += 8;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            if (!(source[y * stride + x / 8] & (0x80 >> (x % 8)))) {
                continue;
            }
            const uint32_t index = vtiled ? (y / 8) * width + x
                                          : y * DIV_ROUND_UP(width, 8) + x / 8;
            const uint8_t position = vtiled ? y % 8 : x % 8;
            const uint8_t bit = BIT(msb_first ? 7 - position : position);
            if (mono10) {
                pixels[index] |= bit;
            } else {
                pixels[index] &= ~bit;
            }
        }
    }
}

static void flush_section(lv_display_t *display, const lv_area_t *area, uint8_t *map) {
    const uint16_t width = lv_area_get_width(area);
    const uint16_t height = lv_area_get_height(area);
    const bool last = lv_display_flush_is_last(display);
    __ASSERT(width * height / 8 <= sizeof(pixels), "Epaper window exceeds conversion buffer");
    __ASSERT(width % 8 == 0 &&
             (!(capabilities.screen_info & SCREEN_INFO_MONO_VTILED) || height % 8 == 0),
             "Epaper window must be aligned by the monochrome area rounder");

    if (full_requested && !full_in_progress) {
        int err = display_blanking_on(panel);
        if (err) {
            LOG_ERR("epaper full refresh: blanking failed (%d)", err);
            lv_display_flush_ready(display);
            return;
        }
        full_in_progress = true;
    }

    convert_pixels(map, width, height);
    const struct display_buffer_descriptor descriptor = {
        .buf_size = width * height / 8,
        .width = width,
        .height = height,
        .pitch = width,
        /* Every partial window is a complete update, even when LVGL has
         * more windows in this frame. Never enable blanking just because
         * this is not the last window: SSD16xx would select its full LUT. */
        .frame_incomplete = full_in_progress && !last,
    };

    LOG_DBG("epaper %s window (%d,%d)-(%d,%d), last=%d",
            full_in_progress ? "full" : "partial",
            area->x1, area->y1, area->x2, area->y2, last);
    int err = display_write(panel, area->x1, area->y1, &descriptor, pixels);
    if (err) {
        LOG_ERR("epaper window write failed (%d)", err);
    }

    if (last && full_in_progress) {
        err = display_blanking_off(panel);
        if (err) {
            LOG_ERR("epaper full refresh: unblanking failed (%d)", err);
        }
        full_in_progress = false;
        full_requested = false;
    }
    lv_display_flush_ready(display);
}

void epaper_refresh_init(void) {
    display_get_capabilities(panel, &capabilities);
    __ASSERT(capabilities.current_pixel_format == PIXEL_FORMAT_MONO10 ||
             capabilities.current_pixel_format == PIXEL_FORMAT_MONO01,
             "The custom epaper screen requires a monochrome display");
    /* Keep Zephyr's registered mono area-rounding callback. Only replace
     * presentation, not LVGL's partial rendering or alignment rules. */
    lv_display_set_flush_cb(lv_display_get_default(), flush_section);
}

void epaper_refresh_full(lv_obj_t *screen) {
    full_requested = true;
    lv_obj_invalidate(screen);
}
