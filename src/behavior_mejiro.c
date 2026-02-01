/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>

#include "mejiro_engine.h"

LOG_MODULE_DECLARE(mejiro, CONFIG_ZMK_BEHAVIOR_MEJIRO_LOG_LEVEL);

struct behavior_mejiro_config {
    const char *label;
};

static int on_binding_pressed(struct zmk_behavior_binding *binding,
                             struct zmk_behavior_binding_event event) {
    (void)event;
    uint8_t key_id = (uint8_t)binding->param1;
    mj_engine_press(key_id);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_binding_released(struct zmk_behavior_binding *binding,
                              struct zmk_behavior_binding_event event) {
    (void)event;
    uint8_t key_id = (uint8_t)binding->param1;
    mj_engine_release(key_id);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct zmk_behavior_driver_api behavior_mejiro_driver_api = {
    .binding_pressed = on_binding_pressed,
    .binding_released = on_binding_released,
};

#define DT_DRV_COMPAT zmk_behavior_mejiro

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define MEJIRO_INST(n)                                                                  \
    static const struct behavior_mejiro_config behavior_mejiro_config_##n = {          \
        .label = DT_INST_PROP(n, label),                                               \
    };                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &behavior_mejiro_config_##n,          \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,         \
                            &behavior_mejiro_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MEJIRO_INST)

#endif
