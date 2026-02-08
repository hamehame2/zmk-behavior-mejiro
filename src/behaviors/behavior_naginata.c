/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_naginata

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>

#include <zmk_naginata/nglist.h>
#include <zmk_naginata/nglistarray.h>
#include <zmk_naginata/naginata_func.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
extern int64_t timestamp;

#define NONE 0

// 薙刀式

// 31キーを32bitの各ビットに割り当てる
#define B_Q (1UL << 0)
#define B_W (1UL << 1)
#define B_E (1UL << 2)
#define B_R (1UL << 3)
#define B_T (1UL << 4)

#define B_Y (1UL << 5)
#define B_U (1UL << 6)
#define B_I (1UL << 7)
#define B_O (1UL << 8)
#define B_P (1UL << 9)

#define B_A (1UL << 10)
#define B_S (1UL << 11)
#define B_D (1UL << 12)
#define B_F (1UL << 13)
#define B_G (1UL << 14)

#define B_H (1UL << 15)
#define B_J (1UL << 16)
#define B_K (1UL << 17)
#define B_L (1UL << 18)
#define B_SEMI (1UL << 19)


#define B_Z (1UL << 20)
#define B_X (1UL << 21)
#define B_C (1UL << 22)
#define B_V (1UL << 23)
#define B_B (1UL << 24)

#define B_N (1UL << 25)
#define B_M (1UL << 26)
#define B_COMMA (1UL << 27)
#define B_DOT (1UL << 28)
#define B_SLASH (1UL << 29)

#define B_SPACE (1UL << 30)
#define B_SQT (1UL << 31)

static NGListArray nginput;
static uint32_t pressed_keys = 0UL; // 押しているキーのビットをたてる
static int8_t n_pressed_keys = 0;   // 押しているキーの数
static uint32_t chord_keys = 0UL;  // current chord (union of pressed keys for this stroke)

#define NG_WINDOWS 0
#define NG_MACOS 1
#define NG_LINUX 2
#define NG_IOS 3

// EEPROMに保存する設定
typedef union {
    uint8_t os : 2;  // 2 bits can store values 0-3 (NG_WINDOWS, NG_MACOS, NG_LINUX, NG_IOS)
    bool tategaki : true; // true: 縦書き, false: 横書き
} user_config_t;

extern user_config_t naginata_config;

// -----------------------------------------------------------------------------
// Mejiro "movement" (finalize-on-release) + debug output
//  - Keep Naginata module integration (&ng, dtsi/binding) untouched.
//  - Change ONLY the finalize timing and the output generation path.
//  - We build a Mejiro-like stroke string using the user's ng<->mejiro mapping,
//    but we output letters only for now (no '-' '#' '*' symbols) to avoid
//    keycode/shift issues during verification.
//    * hash '#'  -> 'Q' prefix (since Q key is mapped to '#')
//    * hyphen '-' -> 'X' separator between left and right (visual only)
//    * asterisk '*' -> 'P' suffix (since P key is mapped to '*')
// -----------------------------------------------------------------------------

static inline void tap_key(uint32_t keycode) {
    raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
}

static uint32_t keycode_from_ascii_letter(char c) {
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    switch (c) {
    case 'A': return A; case 'B': return B; case 'C': return C; case 'D': return D; case 'E': return E;
    case 'F': return F; case 'G': return G; case 'H': return H; case 'I': return I; case 'J': return J;
    case 'K': return K; case 'L': return L; case 'M': return M; case 'N': return N; case 'O': return O;
    case 'P': return P; case 'Q': return Q; case 'R': return R; case 'S': return S; case 'T': return T;
    case 'U': return U; case 'V': return V; case 'W': return W; case 'X': return X; case 'Y': return Y;
    case 'Z': return Z;
    default:  return NONE;
    }
}

static void send_ascii_letters_only(const char *s) {
    for (const char *p = s; *p; p++) {
        uint32_t kc = keycode_from_ascii_letter(*p);
        if (kc != NONE) {
            tap_key(kc);
        }
    }
}

static void send_mejiro_output(const char *mejiro_id);



// Build "Mejiro stroke" (letters-only) from ng chord bits.
// Output format (letters only):
//   [Q?] <Lconso STKN> <Lvowel YIAU> <Lparticle ntk> X <Rconso STKN> <Rvowel YIAU> <Rparticle ntk> [P?]
static void build_mejiro_id(uint32_t chord, char *out, size_t out_sz) {
    size_t n = 0;
    #define APP(ch) do { if (n + 1 < out_sz) out[n++] = (ch); } while (0)

    // --- Left: consonants STKN (A,W,S,D) + vowels YIAU (E,R,F,(T|G)) + particles ntk (C,V,B) + optional '#'(Q)
    if (chord & B_A) APP('S');   // A左S
    if (chord & B_W) APP('T');   // W左T
    if (chord & B_S) APP('K');   // S左K
    if (chord & B_D) APP('N');   // D左N

    if (chord & B_E) APP('Y');   // E左Y
    if (chord & B_R) APP('I');   // R左I
    if (chord & B_F) APP('A');   // F左A
    if (chord & (B_T | B_G)) APP('U'); // T左U, G左U

    if (chord & B_C) APP('n');   // C左n
    if (chord & B_V) APP('t');   // V左t
    if (chord & B_B) APP('k');   // B左k
    if (chord & B_Q) APP('#');   // Q左#

    APP('-');

    // --- Right: consonants STKN (SEMI,O,L,K) + vowels YIAU (I,U,J,(H|Y)) + particles ntk (COMMA,M,N) + optional '*'
    if (chord & B_SEMI) APP('S'); // SEMI右S
    if (chord & B_O) APP('T');    // O右T
    if (chord & B_L) APP('K');    // L右K
    if (chord & B_K) APP('N');    // K右N

    if (chord & B_I) APP('Y');    // I右Y
    if (chord & B_U) APP('I');    // U右I
    if (chord & B_J) APP('A');    // J右A
    if (chord & (B_H | B_Y)) APP('U'); // H右U, Y右U

    if (chord & B_COMMA) APP('n'); // COMMA右n
    if (chord & B_M) APP('t');     // M右t
    if (chord & B_N) APP('k');     // N右k
    if (chord & B_P) APP('*');     // P右*

    out[n] = '
