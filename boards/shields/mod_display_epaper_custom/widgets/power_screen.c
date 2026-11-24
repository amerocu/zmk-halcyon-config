/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <lvgl.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zmk/display.h>

#include "power_art.h"
#include "power_screen.h"
#include "refresh.h"
#include "util.h"

BUILD_ASSERT(IS_ENABLED(CONFIG_ZMK_DISPLAY_WORK_QUEUE_DEDICATED),
             "Final epaper refresh requires a dedicated display queue");

static const struct gpio_dt_spec busy =
    GPIO_DT_SPEC_GET(DT_CHOSEN(zephyr_display), busy_gpios);
static uint8_t buffer[8 + LV_CANVAS_BUF_SIZE(POWER_ART_HEIGHT, POWER_ART_WIDTH, 1,
                                           LV_DRAW_BUF_STRIDE_ALIGN)] __aligned(LV_DRAW_BUF_ALIGN);
static bool show_off;
static int result;
static lv_obj_t *power_screen;
K_SEM_DEFINE(power_screen_done, 0, 1);

static void show_cb(struct k_work *work) {
    if (!zmk_display_is_initialized()) {
        result = -ENODEV;
        goto done;
    }

    /* Loading a separate root prevents late battery/layer/disconnect updates
     * to the normal widgets from overwriting the final image. */
    if (power_screen == NULL) {
        power_screen = lv_obj_create(NULL);
        if (power_screen == NULL) {
            result = -ENOMEM;
            goto done;
        }
        lv_obj_remove_style_all(power_screen);
        lv_obj_set_style_bg_color(power_screen, LVGL_BACKGROUND, 0);
        lv_obj_set_style_bg_opa(power_screen, LV_OPA_COVER, 0);
        lv_obj_remove_flag(power_screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *canvas = lv_canvas_create(power_screen);
        if (canvas == NULL) {
            lv_obj_delete(power_screen);
            power_screen = NULL;
            result = -ENOMEM;
            goto done;
        }
        lv_canvas_set_buffer(canvas, buffer, POWER_ART_HEIGHT, POWER_ART_WIDTH,
                             LV_COLOR_FORMAT_I1);
        lv_canvas_set_palette(canvas, 0, lv_color_to_32(LVGL_BACKGROUND, LV_OPA_COVER));
        lv_canvas_set_palette(canvas, 1, lv_color_to_32(LVGL_FOREGROUND, LV_OPA_COVER));
    }
    const size_t stride = lv_draw_buf_width_to_stride(POWER_ART_HEIGHT, LV_COLOR_FORMAT_I1);
    power_art_render(buffer + 8, stride, show_off);
    lv_screen_load(power_screen);
    epaper_refresh_full(power_screen);
    lv_refr_now(lv_display_get_default());

    /* The final full-refresh command returns before the panel finishes its
     * waveform. Observe BUSY, not just LVGL's flush-ready notification. */
    const int64_t deadline = k_uptime_get() + 10000;
    k_sleep(K_MSEC(10));
    while ((result = gpio_pin_get_dt(&busy)) > 0) {
        if (k_uptime_get() >= deadline) {
            result = -ETIMEDOUT;
            break;
        }
        k_sleep(K_MSEC(10));
    }
    if (result == 0) {
        result = epaper_refresh_result();
    }
done:
    k_sem_give(&power_screen_done);
}

K_WORK_DEFINE(power_screen_work, show_cb);

int epaper_power_screen_show(bool off) {
    show_off = off;
    int err = k_work_submit_to_queue(zmk_display_work_q(), &power_screen_work);
    if (err < 0) {
        return err;
    }
    k_sem_take(&power_screen_done, K_FOREVER);
    return result;
}
