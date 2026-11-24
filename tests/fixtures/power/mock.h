/* SPDX-License-Identifier: MIT */
#pragma once

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IS_ENABLED(value) (value)
#define CONFIG_SPLITKB_VIK_SLEEP 0
#define CONFIG_ZMK_BEHAVIOR_METADATA 0
#define CONFIG_APPLICATION_INIT_PRIORITY 90
#define CONFIG_KERNEL_INIT_PRIORITY_DEFAULT 40
#define CONFIG_KERNEL_INIT_PRIORITY_DEVICE 50
#define CONFIG_MIPI_DBI_INIT_PRIORITY 80
#define CONFIG_DISPLAY_INIT_PRIORITY 85
#define BUILD_ASSERT(condition, ...) _Static_assert(condition, "build assertion")
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define LOG_MODULE_DECLARE(...)
#define LOG_ERR(...) mock_log(__VA_ARGS__)
#define LOG_WRN(...) mock_log(__VA_ARGS__)
static inline void mock_log(const char *format, ...) {}

typedef int atomic_t;
static inline int atomic_get(atomic_t *value) { return *value; }
static inline void atomic_set(atomic_t *value, int next) { *value = next; }
static inline bool atomic_cas(atomic_t *value, int old, int next) {
    if (*value != old) {
        return false;
    }
    *value = next;
    return true;
}

struct k_work { void (*handler)(struct k_work *); };
#define K_WORK_DEFINE(name, callback) static struct k_work name = {.handler = callback}
int k_work_submit(struct k_work *work);

enum { ZMK_ACTIVITY_ACTIVE, ZMK_ACTIVITY_IDLE, ZMK_ACTIVITY_SLEEP };
struct zmk_activity_state_changed { int state; };
typedef struct zmk_activity_state_changed zmk_event_t;
static inline struct zmk_activity_state_changed *as_zmk_activity_state_changed(
    const zmk_event_t *event) {
    return (struct zmk_activity_state_changed *)event;
}
int raise_zmk_activity_state_changed(struct zmk_activity_state_changed event);
int zmk_pm_soft_off(void);
#define ZMK_EV_EVENT_BUBBLE 0
#define ZMK_LISTENER(...)
#define ZMK_SUBSCRIPTION(...)
enum { POST_KERNEL, APPLICATION };
struct mock_init_entry {
    int (*callback)(void);
    int level;
    int priority;
};
#define SYS_INIT(callback, level, priority)                                                        \
    static const struct mock_init_entry mock_init_##callback = {callback, level, priority}

struct zmk_behavior_binding { int unused; };
struct zmk_behavior_binding_event { int unused; };
enum { BEHAVIOR_LOCALITY_GLOBAL = 2, ZMK_BEHAVIOR_OPAQUE = 0 };
struct behavior_driver_api {
    int locality;
    int (*binding_pressed)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
    int (*binding_released)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
};
#define DT_INST_FOREACH_STATUS_OKAY(...)

typedef int gpio_flags_t;
enum { GPIO_INPUT = 1, GPIO_PULL_DOWN = 2, GPIO_OUTPUT_ACTIVE = 4, GPIO_OUTPUT_INACTIVE = 8 };
struct gpio_dt_spec { const void *port; int pin; };
extern const int mock_port;
int gpio_pin_configure_dt(const struct gpio_dt_spec *pin, gpio_flags_t flags);
bool gpio_is_ready_dt(const struct gpio_dt_spec *pin);
#define DT_NODELABEL(name) name
#define DT_FOREACH_CHILD(node, callback) callback(epaper)
#define DT_NODE_HAS_PROP(node, prop) 1
#define UNPAREN(...) __VA_ARGS__
#define COND_CODE_1(condition, yes, no) UNPAREN yes
#define DT_FOREACH_PROP_ELEM(node, prop, callback) MOCK_##prop(node, callback)
#define MOCK_output_gpios(node, callback)                                                         \
    callback(node, output_gpios, 0) callback(node, output_gpios, 1) callback(node, output_gpios, 2)
#define MOCK_input_gpios(node, callback) callback(node, input_gpios, 0)
#define GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx) MOCK_GPIO_##prop(idx)
#define MOCK_GPIO_output_gpios(idx) {.port = &mock_port, .pin = 1 + idx}
#define MOCK_GPIO_input_gpios(idx) {.port = &mock_port, .pin = 4}
#define GPIO_DT_SPEC_GET_OR(node, prop, fallback) {.port = &mock_port, .pin = 0}
