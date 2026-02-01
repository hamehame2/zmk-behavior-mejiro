/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "mejiro_engine.h"
#include "mejiro_commands_zmk.h"
#include "mejiro_transform.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_REGISTER(mejiro, CONFIG_ZMK_BEHAVIOR_MEJIRO_LOG_LEVEL);

/* Chord capture as bitset:
 *  - bits 0..11  : left S,T,K,N,Y,I,A,U,n,t,k,#
 *  - bits 12..23 : right S,T,K,N,Y,I,A,U,n,t,k,*
 */
static uint32_t g_down_bits = 0;
static uint32_t g_latched_bits = 0;
static uint8_t  g_down_count = 0;

/* History for repeat/undo (same sizes as QMK) */
#define HISTORY_SIZE 20
static char    history_outputs[HISTORY_SIZE][64];
static uint8_t history_lengths[HISTORY_SIZE];
static uint8_t history_count = 0;
static bool    last_output_was_space = false;

/* ----- ZMK invoke helpers (internal &kp) ----- */
static int invoke_kp(uint32_t keycode, bool pressed, struct zmk_behavior_binding_event event) {
    struct zmk_behavior_binding b = {
        .behavior_dev = "kp",
        .param1 = keycode,
        .param2 = 0,
    };
    return zmk_behavior_invoke_binding(&b, event, pressed);
}

static void tap_key(uint32_t keycode, struct zmk_behavior_binding_event event) {
    (void)invoke_kp(keycode, true, event);
    (void)invoke_kp(keycode, false, event);
}

static void press_mods(uint8_t mods, struct zmk_behavior_binding_event event) {
    if (mods & MJ_MOD_CTRL)  (void)invoke_kp(LCTRL,  true, event);
    if (mods & MJ_MOD_SHIFT) (void)invoke_kp(LSHIFT, true, event);
    if (mods & MJ_MOD_ALT)   (void)invoke_kp(LALT,   true, event);
    if (mods & MJ_MOD_GUI)   (void)invoke_kp(LGUI,   true, event);
}

static void release_mods(uint8_t mods, struct zmk_behavior_binding_event event) {
    if (mods & MJ_MOD_GUI)   (void)invoke_kp(LGUI,   false, event);
    if (mods & MJ_MOD_ALT)   (void)invoke_kp(LALT,   false, event);
    if (mods & MJ_MOD_SHIFT) (void)invoke_kp(LSHIFT, false, event);
    if (mods & MJ_MOD_CTRL)  (void)invoke_kp(LCTRL,  false, event);
}

/* ----- ASCII sending (JIS-aware minimal mapping for command strings) -----
 * Mejiro transform output is romaji, so letters + a few punctuation dominate.
 * For JIS host, several symbols require different keycodes.
 *
 * NOTE:
 *  - This mapping intentionally focuses on symbols used by the QMK command table.
 *  - If you need perfect JIS symbol coverage, extend map_ascii_jis().
 */
struct mj_key {
    uint32_t keycode;
    uint8_t mods; /* MJ_MOD_SHIFT etc */
    bool ok;
};

static struct mj_key map_ascii_us(char c) {
    struct mj_key k = {0, 0, true};

    if (c >= 'a' && c <= 'z') { k.keycode = (A + (c - 'a')); return k; }
    if (c >= 'A' && c <= 'Z') { k.keycode = (A + (c - 'A')); k.mods = MJ_MOD_SHIFT; return k; }

    if (c >= '1' && c <= '9') { k.keycode = (N1 + (c - '1')); return k; }
    if (c == '0') { k.keycode = N0; return k; }

