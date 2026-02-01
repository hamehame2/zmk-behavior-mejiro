/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "mejiro_commands_zmk.h"
#include <string.h>
#include <dt-bindings/zmk/keys.h>

/* Direct port of qmk keyboards/jeebis/mejiro31/mejiro_commands.c
 * - LCTL(KC_X) etc are expressed as (mods,keycode)
 */

const struct mj_command mj_commands[] = {
    /* Special */
    {"#-",     CMD_REPEAT, {.key = {.keycode = 0, .mods = 0}}},
    {"-U",     CMD_UNDO,   {.key = {.keycode = 0, .mods = 0}}},

    /* Ctrl shortcuts */
    {"Sk#-",   CMD_KEY, {.key = {.keycode = Z, .mods = MJ_MOD_CTRL}}},
    {"Kk#-",   CMD_KEY, {.key = {.keycode = X, .mods = MJ_MOD_CTRL}}},
    {"Nk#-",   CMD_KEY, {.key = {.keycode = C, .mods = MJ_MOD_CTRL}}},
    {"Ak#-",   CMD_KEY, {.key = {.keycode = V, .mods = MJ_MOD_CTRL}}},
    {"Ank#-",  CMD_KEY, {.key = {.keycode = V, .mods = (MJ_MOD_CTRL | MJ_MOD_SHIFT)}}},

    /* Keycodes */
    {"-AU",    CMD_KEY, {.key = {.keycode = BSPC, .mods = 0}}},
    {"-IU",    CMD_KEY, {.key = {.keycode = DEL,  .mods = 0}}},
    {"-S",     CMD_KEY, {.key = {.keycode = ESC,  .mods = 0}}},

    {"-A",     CMD_KEY, {.key = {.keycode = LEFT,  .mods = 0}}},
    {"-N",     CMD_KEY, {.key = {.keycode = DOWN,  .mods = 0}}},
    {"-Y",     CMD_KEY, {.key = {.keycode = UP,    .mods = 0}}},
    {"-K",     CMD_KEY, {.key = {.keycode = RIGHT, .mods = 0}}},
    {"-I",     CMD_KEY, {.key = {.keycode = HOME,  .mods = 0}}},
    {"-T",     CMD_KEY, {.key = {.keycode = END,   .mods = 0}}},

    {"-An",    CMD_KEY, {.key = {.keycode = LEFT,  .mods = MJ_MOD_SHIFT}}},
    {"-Nn",    CMD_KEY, {.key = {.keycode = DOWN,  .mods = MJ_MOD_SHIFT}}},
    {"-Yn",    CMD_KEY, {.key = {.keycode = UP,    .mods = MJ_MOD_SHIFT}}},
    {"-Kn",    CMD_KEY, {.key = {.keycode = RIGHT, .mods = MJ_MOD_SHIFT}}},
    {"-In",    CMD_KEY, {.key = {.keycode = HOME,  .mods = MJ_MOD_SHIFT}}},
    {"-Tn",    CMD_KEY, {.key = {.keycode = END,   .mods = MJ_MOD_SHIFT}}},

    {"-n",     CMD_KEY, {.key = {.keycode = ENTER, .mods = 0}}},
    {"n-",     CMD_KEY, {.key = {.keycode = SPACE, .mods = 0}}},
    {"n-n",    CMD_KEY, {.key = {.keycode = TAB,   .mods = 0}}},
    {"-ntk",   CMD_KEY, {.key = {.keycode = F7,    .mods = 0}}},
    {"n-ntk",  CMD_KEY, {.key = {.keycode = F8,    .mods = 0}}},

    /* Strings */
    {"-YA",      CMD_STRING, {.string = """}},
    {"-NI",      CMD_STRING, {.string = "'"}},
    {"-TK",      CMD_STRING, {.string = "|"}},
    {"-IA",      CMD_STRING, {.string = ":"}},
    {"-NY",      CMD_STRING, {.string = "/"}},
    {"-TN",      CMD_STRING, {.string = "*"}},
    {"-KY",      CMD_STRING, {.string = "**"}},
    {"-TI",      CMD_STRING, {.string = "~"}},
    {"-YI",      CMD_STRING, {.string = "("}},
    {"-TY",      CMD_STRING, {.string = ")"}},
    {"-SYI",     CMD_STRING, {.string = "(("}},
    {"-STY",     CMD_STRING, {.string = "))"}},
    {"-NA",      CMD_STRING, {.string = "["}},
    {"-KN",      CMD_STRING, {.string = "]"}},
    {"-SNA",     CMD_STRING, {.string = "{"}},
    {"-SKN",     CMD_STRING, {.string = "}"}},
    {"-NYIA",    CMD_STRING, {.string = "<"}},
    {"-TKNY",    CMD_STRING, {.string = ">"}},
    {"-SNYIA",   CMD_STRING, {.string = "<<"}},
    {"-STKNY",   CMD_STRING, {.string = ">>"}},

    {"-SYA",     CMD_STRING, {.string = """{#Left}"}},
    {"-SNI",     CMD_STRING, {.string = "''{#Left}"}},
    {"-TYI",     CMD_STRING, {.string = "(){#Left}"}},
    {"-STYI",    CMD_STRING, {.string = "(()){#Left}"}},
    {"-KNA",     CMD_STRING, {.string = "[]{#Left}"}},
    {"-SKNA",    CMD_STRING, {.string = "{}{#Left}"}},
    {"-TKNYIA",  CMD_STRING, {.string = "<>{#Left}"}},
    {"-STKNYIA", CMD_STRING, {.string = "<<>>{#Left}"}},

    {"-TKIA",    CMD_STRING, {.string = "||"}},
    {"-KA",      CMD_STRING, {.string = "///"}},
    {"-TNI",     CMD_STRING, {.string = "|v"}},
    {"-KYA",     CMD_STRING, {.string = "|^"}},
    {"-IAU",     CMD_STRING, {.string = "<-"}},
    {"-STK",     CMD_STRING, {.string = "->"}},
    {"-STKIAU",  CMD_STRING, {.string = "<->"}},
    {"-NYA",     CMD_STRING, {.string = "v/"}},
    {"-TNY",     CMD_STRING, {.string = "/^"}},
    {"-NYI",     CMD_STRING, {.string = "^/"}},
    {"-KNY",     CMD_STRING, {.string = "/v"}},

    {"-nt",      CMD_STRING, {.string = "."}},
    {"-nk",      CMD_STRING, {.string = ","}},
    {"n-nt",     CMD_STRING, {.string = "?"}},
    {"n-nk",     CMD_STRING, {.string = "!"}},
};

const uint8_t mj_command_count = sizeof(mj_commands) / sizeof(mj_commands[0]);

const struct mj_command *mj_find_command(const char *stroke) {
    for (uint8_t i = 0; i < mj_command_count; i++) {
        if (!strcmp(mj_commands[i].pattern, stroke)) {
            return &mj_commands[i];
        }
    }
    return NULL;
}
