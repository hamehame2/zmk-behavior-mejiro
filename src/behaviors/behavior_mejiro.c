\
/*
 * SPDX-License-Identifier: MIT
 *
 * Minimal "do nothing" ZMK behavior so your build unblocks.
 * - Provides a node labeled `mg` via behaviors/mejiro.dtsi
 * - Press/release handlers return 0 without emitting keycodes.
 *
 * Once the build is green, we can replace the body with real Mejiro logic.
 */

#define DT_DRV_COMPAT zmk_behavior_mejiro

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <drivers/zmk/behavior.h>

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_MEJIRO_LOG_LEVEL) && (CONFIG_ZMK_BEHAVIOR_MEJIRO_LOG_LEVEL > 0)
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(zmk_behavior_mejiro, CONFIG_ZMK_BEHAVIOR_MEJIRO_LOG_LEVEL);
#else
#define LOG_INF(...)
#define LOG_DBG(...)
#define LOG_ERR(...)
#endif

static int behavior_mejiro_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    LOG_DBG("pressed");
    return 0;
}

static int behavior_mejiro_released(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    LOG_DBG("released");
    return 0;
}

static const struct zmk_behavior_driver_api behavior_mejiro_driver_api = {
    .binding_pressed = behavior_mejiro_pressed,
    .binding_released = behavior_mejiro_released,
};

static int behavior_mejiro_init(const struct device *dev) {
    ARG_UNUSED(dev);
    LOG_INF("init");
    return 0;
}

/*
 * Instantiate one device for each DT instance.
 * This matches the style used by existing ZMK behaviors.
 */
#define MEJIRO_INST(n)                                                                          \
    DEVICE_DT_INST_DEFINE(n, behavior_mejiro_init, NULL, NULL, NULL, POST_KERNEL,               \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_mejiro_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MEJIRO_INST)
