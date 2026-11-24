/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POWER_ART_WIDTH 88
#define POWER_ART_HEIGHT 184

/* Render directly into native, row-packed I1 pixels (without a palette).
 * Native dimensions are 184 x 88, matching the status screen's rotation. */
void power_art_render(uint8_t *pixels, size_t stride, bool off);