    switch (c) {
    case ' ': k.keycode = SPACE; return k;
    case '\n': k.keycode = ENTER; return k;
    case '\t': k.keycode = TAB; return k;

    case '.': k.keycode = DOT; return k;
    case ',': k.keycode = COMMA; return k;
    case '/': k.keycode = FSLH; return k;
    case '-': k.keycode = MINUS; return k;

    case '!': k.keycode = N1; k.mods = MJ_MOD_SHIFT; return k;
    case '?': k.keycode = FSLH; k.mods = MJ_MOD_SHIFT; return k;
    case ':': k.keycode = SEMI; k.mods = MJ_MOD_SHIFT; return k;
    case ';': k.keycode = SEMI; return k;

    case '\'': k.keycode = SQT; return k;
    case '"': k.keycode = SQT; k.mods = MJ_MOD_SHIFT; return k;

    case '(': k.keycode = N9; k.mods = MJ_MOD_SHIFT; return k;
    case ')': k.keycode = N0; k.mods = MJ_MOD_SHIFT; return k;

    case '[': k.keycode = LBKT; return k;
    case ']': k.keycode = RBKT; return k;
    case '{': k.keycode = LBKT; k.mods = MJ_MOD_SHIFT; return k;
    case '}': k.keycode = RBKT; k.mods = MJ_MOD_SHIFT; return k;

    case '<': k.keycode = COMMA; k.mods = MJ_MOD_SHIFT; return k;
    case '>': k.keycode = DOT; k.mods = MJ_MOD_SHIFT; return k;

    case '*': k.keycode = N8; k.mods = MJ_MOD_SHIFT; return k;

    case '|': k.keycode = BSLH; k.mods = MJ_MOD_SHIFT; return k;

    case '^': k.keycode = N6; k.mods = MJ_MOD_SHIFT; return k;
    case '~': k.keycode = GRAVE; k.mods = MJ_MOD_SHIFT; return k;

    default:
        k.ok = false;
        return k;
    }
}

static struct mj_key map_ascii_jis(char c) {
    /* For letters/digits and many punctuation, US mapping works.
       For JIS-specific symbols in our command strings, apply JP translations.

       Mappings adapted from a commonly used ZMK JIS translation table:
         - '"' -> AT
         - '\'' -> AMPS (i.e., '&' key on US)
         - ':'  -> SQT (single-quote key)
         - '|'  -> NON_US_BSLH with shift (0x89 in many tables)
         - '['  -> RBKT
         - ']'  -> BSLH
         - '{'  -> RBKT with shift
         - '}'  -> BSLH with shift (approx)
       If your board/OS differs, this is the first place to tweak.
    */
    struct mj_key k = map_ascii_us(c);

#if defined(CONFIG_ZMK_BEHAVIOR_MEJIRO_HOST_JIS)
    switch (c) {
    case '"': k.keycode = AT; k.mods = 0; k.ok = true; return k;
    case '\'': k.keycode = AMPS; k.mods = 0; k.ok = true; return k;
    case ':':  k.keycode = SQT; k.mods = 0; k.ok = true; return k;

    case '[':  k.keycode = RBKT; k.mods = 0; k.ok = true; return k;
    case ']':  k.keycode = BSLH; k.mods = 0; k.ok = true; return k;
    case '{':  k.keycode = RBKT; k.mods = MJ_MOD_SHIFT; k.ok = true; return k;
    case '}':  k.keycode = BSLH; k.mods = MJ_MOD_SHIFT; k.ok = true; return k;

    case '|':  k.keycode = 0x89; k.mods = MJ_MOD_SHIFT; k.ok = true; return k; /* may need adjustment */
    default:
        return k;
    }
#else
    return k;
#endif
}

static struct mj_key map_ascii(char c) {
#if defined(CONFIG_ZMK_BEHAVIOR_MEJIRO_HOST_JIS)
    return map_ascii_jis(c);
#else
    return map_ascii_us(c);
#endif
}

static void send_backspace_times(uint8_t n, struct zmk_behavior_binding_event event) {
    for (uint8_t i = 0; i < n; i++) {
        tap_key(BSPC, event);
    }
}

static bool ends_with_space(const char *s) {
    size_t len = strlen(s);
    return (len > 0) && (s[len - 1] == ' ');
}

