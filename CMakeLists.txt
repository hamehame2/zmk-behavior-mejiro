# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)

if (CONFIG_ZMK_BEHAVIOR_MEJIRO)
  # Build as part of the ZMK app target (same strategy as zmk-naginata),
  # so ZMK include paths like <drivers/zmk/behavior.h> are available.
  target_sources(app PRIVATE
    src/behaviors/behavior_mejiro.c
  )
endif()

# Expose module include/ (if you add headers later)
zephyr_include_directories(include)
