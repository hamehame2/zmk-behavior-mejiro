/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_naginata

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

/* C standard types (Zephyr headers often include these indirectly, but keep it explicit) */
#include <stdbool.h>
#include <stddef.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>

#include <zmk_naginata/nglist.h>
#include <zmk_naginata/nglistarray.h>
#include <zmk_naginata/naginata_func.h>

/* --------------------------------------------------------------------------
 * Mejiro abbreviations/verbs (enabled)
 *
 *  - These are pure string transforms and do not depend on QMK.
 *  - Output is UTF-8 (Japanese), which ZMK will pass through to the host IME.
 * -------------------------------------------------------------------------- */

typedef struct {
    char output[256];
    bool success;
} abbreviation_result_t;

typedef struct {
    char output[128];
    bool success;
} verb_result_t;

// ユーザー略語のマッピング（完全なストローク形式）
typedef struct {
    const char *stroke;
    const char *output;
} user_abbreviation_t;

static const user_abbreviation_t user_abbreviations[] = {
    {"A-SKNIA", "あめりか"},
    {"KNUntk-KNU", "ぐーぐる"},
    {"KAUn-TAntk", "こんぴゅーたー"},
    {"SKNIA-SAUtk", "めそっど"},
    {"TNIA-SNI", "でじたる"},
    {"SU-STKNAU", "すまーとふぉん"},
    {"SU-TKAU", "すまほ"},
    {"STKU-STA", "ぷらすちっく"},
    {"KI-TKNAU", "きーぼーど"},
    {"In-STA", "いんふら"},
    {"KAUn-TKNIn", "こんびに"},
    {"SI-KNAUt", "しごと"},
    {"SI-SU", "しすてむ"},
    {"SAU-TKUt", "そふと"},
    {"SAU-SKIA", "そふとうぇあ"},
    {"TKA-SKIA", "はーどうぇあ"},
    {"In-NIAtk", "いんたーねっと"},
    {"In-STKNAU", "いんふぉめーしょん"},
    {"KAU-SKNYU", "こみゅにけーしょん"},
    {"SI-SKNYU", "しみゅれーしょん"},
    {"IU-STU", "ういるす"},
    {"KAU-IU", "ころなういるす"},
    {"SIn-KNAt", "しんがた"},
    {"A-STI", "ありがとう"},
    {"AU-NIA", "おねがい"},
    {"YAU-STAU", "よろしく"},
    {"TA-TAU", "たとえば"},
    {"KNU-TY", "ぐたいてきには"},
    {"TA-SI", "たしかに"},
    {NULL, NULL}
};

// 一般略語の完全一致マッピング
typedef struct {
    const char *stroke;
    const char *output;
} abstract_abbreviation_t;

static const abstract_abbreviation_t abstract_abbreviations[] = {
    {"A-STIA", "あれ"},
    {"KAU-STIA", "これ"},
    {"SAU-STIA", "それ"},
    {"TNAU-STIA", "どれ"},
    {"TNA-STIA", "だれ"},
    {"A-TNA", "あれだけ"},
    {"KAU-TNA", "これだけ"},
    {"SAU-TNA", "それだけ"},
    {"TNAU-TNA", "どれだけ"},
    {"TNA-TNA", "だれだけ"},
    {"A-IU", "ああいう"},
    {"KAU-IU", "こういう"},
    {"SAU-IU", "そういう"},
    {"TNAU-IU", "どういう"},
    {"NA-IU", "なんていう"},
    {"TAU-IU", "という"},
    {"TAU-KAU", "ところ"},
    {"KAU-TAU", "こと"},
    {"TKI-TAU", "ひと"},
    {"TAU-KI", "とき"},
    {"SKNAU-NAU", "もの"},
    {"TKNIA-TIA", "すべて"},
    {"NA-KNA", "ながら"},
    {"AU-KA", "おかげ"},
    {"SI-KNAU", "しごと"},
    {NULL, NULL}
};

// 一般略語の左側マッピング
typedef struct {
    const char *stroke;
    const char *output;
} abstract_left_t;

