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
extern void sync_state_draw(void);