/* Supports {#Left} token */
static void send_string_with_tokens(const char *s, struct zmk_behavior_binding_event event) {
    for (size_t i = 0; s[i];) {
        if (s[i] == '{' && !strncmp(&s[i], "{#Left}", 7)) {
            tap_key(LEFT, event);
            i += 7;
            continue;
        }

        struct mj_key k = map_ascii(s[i]);
        if (k.ok) {
            press_mods(k.mods, event);
            tap_key(k.keycode, event);
            release_mods(k.mods, event);
        }
        i++;
    }
}

/* Build mejiro ID string from bits (exactly as QMK mejiro_fifo.c) */
static void build_mejiro_id(uint32_t bits, char *out, size_t out_sz) {
    static const char *labels_left[12]  = {"S","T","K","N","Y","I","A","U","n","t","k","#"};
    static const char *labels_right[12] = {"S","T","K","N","Y","I","A","U","n","t","k","*"};

    size_t pos = 0;

    for (uint8_t i = 0; i < 12; i++) {
        if (bits & (1UL << i)) {
            const char *p = labels_left[i];
            while (*p && pos + 1 < out_sz) out[pos++] = *p++;
        }
    }

    /* QMK always inserts '-' as a divider (even if one side is empty) when any chord exists */
    if (pos + 1 < out_sz) out[pos++] = '-';

    for (uint8_t i = 0; i < 12; i++) {
        uint8_t bit = 12 + i;
        if (bits & (1UL << bit)) {
            const char *p = labels_right[i];
            while (*p && pos + 1 < out_sz) out[pos++] = *p++;
        }
    }

    if (pos < out_sz) out[pos] = '\0';
    else out[out_sz - 1] = '\0';
}

static void history_push(const char *s) {
    if (history_count < HISTORY_SIZE) {
        strncpy(history_outputs[history_count], s, sizeof(history_outputs[0]) - 1);
        history_outputs[history_count][sizeof(history_outputs[0]) - 1] = '\0';
        history_lengths[history_count] = (uint8_t)strlen(history_outputs[history_count]);
        history_count++;
        return;
    }

    /* shift left */
    for (uint8_t j = 0; j < HISTORY_SIZE - 1; j++) {
        strcpy(history_outputs[j], history_outputs[j + 1]);
        history_lengths[j] = history_lengths[j + 1];
    }
    strncpy(history_outputs[HISTORY_SIZE - 1], s, sizeof(history_outputs[0]) - 1);
    history_outputs[HISTORY_SIZE - 1][sizeof(history_outputs[0]) - 1] = '\0';
    history_lengths[HISTORY_SIZE - 1] = (uint8_t)strlen(history_outputs[HISTORY_SIZE - 1]);
}

static void history_repeat_last(struct zmk_behavior_binding_event event, bool double_send) {
    if (history_count == 0) return;
    uint8_t idx = history_count - 1;
    send_string_with_tokens(history_outputs[idx], event);
    if (double_send) {
        send_string_with_tokens(history_outputs[idx], event);
    }
    last_output_was_space = ends_with_space(history_outputs[idx]);

    if (!double_send) {
        history_push(history_outputs[idx]);
    } else {
        /* store doubled */
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s%s", history_outputs[idx], history_outputs[idx]);
        history_push(tmp);
    }
}

