/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>

void mj_engine_reset(void);

/* key_id: 0..23 from dt-bindings (lS..rX) */
void mj_engine_press(uint8_t key_id);
void mj_engine_release(uint8_t key_id);
