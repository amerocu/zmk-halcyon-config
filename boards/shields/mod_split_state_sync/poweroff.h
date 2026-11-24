/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>

/* Set before the manual shutdown work is queued, including while the display
 * is finishing an earlier update. Automatic sleep leaves this false. */
bool split_poweroff_requested(void);
