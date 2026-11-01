/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/sys/util.h>
#include "widgets/refresh.h"

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include "widgets/peripheral_status.h"
#else
#include "widgets/status.h"
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_HALCYON_EPAPER_WIDGET_STATUS)
static struct zmk_widget_status status_widget;
#endif

lv_obj_t *zmk_display_status_screen() {
    epaper_refresh_init();

    lv_obj_t *screen;
    screen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, LVGL_BACKGROUND, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

#if IS_ENABLED(CONFIG_HALCYON_EPAPER_WIDGET_STATUS)
    zmk_widget_status_init(&status_widget, screen);
    lv_obj_align(zmk_widget_status_obj(&status_widget), LV_ALIGN_TOP_LEFT, 0, 0);
#endif

    return screen;
}