static const abstract_left_t abstract_left[] = {
    {"STN", ""},
    {"IAU", "あの"},
    {"KIAU", "この"},
    {"SIAU", "その"},
    {"TIAU", "との"},
    {"TNIAU", "どの"},
    {"NIAU", "なんの"},
    {"IU", "いう"},
    {"YIU", "ああいう"},
    {"KIU", "こういう"},
    {"SIU", "そういう"},
    {"TIU", "という"},
    {"TNIU", "どういう"},
    {"NIU", "なんていう"},
    {NULL, NULL}
};

// 一般略語の右側マッピング
typedef struct {
    const char *stroke;
    const char *output;
} abstract_right_t;

static const abstract_right_t abstract_right[] = {
    {"STN", ""},
    {"KAU", "こと"},
    {"STAU", "ころ"},
    {"KI", "とき"},
    {"TAU", "ところ"},
    {"TKI", "ひと"},
    {"TKA", "はなし"},
    {"TKIAU", "ほう"},
    {"SKNAU", "もの"},
    {"KNAU", "ものごと"},
    {"SI", "しごと"},
    {NULL, NULL}
};

// ユーザー略語を検索（完全なストローク）
abbreviation_result_t mejiro_user_abbreviation(const char *stroke) {
    abbreviation_result_t result = {{0}, false};
    
    for (size_t i = 0; user_abbreviations[i].stroke != NULL; i++) {
        if (strcmp(stroke, user_abbreviations[i].stroke) == 0) {
            strcpy(result.output, user_abbreviations[i].output);
            result.success = true;
            return result;
        }
    }
    
    return result;
}

// 一般略語を検索（完全なストローク）
abbreviation_result_t mejiro_abstract_abbreviation(const char *stroke) {
    abbreviation_result_t result = {{0}, false};
    
    // 完全一致を先にチェック
    for (size_t i = 0; abstract_abbreviations[i].stroke != NULL; i++) {
        if (strcmp(stroke, abstract_abbreviations[i].stroke) == 0) {
            strcpy(result.output, abstract_abbreviations[i].output);
            result.success = true;
            return result;
        }
    }
    
    // 左右の組み合わせをチェック
    // strokeを'-'で分割
    char left_part[32] = {0};
    char right_part[32] = {0};
    const char *hyphen = strchr(stroke, '-');
    if (hyphen != NULL) {
        size_t left_len = hyphen - stroke;
        strncpy(left_part, stroke, left_len);
        left_part[left_len] = '\0';
        strcpy(right_part, hyphen + 1);
        
        const char *left_output = NULL;
        const char *right_output = NULL;
        
        for (size_t i = 0; abstract_left[i].stroke != NULL; i++) {
            if (strcmp(left_part, abstract_left[i].stroke) == 0) {
                left_output = abstract_left[i].output;
                break;
            }
        }
        
        for (size_t i = 0; abstract_right[i].stroke != NULL; i++) {
            if (strcmp(right_part, abstract_right[i].stroke) == 0) {
                right_output = abstract_right[i].output;
                break;
            }
        }
        
        if (left_output != NULL && right_output != NULL) {
            strcpy(result.output, left_output);
            strcat(result.output, right_output);
            result.success = true;
        }
    }
    
    return result;
}


/*
 * Mejiro verb conjugation system implementation
 */

// 五十音の行をインデックスに変換
static int gyou_to_index(char gyou) {
    switch (gyou) {
        case 'k': return 0;
        case 'g': return 1;
        case 's':
        case 'z': return 2;
        case 't': return 3;
        case 'n': return 4;
        case 'b': return 5;
        case 'm': return 6;
        case 'r': return 7;
        case 'w': return 8;
        default: return 9;  // dummy 行
    }
}

// 下一段活用向けの行インデックス変換
static int gyou_to_index_simo(char gyou) {
    switch (gyou) {
        case 'k': return 0;
        case 'g': return 1;
        case 's': return 2;
        case 'z': return 3;
        case 't': return 4;
        case 'd': return 5;
        case 'n': return 6;
        case 'h': return 7;
        case 'b': return 8;
        case 'm': return 9;
        case 'r': return 10;
        case 'w': return 11;
        default: return 11;  // fallback to w行
    }
}

// 仮名の先頭文字から行を判定
static char kana_to_gyou(const char *kana) {
    if (kana == NULL || kana[0] == '