static void convert_and_send(uint32_t bits, struct zmk_behavior_binding_event event) {
    char id[64];
    build_mejiro_id(bits, id, sizeof(id));

    if (id[0] == '\0') return;

    bool has_hash = (strchr(id, '#') != NULL);
    char without_hash[64] = {0};

    if (has_hash) {
        size_t j = 0;
        for (size_t i = 0; id[i] && j + 1 < sizeof(without_hash); i++) {
            if (id[i] != '#') without_hash[j++] = id[i];
        }
        without_hash[j] = '\0';
    }

    /* 1) command table lookup (original pattern first) */
    const struct mj_command *cmd = mj_find_command(id);
    if (cmd) {
        if (cmd->type == CMD_REPEAT) {
            history_repeat_last(event, false);
            return;
        } else if (cmd->type == CMD_UNDO) {
            if (history_count > 0) {
                uint8_t idx = history_count - 1;
                send_backspace_times(history_lengths[idx], event);
                history_count--;
            } else {
                send_backspace_times(2, event);
            }
            mejiro_clear_pending_tsu();
            return;
        } else if (cmd->type == CMD_KEY) {
            /* undo/backspace clears pending tsu */
            if (cmd->action.key.keycode == BSPC || cmd->action.key.keycode == DEL ||
                ((cmd->action.key.mods & MJ_MOD_CTRL) && cmd->action.key.keycode == Z)) {
                mejiro_clear_pending_tsu();
            }

            press_mods(cmd->action.key.mods, event);
            tap_key(cmd->action.key.keycode, event);
            release_mods(cmd->action.key.mods, event);

            if (cmd->action.key.keycode == SPACE) {
                last_output_was_space = true;
            } else {
                last_output_was_space = false;
            }
            return;
        } else if (cmd->type == CMD_STRING) {
            send_string_with_tokens(cmd->action.string, event);
            last_output_was_space = ends_with_space(cmd->action.string);
            history_push(cmd->action.string);
            return;
        }
    }

    /* 2) if has_hash, second command lookup with hash removed */
    if (has_hash && without_hash[0]) {
        const struct mj_command *cmd2 = mj_find_command(without_hash);
        if (cmd2 && cmd2->type == CMD_REPEAT) {
            history_repeat_last(event, true);
            return;
        }
        if (cmd2 && cmd2->type == CMD_STRING) {
            /* send twice */
            send_string_with_tokens(cmd2->action.string, event);
            send_string_with_tokens(cmd2->action.string, event);
            last_output_was_space = ends_with_space(cmd2->action.string);

            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%s%s", cmd2->action.string, cmd2->action.string);
            history_push(tmp);
            return;
        }
        if (cmd2 && cmd2->type == CMD_KEY) {
            press_mods(cmd2->action.key.mods, event);
            tap_key(cmd2->action.key.keycode, event);
            release_mods(cmd2->action.key.mods, event);
            /* repeat key */
            press_mods(cmd2->action.key.mods, event);
            tap_key(cmd2->action.key.keycode, event);
            release_mods(cmd2->action.key.mods, event);
            return;
        }
    }

    /* 3) transform: if has_hash, transform without_hash */
    const char *xform_id = (has_hash && without_hash[0]) ? without_hash : id;
    const mejiro_result_t r = mejiro_transform(xform_id);
    if (r.success) {
        send_string_with_tokens(r.output, event);
        last_output_was_space = ends_with_space(r.output);

        history_push(r.output);

        if (has_hash) {
            /* send same output again and store doubled */
            send_string_with_tokens(r.output, event);
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%s%s", r.output, r.output);
            history_push(tmp);
        }
        return;
    }

    /* 4) conversion failed: do nothing for now (passthrough can be added later) */
}

void mj_engine_reset(void) {
    g_down_bits = 0;
    g_latched_bits = 0;
    g_down_count = 0;
    history_count = 0;
    last_output_was_space = false;
}

void mj_engine_press(uint8_t key_id) {
    if (key_id >= 24) return;
    uint32_t bit = (1UL << key_id);

    g_down_bits |= bit;
    g_latched_bits |= bit;
    g_down_count++;
}

void mj_engine_release(uint8_t key_id) {
    if (key_id >= 24) return;
    uint32_t bit = (1UL << key_id);

    g_down_bits &= ~bit;
    if (g_down_count > 0) g_down_count--;

    if (g_down_count != 0) return;

    if (g_latched_bits == 0) {
        g_down_bits = 0;
        g_latched_bits = 0;
        return;
    }

    struct zmk_behavior_binding_event ev = {
        .position = 0,
        .timestamp = k_uptime_get(),
    };

    convert_and_send(g_latched_bits, ev);

    g_down_bits = 0;
    g_latched_bits = 0;
}
