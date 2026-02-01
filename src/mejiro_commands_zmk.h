/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum mj_cmd_type {
    CMD_REPEAT = 0,
    CMD_UNDO   = 1,
    CMD_KEY    = 2,
    CMD_STRING = 3,
};

enum {
    MJ_MOD_CTRL  = 0x01,
    MJ_MOD_SHIFT = 0x02,
    MJ_MOD_ALT   = 0x04,
    MJ_MOD_GUI   = 0x08,
};

struct mj_cmd_key {
    uint32_t keycode;
    uint8_t mods;
};

struct mj_command {
    const char *pattern;
    enum mj_cmd_type type;
    union {
        struct mj_cmd_key key;
        const char *string;
    } action;
};

extern const struct mj_command mj_commands[];
extern const uint8_t mj_command_count;

const struct mj_command *mj_find_command(const char *stroke);
