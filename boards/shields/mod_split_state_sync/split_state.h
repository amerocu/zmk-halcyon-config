/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct peripheral_state {
    uint8_t layer;
    uint8_t mods;
    bool caps_word;
    bool mac;
};

extern struct peripheral_state synced_state;

// Called by the GLOBAL behaviours after updating synced_state.  The display
// widgets provide a strong override; the default (weak) here is a no-op for
// headless centrals (e.g. the dongle).
void sync_state_draw(void);