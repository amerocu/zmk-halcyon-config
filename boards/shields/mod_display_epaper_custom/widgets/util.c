/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/kernel.h>
#include "util.h"
#include "refresh.h"

LV_IMG_DECLARE(bolt);

BUILD_ASSERT(STATUS_OFFSET + STATUS_HEIGHT <= LAYER_OFFSET);
BUILD_ASSERT(LAYER_OFFSET + LAYER_HEIGHT <= PROFILE_OFFSET);
BUILD_ASSERT(PROFILE_OFFSET + PROFILE_HEIGHT <= MODIFIER_OFFSET);
BUILD_ASSERT(MODIFIER_OFFSET + MODIFIER_HEIGHT == SCREEN_LENGTH);

/* In native display coordinates each section is height x 88. In the
 * portrait drawing coordinates it is 88 x height. Sections never overlap. */
void init_section(struct display_section *section, lv_obj_t *parent, void *buffer,
                  uint32_t height, lv_coord_t offset, section_draw_cb draw, const void *context) {
    __ASSERT(height <= MAX_SECTION_HEIGHT && offset >= 0 &&
             offset + height <= SCREEN_LENGTH, "Display section is outside the layout");
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_obj_remove_style_all(canvas);
    lv_canvas_set_buffer(canvas, buffer, height, CANVAS_SIZE, CANVAS_COLOR_FORMAT);
    lv_obj_set_size(canvas, height, CANVAS_SIZE);
    lv_obj_set_pos(canvas, offset, 0);
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    *section = (struct display_section){.canvas = canvas, .draw = draw, .context = context};
}

void redraw_section(struct display_section *section) {
    if (section->draw != NULL) {
        section->draw(section->canvas, section->context);
    }
}

void redraw_screen(struct display_section *sections, size_t count, lv_obj_t *screen) {
    for (size_t i = 0; i < count; i++) {
        redraw_section(&sections[i]);
    }
    epaper_refresh_full(screen);
}

/* Drawing runs synchronously on the display queue. Use one hidden scratch
 * canvas so drawing operations never resize/invalidate a visible canvas. */
static lv_obj_t *drawing_canvas;
static lv_obj_t *drawing_target;
static uint8_t drawing_buffer[SECTION_BUF_SIZE(MAX_SECTION_HEIGHT)];

lv_obj_t *begin_canvas(lv_obj_t *canvas) {
    if (drawing_canvas == NULL) {
        drawing_canvas = lv_canvas_create(lv_obj_get_parent(canvas));
        lv_obj_add_flag(drawing_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_style_all(drawing_canvas);
    }
    drawing_target = canvas;
    const uint32_t height = lv_canvas_get_draw_buf(canvas)->header.w;
    lv_canvas_set_buffer(drawing_canvas, drawing_buffer, CANVAS_SIZE, height,
                         CANVAS_COLOR_FORMAT);
    lv_canvas_fill_bg(drawing_canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    return drawing_canvas;
}

void rotate_canvas(lv_obj_t *canvas) {
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(canvas);
    const uint32_t width = draw_buf->header.w;
    const uint32_t height = draw_buf->header.h;
    const uint32_t src_stride = draw_buf->header.stride;
    lv_draw_buf_t *target_buf = lv_canvas_get_draw_buf(drawing_target);

    lv_draw_sw_rotate(draw_buf->data, target_buf->data, width, height,
                      src_stride, target_buf->header.stride,
                      LV_DISPLAY_ROTATION_90, CANVAS_COLOR_FORMAT);
    /* Only publish the final native-orientation image, once per section. */
    lv_obj_invalidate(drawing_target);
    drawing_target = NULL;
}

void draw_battery(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);

    canvas_draw_rect(canvas, 0, 2, 29, 12, &rect_white_dsc);
    canvas_draw_rect(canvas, 1, 3, 27, 10, &rect_black_dsc);
    canvas_draw_rect(canvas, 2, 4, (state->battery + 2) / 4, 8, &rect_white_dsc);
    canvas_draw_rect(canvas, 30, 5, 3, 6, &rect_white_dsc);
    canvas_draw_rect(canvas, 31, 6, 1, 4, &rect_black_dsc);

    if (state->charging) {
        lv_draw_image_dsc_t img_dsc;
        lv_draw_image_dsc_init(&img_dsc);
        canvas_draw_img(canvas, 9, -1, &bolt, &img_dsc);
    }
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align) {
    lv_draw_label_dsc_init(label_dsc);
    label_dsc->color = color;
    label_dsc->font = font;
    label_dsc->align = align;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color) {
    lv_draw_rect_dsc_init(rect_dsc);
    rect_dsc->bg_color = bg_color;
}

void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width) {
    lv_draw_line_dsc_init(line_dsc);
    line_dsc->color = color;
    line_dsc->width = width;
}

void init_arc_dsc(lv_draw_arc_dsc_t *arc_dsc, lv_color_t color, uint8_t width) {
    lv_draw_arc_dsc_init(arc_dsc);
    arc_dsc->color = color;
    arc_dsc->width = width;
}

void canvas_draw_line(lv_obj_t *canvas, const lv_point_t points[], uint32_t point_cnt,
                      lv_draw_line_dsc_t *draw_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    for (uint32_t i = 1; i < point_cnt; ++i) {
        draw_dsc->p1.x = points[i - 1].x;
        draw_dsc->p1.y = points[i - 1].y;
        draw_dsc->p2.x = points[i].x;
        draw_dsc->p2.y = points[i].y;
        lv_draw_line(&layer, draw_dsc);
    }

    lv_canvas_finish_layer(canvas, &layer);
}
void canvas_draw_rect(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                      lv_draw_rect_dsc_t *draw_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_area_t coords = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(&layer, draw_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}

void canvas_draw_arc(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t r,
                     int32_t start_angle, int32_t end_angle, lv_draw_arc_dsc_t *draw_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    draw_dsc->center.x = x;
    draw_dsc->center.y = y;
    draw_dsc->radius = r;
    draw_dsc->start_angle = start_angle;
    draw_dsc->end_angle = end_angle;
    lv_draw_arc(&layer, draw_dsc);

    lv_canvas_finish_layer(canvas, &layer);
}

void canvas_draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t max_w,
                      lv_draw_label_dsc_t *draw_dsc, const char *txt) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    draw_dsc->text = txt;
    lv_area_t coords = {x, y, x + max_w, y + CANVAS_SIZE};
    lv_draw_label(&layer, draw_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}

void canvas_draw_img(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, const lv_image_dsc_t *src,
                     lv_draw_image_dsc_t *draw_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    draw_dsc->src = src;
    lv_area_t coords = {x, y, x + src->header.w - 1, y + src->header.h - 1};
    lv_draw_image(&layer, draw_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}
