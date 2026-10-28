/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "split_state.h"

struct peripheral_state synced_state;

__attribute__((weak)) void sync_state_draw(void) {}