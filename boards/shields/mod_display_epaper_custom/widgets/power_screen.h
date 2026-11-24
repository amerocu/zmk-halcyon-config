/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>

/* Called outside the display queue. Returns only after the queued render and
 * the SSD1680's BUSY signal have completed, before its pins may be released. */
int epaper_power_screen_show(bool off);
