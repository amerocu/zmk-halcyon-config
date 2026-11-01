/* SPDX-License-Identifier: MIT */
#pragma once

#include <lvgl.h>

/* Install once before creating the status screen. Normal LVGL flushes update
 * individual windows; only this explicit full-refresh API enables blanking. */
void epaper_refresh_init(void);
void epaper_refresh_full(lv_obj_t *screen);
