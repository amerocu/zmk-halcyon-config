/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#include "poweroff.h"
#include "widgets/power_screen.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(!IS_ENABLED(CONFIG_SPLITKB_VIK_SLEEP),
             "Custom epaper power handling replaces the VIK sleep listener");
BUILD_ASSERT(CONFIG_KERNEL_INIT_PRIORITY_DEVICE < CONFIG_MIPI_DBI_INIT_PRIORITY &&
                 CONFIG_KERNEL_INIT_PRIORITY_DEVICE < CONFIG_DISPLAY_INIT_PRIORITY,
             "VIK power must be enabled before the DBI and display drivers initialize");

/* Mirror the module's VIK pin setup, but own the order explicitly: the panel
 * must complete its last waveform before control, RESET, CS or DC is released.
 * Listener link order is not a reliable way to synchronize those operations. */
#define VIK_NODE DT_NODELABEL(vik_sleep)
#define EACH_OUTPUT(node, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx),
#define CHILD_OUTPUTS(node)                                                                        \
    COND_CODE_1(DT_NODE_HAS_PROP(node, output_gpios),                                               \
                (DT_FOREACH_PROP_ELEM(node, output_gpios, EACH_OUTPUT)), ())
#define CHILD_INPUTS(node)                                                                         \
    COND_CODE_1(DT_NODE_HAS_PROP(node, input_gpios),                                                \
                (DT_FOREACH_PROP_ELEM(node, input_gpios, EACH_OUTPUT)), ())

static const struct gpio_dt_spec outputs[] = {DT_FOREACH_CHILD(VIK_NODE, CHILD_OUTPUTS)};
static const struct gpio_dt_spec inputs[] = {DT_FOREACH_CHILD(VIK_NODE, CHILD_INPUTS)};
static const struct gpio_dt_spec control = GPIO_DT_SPEC_GET_OR(VIK_NODE, control_gpios, {0});
static bool sleeping;

static void configure(const struct gpio_dt_spec *pins, size_t count, gpio_flags_t flags) {
    for (size_t i = 0; i < count; i++) {
        int err = gpio_pin_configure_dt(&pins[i], flags);
        if (err < 0) {
            LOG_WRN("epaper power: GPIO setup failed (%d)", err);
        }
    }
}

static void release_pins(void) {
    if (control.port) {
        configure(&control, 1, GPIO_INPUT | GPIO_PULL_DOWN);
    }
    configure(outputs, ARRAY_SIZE(outputs), GPIO_INPUT);
    configure(inputs, ARRAY_SIZE(inputs), GPIO_INPUT);
}

static int power_event(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *event = as_zmk_activity_state_changed(eh);
    if (event->state == ZMK_ACTIVITY_SLEEP && !sleeping) {
        sleeping = true;
        int err = epaper_power_screen_show(split_poweroff_requested());
        if (err < 0) {
            LOG_ERR("epaper final screen failed (%d)", err);
        }
        release_pins();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(epaper_power, power_event);
ZMK_SUBSCRIPTION(epaper_power, zmk_activity_state_changed);

static int power_init(void) {
    /* Preserve the original VIK handler's early power-up. APPLICATION is too
     * late: the SSD1680 has already tried to reset/configure an unpowered panel.
     * Only enable the supply here; RESET, CS, DC and BUSY belong to the drivers
     * until the final shutdown refresh has completed. */
    if (control.port) {
        if (!gpio_is_ready_dt(&control)) {
            LOG_ERR("epaper VIK power GPIO is not ready");
            return -ENODEV;
        }
        int err = gpio_pin_configure_dt(&control, GPIO_OUTPUT_ACTIVE);
        if (err < 0) {
            LOG_ERR("epaper VIK power enable failed (%d)", err);
        }
        return err;
    }
    return 0;
}

SYS_INIT(power_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
