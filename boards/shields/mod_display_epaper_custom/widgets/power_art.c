/* SPDX-License-Identifier: MIT */
/* Neko sprites: Copyright © 2022 adryd. See LICENSE.oneko and ../README.md. */
#include <string.h>
#include "power_art.h"

/* Exact monochrome crops of oneko.js/oneko.gif at commit
 * 5281d057c4ea9bd4f6f997ee96ba30491aed16c0: sleeping[0] (column 2, row 0)
 * and NE[1] (column 0, row 3), using zero-based 32 x 32 cells.
 * Transparent and white pixels become background; black pixels become '#'.
 * Each character becomes a crisp 2 x 2 square, without interpolation.
 * These source images live in flash, not the LVGL heap. */
static const char *const sleeping_cat[] = {
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "        ######                  ",
    "            #     ####          ",
    "          ##        #           ",
    "          #        #   ##       ",
    "        ######    ##### #       ",
    "               #     ## #       ",
    "              ###    #   #      ",
    "           #### ######   ##     ",
    "         ##  #   #  #    # #    ",
    "        #    #    ##      # #   ",
    "       #    #              ##   ",
    "       #    #              ##   ",
    "       #    #              ###  ",
    "      #     #            # # #  ",
    "      #      # ###      #  # #  ",
    "      #      #    ##   #   # ## ",
    "      #       #     #     # # # ",
    "      #        #      # ## ## # ",
    "       #        ########  ##  # ",
    "        ######      ####  #  ## ",
    "             ########  #######  ",
    "                                ",
    "                                ",
};

static const char *const departing_cat[] = {
    "             ###                ",
    "             #  #########       ",
    "             #   ####    #      ",
    "             #    #  # ####     ",
    "             #    #   #   ## ## ",
    "             #    #    #   ##   ",
    "             #    #        #    ",
    "             #             #    ",
    "          #####            #    ",
    "       ###     #          #     ",
    "      #                   #     ",
    "     #                  ##      ",
    "    #                  #        ",
    "    #                  #        ",
    "   #                   #        ",
    "  #                    #        ",
    "  #             ##    ##        ",
    "  #               #   #         ",
    "  #               ##  #         ",
    "  ##              #   #         ",
    "  ##              #  #          ",
    "  #              ##  #          ",
    " ##  #  ##      ## ##           ",
    " #  #### #     ##  #            ",
    "#  ## #  #    ### ##            ",
    "#  #  ######     ###            ",
    " ##        ####    #            ",
    "              ######            ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
};

static const char *const bye[] = {
    "##  # # ###",
    "# # # # #  ",
    "##   #  ## ",
    "# #  #  #  ",
    "##   #  ###",
};

static void pixel(uint8_t *pixels, size_t stride, int x, int y) {
    if (x < 0 || x >= POWER_ART_WIDTH || y < 0 || y >= POWER_ART_HEIGHT) {
        return;
    }
    /* Same 90-degree mapping as util.c, without an L8 scratch framebuffer. */
    pixels[(POWER_ART_WIDTH - 1 - x) * stride + y / 8] |= 0x80 >> (y % 8);
}

static void stamp(uint8_t *pixels, size_t stride, const char *const *rows, size_t count,
                  int left, int top, int scale) {
    for (size_t y = 0; y < count; y++) {
        for (size_t x = 0; rows[y][x]; x++) {
            if (rows[y][x] != '#') {
                continue;
            }
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    pixel(pixels, stride, left + x * scale + dx, top + y * scale + dy);
                }
            }
        }
    }
}

#define STAMP(image, left, top, scale)                                                              \
    stamp(pixels, stride, image, sizeof(image) / sizeof(image[0]), left, top, scale)

void power_art_render(uint8_t *pixels, size_t stride, bool off) {
    memset(pixels, 0, stride * POWER_ART_WIDTH);
    if (off) {
        STAMP(departing_cat, 12, 60, 2);
        STAMP(bye, 22, 133, 4);
    } else {
        /* The original sleeping frame already includes its own Zs. */
        STAMP(sleeping_cat, 12, 60, 2);
    }
}
