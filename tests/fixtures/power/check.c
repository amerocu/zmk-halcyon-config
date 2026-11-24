/* SPDX-License-Identifier: MIT */
#include "mock.h"
#include "../../../boards/shields/mod_split_state_sync/src/poweroff.c"
#if TEST_DISPLAY
#include "../../../boards/shields/mod_display_epaper_custom/power.c"
#endif
#include "../../../boards/shields/mod_display_epaper_custom/widgets/power_art.c"

const int mock_port;
static struct k_work *queued;
static int submissions;
static int submit_error;
static int event_count;
static int off_count;
static int screen_count;
static bool last_off;
static bool rendered;
static gpio_flags_t pins[5];
static unsigned int pin_configurations[5];
static bool gpio_ready = true;
static int gpio_error;

int k_work_submit(struct k_work *work) {
    if (submit_error) {
        return submit_error;
    }
    queued = work;
    submissions++;
    return 1;
}

int gpio_pin_configure_dt(const struct gpio_dt_spec *pin, gpio_flags_t flags) {
    if (gpio_error) {
        return gpio_error;
    }
    /* No display control pin may be released until its refresh completes. */
    if (pin->pin < 4 && flags & GPIO_INPUT) {
        assert(rendered);
    }
    pins[pin->pin] = flags;
    pin_configurations[pin->pin]++;
    return 0;
}

bool gpio_is_ready_dt(const struct gpio_dt_spec *pin) { return gpio_ready; }

#if TEST_DISPLAY
static void check_boot(void) {
    memset(pins, 0, sizeof(pins));
    memset(pin_configurations, 0, sizeof(pin_configurations));
    /* Execute the production SYS_INIT registration in its actual level and
     * priority, interleaved with simulated DBI and panel initialization. A late
     * APPLICATION callback must fail here, even if its GPIO calls look right. */
    for (int level = POST_KERNEL; level <= APPLICATION; level++) {
        for (int priority = 0; priority < 100; priority++) {
            if (mock_init_power_init.level == level && mock_init_power_init.priority == priority) {
                assert(mock_init_power_init.callback() == 0);
            }
            if (level == POST_KERNEL && priority == CONFIG_MIPI_DBI_INIT_PRIORITY) {
                assert(pins[0] == GPIO_OUTPUT_ACTIVE && "VIK power must precede DBI initialization");
                for (int pin = 1; pin < 5; pin++) {
                    assert(pin_configurations[pin] == 0 && "Boot power callback must not touch bus pins");
                    const struct gpio_dt_spec spec = {.port = &mock_port, .pin = pin};
                    gpio_pin_configure_dt(&spec, pin == 4 ? GPIO_INPUT : GPIO_OUTPUT_INACTIVE);
                }
            }
            if (level == POST_KERNEL && priority == CONFIG_DISPLAY_INIT_PRIORITY) {
                assert(pins[0] == GPIO_OUTPUT_ACTIVE && "Panel must be powered before reset/configuration");
            }
        }
    }
    for (int pin = 0; pin < 5; pin++) {
        assert(pin_configurations[pin] == 1);
    }
}

static void check_boot_errors(void) {
    const unsigned int before = pin_configurations[0];
    gpio_ready = false;
    assert(mock_init_power_init.callback() == -ENODEV);
    assert(pin_configurations[0] == before);
    gpio_ready = true;
    gpio_error = -EIO;
    assert(mock_init_power_init.callback() == -EIO);
    assert(pin_configurations[0] == before);
    gpio_error = 0;
}
#endif

int epaper_power_screen_show(bool off) {
    assert(pins[0] == GPIO_OUTPUT_ACTIVE);
    for (int i = 1; i < 4; i++) {
        assert(pins[i] == GPIO_OUTPUT_INACTIVE);
    }
    screen_count++;
    last_off = off;
    rendered = true;
    return 0;
}

int raise_zmk_activity_state_changed(struct zmk_activity_state_changed event) {
    assert(event.state == ZMK_ACTIVITY_SLEEP);
    assert(split_poweroff_requested());
    event_count++;
#if TEST_DISPLAY
    return power_event(&event);
#else
    return 0;
#endif
}

int zmk_pm_soft_off(void) {
    assert(event_count == 1);
#if TEST_DISPLAY
    assert(rendered && last_off && screen_count == 1);
    assert(pins[0] == (GPIO_INPUT | GPIO_PULL_DOWN));
    for (int i = 1; i < 5; i++) {
        assert(pins[i] == GPIO_INPUT);
    }
#endif
    off_count++;
    return 0;
}

static void check_art(void) {
    /* Guard both ends, vary the stride, and ensure repeated renders clear
     * stale pixels and never touch the row padding. */
    for (size_t stride = 23; stride <= 32; stride++) {
        uint8_t storage[2 + 32 * POWER_ART_WIDTH];
        uint8_t original[32 * POWER_ART_WIDTH];
        const size_t size = stride * POWER_ART_WIDTH;
        memset(storage, 0xa5, sizeof(storage));
        power_art_render(storage + 1, stride, false);
        assert(storage[0] == 0xa5 && storage[size + 1] == 0xa5);
        memcpy(original, storage + 1, size);
        unsigned int ink = 0;
        for (int row = 0; row < POWER_ART_WIDTH; row++) {
            for (size_t col = 0; col < stride; col++) {
                const uint8_t value = storage[1 + row * stride + col];
                if (col >= POWER_ART_HEIGHT / 8) {
                    assert(value == 0);
                }
                ink += value != 0;
            }
        }
        assert(ink > 100);
        power_art_render(storage + 1, stride, true);
        assert(memcmp(original, storage + 1, size) != 0);
        assert(storage[0] == 0xa5 && storage[size + 1] == 0xa5);
        power_art_render(storage + 1, stride, false);
        assert(memcmp(original, storage + 1, size) == 0);
    }
}

int main(void) {
    check_art();
    assert(api.locality == BEHAVIOR_LOCALITY_GLOBAL);
    assert(!split_poweroff_requested());
#if TEST_DISPLAY
    check_boot();
    check_boot_errors();
    const zmk_event_t idle = {.state = ZMK_ACTIVITY_IDLE};
    power_event(&idle);
    assert(screen_count == 0);
    const zmk_event_t sleep = {.state = ZMK_ACTIVITY_SLEEP};
    power_event(&sleep);
    assert(screen_count == 1 && !last_off && rendered);
    power_event(&sleep);
    assert(screen_count == 1);
    /* Simulate the fresh boot that follows automatic sleep. */
    sleeping = false;
    rendered = false;
    screen_count = 0;
    check_boot();
#endif
    struct zmk_behavior_binding binding = {0};
    struct zmk_behavior_binding_event event = {0};
    api.binding_pressed(&binding, event);
    assert(submissions == 0 && !split_poweroff_requested());
#if CONFIG_ZMK_SPLIT_ROLE_CENTRAL
    api.binding_released(&binding, event);
    assert(submissions == 0 && off_count == 0 && !split_poweroff_requested());
#else
    submit_error = -EIO;
    assert(api.binding_released(&binding, event) == -EIO);
    assert(!split_poweroff_requested());
    submit_error = 0;
    api.binding_released(&binding, event);
    assert(split_poweroff_requested() && submissions == 1 && off_count == 0);
    api.binding_released(&binding, event);
    assert(submissions == 1);
    queued->handler(queued);
    assert(event_count == 1 && off_count == 1);
#endif
    puts("PASS: shutdown routing, final refresh before pin cleanup, and artwork buffers");
    return 0;
}
