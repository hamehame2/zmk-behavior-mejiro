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


// ---- Mejiro debug safe mode (0=normal,1=ASCII-only output,2=do not send output,3=bypass naginata_press/release) ----
static unsigned char g_mejiro_safe_mode = 2;

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
    if (kana == NULL || kana[0] == '\0') {
        return '\0';
    }

    if (strncmp(kana, "か", 3) == 0 || strncmp(kana, "き", 3) == 0 || strncmp(kana, "く", 3) == 0 ||
        strncmp(kana, "け", 3) == 0 || strncmp(kana, "こ", 3) == 0) {
        return 'k';
    }
    if (strncmp(kana, "が", 3) == 0 || strncmp(kana, "ぎ", 3) == 0 || strncmp(kana, "ぐ", 3) == 0 ||
        strncmp(kana, "げ", 3) == 0 || strncmp(kana, "ご", 3) == 0) {
        return 'g';
    }
    if (strncmp(kana, "さ", 3) == 0 || strncmp(kana, "し", 3) == 0 || strncmp(kana, "す", 3) == 0 ||
        strncmp(kana, "せ", 3) == 0 || strncmp(kana, "そ", 3) == 0) {
        return 's';
    }
    if (strncmp(kana, "ざ", 3) == 0 || strncmp(kana, "じ", 3) == 0 || strncmp(kana, "ず", 3) == 0 ||
        strncmp(kana, "ぜ", 3) == 0 || strncmp(kana, "ぞ", 3) == 0) {
        return 'z';
    }
    if (strncmp(kana, "た", 3) == 0 || strncmp(kana, "ち", 3) == 0 || strncmp(kana, "つ", 3) == 0 ||
        strncmp(kana, "て", 3) == 0 || strncmp(kana, "と", 3) == 0) {
        return 't';
    }
    if (strncmp(kana, "だ", 3) == 0 || strncmp(kana, "ぢ", 3) == 0 || strncmp(kana, "づ", 3) == 0 ||
        strncmp(kana, "で", 3) == 0 || strncmp(kana, "ど", 3) == 0) {
        return 'd';
    }
    if (strncmp(kana, "な", 3) == 0 || strncmp(kana, "に", 3) == 0 || strncmp(kana, "ぬ", 3) == 0 ||
        strncmp(kana, "ね", 3) == 0 || strncmp(kana, "の", 3) == 0) {
        return 'n';
    }
    if (strncmp(kana, "は", 3) == 0 || strncmp(kana, "ひ", 3) == 0 || strncmp(kana, "ふ", 3) == 0 ||
        strncmp(kana, "へ", 3) == 0 || strncmp(kana, "ほ", 3) == 0) {
        return 'h';
    }
    if (strncmp(kana, "ば", 3) == 0 || strncmp(kana, "び", 3) == 0 || strncmp(kana, "ぶ", 3) == 0 ||
        strncmp(kana, "べ", 3) == 0 || strncmp(kana, "ぼ", 3) == 0) {
        return 'b';
    }
    if (strncmp(kana, "ま", 3) == 0 || strncmp(kana, "み", 3) == 0 || strncmp(kana, "む", 3) == 0 ||
        strncmp(kana, "め", 3) == 0 || strncmp(kana, "も", 3) == 0) {
        return 'm';
    }
    if (strncmp(kana, "ら", 3) == 0 || strncmp(kana, "り", 3) == 0 || strncmp(kana, "る", 3) == 0 ||
        strncmp(kana, "れ", 3) == 0 || strncmp(kana, "ろ", 3) == 0) {
        return 'r';
    }
    if (strncmp(kana, "わ", 3) == 0 || strncmp(kana, "あ", 3) == 0 || strncmp(kana, "い", 3) == 0 ||
        strncmp(kana, "う", 3) == 0 || strncmp(kana, "え", 3) == 0 || strncmp(kana, "お", 3) == 0) {
        return 'w';
    }

    return '\0';
}

// 五段活用テーブル
// [行][活用形] = 語尾
static const char *godan_conjugate[10][10] = {
    // k行
    {"か", "か", "か", "き", "く", "い", "こう", "け", "け", "け"},
    // g行
    {"が", "が", "が", "ぎ", "ぐ", "い", "ごう", "げ", "げ", "げ"},
    // s行
    {"さ", "さ", "さ", "し", "す", "し", "そう", "せ", "せ", "せ"},
    // t行
    {"た", "た", "た", "ち", "つ", "っ", "とう", "て", "て", "て"},
    // n行
    {"な", "な", "な", "に", "ぬ", "ん", "のう", "ね", "ね", "ね"},
    // b行
    {"ば", "ば", "ば", "び", "ぶ", "ん", "ぼう", "べ", "べ", "べ"},
    // m行
    {"ま", "ま", "ま", "み", "む", "ん", "もう", "め", "め", "め"},
    // r行
    {"ら", "ら", "ら", "り", "る", "っ", "ろう", "れ", "れ", "れ"},
    // w行
    {"わ", "わ", "わ", "い", "う", "っ", "おう", "え", "え", "え"},
    // dummy
    {"", "", "", "", "", "", "", "", "", ""}
};

// 上一段活用テーブル
static const char *kami_conjugate[10][10] = {
    // k行
    {"き", "きさ", "きら", "き", "きる", "き", "きよう", "きれ", "きれ", "きろ"},
    // g行
    {"ぎ", "ぎさ", "ぎら", "ぎ", "ぎる", "ぎ", "ぎよう", "ぎれ", "ぎれ", "ぎろ"},
    // z行
    {"じ", "じさ", "じら", "じ", "じる", "じ", "じよう", "じれ", "じれ", "じろ"},
    // t行
    {"ち", "ちさ", "ちら", "ち", "ちる", "ち", "ちよう", "ちれ", "ちれ", "ちろ"},
    // n行
    {"に", "にさ", "にら", "に", "にる", "に", "によう", "にれ", "にれ", "にろ"},
    // b行
    {"び", "びさ", "びら", "び", "びる", "び", "びよう", "びれ", "びれ", "びろ"},
    // m行
    {"み", "みさ", "みら", "み", "みる", "み", "みよう", "みれ", "みれ", "みろ"},
    // r行
    {"り", "りさ", "りら", "り", "りる", "り", "りよう", "りれ", "りれ", "りろ"},
    // w行
    {"い", "いさ", "いら", "い", "いる", "い", "いよう", "いれ", "いれ", "いろ"},
    // dummy
    {"", "", "", "", "", "", "", "", "", ""}
};

// 下一段活用テーブル
static const char *simo_conjugate[12][10] = {
    // k行
    {"け", "けさ", "けら", "け", "ける", "け", "けよう", "けれ", "けれ", "けろ"},
    // g行
    {"げ", "げさ", "げら", "げ", "げる", "げ", "げよう", "げれ", "げれ", "げろ"},
    // s行
    {"せ", "せさ", "せら", "せ", "せる", "せ", "せよう", "せれ", "せれ", "せろ"},
    // z行
    {"ぜ", "ぜさ", "ぜら", "ぜ", "ぜる", "ぜ", "ぜよう", "ぜれ", "ぜれ", "ぜろ"},
    // t行
    {"て", "てさ", "てら", "て", "てる", "て", "てよう", "てれ", "てれ", "てろ"},
    // d行
    {"で", "でさ", "でら", "で", "でる", "で", "でよう", "でれ", "でれ", "でろ"},
    // n行
    {"ね", "ねさ", "ねら", "ね", "ねる", "ね", "ねよう", "ねれ", "ねれ", "ねろ"},
    // h行
    {"へ", "へさ", "へら", "へ", "へる", "へ", "へよう", "へれ", "へれ", "へろ"},
    // b行
    {"べ", "べさ", "べら", "べ", "べる", "べ", "べよう", "べれ", "べれ", "べろ"},
    // m行
    {"め", "めさ", "めら", "め", "める", "め", "めよう", "めれ", "めれ", "めろ"},
    // r行
    {"れ", "れさ", "れら", "れ", "れる", "れ", "れよう", "れれ", "れれ", "れろ"},
    // w行
    {"え", "えさ", "えら", "え", "える", "え", "えよう", "えれ", "えれ", "えろ"}
};

// サ変活用
static const char *sahen_conjugate[10] __attribute__((unused)) = {
    "し", "さ", "さ", "し", "する", "し", "しよう", "すれ", "でき", "しろ"
};
// カ変活用
static const char *kahen_conjugate[10] = {
    "こ", "こさ", "こら", "き", "くる", "き", "こよう", "くれ", "これ", "こい"
};
// 行く（五段特殊）
static const char *iku_conjugate[10] = {
    "いか", "いか", "いか", "いき", "いく", "いっ", "いこう", "いけ", "いけ", "いけ"
};
// ある（五段特殊）
static const char *aru_conjugate[10] = {
    "", "あら", "あら", "あり", "ある", "あっ", "あろう", "あれ", "ありえ", "あれ"
};

// 主要な動詞辞書 (Plover_Mejiroより統合)

// ------------------------------------------------------------
// Verb dictionary types (ported from QMK mejiro_verb.h)
// ------------------------------------------------------------
typedef enum {
    VERB_TYPE_GODAN,
    VERB_TYPE_KAMI,
    VERB_TYPE_SIMO,
    VERB_TYPE_SAHEN,
    VERB_TYPE_KAHEN,
    VERB_TYPE_SPECIAL
} verb_type_t;

// Conjugation forms (ported from QMK mejiro_verb.h)
typedef enum {
    CONJ_JISHO,   // 辞書形
    CONJ_NAI,     // ない形
    CONJ_TE_TA,   // て/た形
    CONJ_MASU,    // ます形
    CONJ_KANOU,   // 可能
    CONJ_KATEI,   // 仮定
    CONJ_IKOU,    // 意向
    CONJ_MEIREI,  // 命令
    CONJ_SHIEKI,  // 使役
    CONJ_UKEMI,   // 受身
} conj_form_t;

typedef struct {
    const char *stroke;
    const char *stem;
    const char gyou;
    const verb_type_t type;
} verb_entry_t;

static const verb_entry_t verb_dict[] = {
    // 五段活用 (k行)
    {"A-STU", "ある", 'k', VERB_TYPE_GODAN},       // 歩く
    {"I-TNA", "いただ", 'k', VERB_TYPE_GODAN},     // 頂く
    {"U-KNAU", "うご", 'k', VERB_TYPE_GODAN},      // 動く
    {"KA-YA", "かがや", 'k', VERB_TYPE_GODAN},     // 輝く
    {"KI-TU", "きがつ", 'k', VERB_TYPE_GODAN},     // 気が付く
    {"KI-TNU", "きづ", 'k', VERB_TYPE_GODAN},      // 気付く
    {"SI-KNAU", "しご", 'k', VERB_TYPE_GODAN},     // 扱く
    {"TA-TA", "たた", 'k', VERB_TYPE_GODAN},       // 叩く
    {"TN-KU", "つづ", 'k', VERB_TYPE_GODAN},       // 続く
    {"NAU-SNAU", "のぞ", 'k', VERB_TYPE_GODAN},    // 除く
    {"TKA-STA", "はたら", 'k', VERB_TYPE_GODAN},   // 働く
    {"SKNI-KNA", "みが", 'k', VERB_TYPE_GODAN},    // 磨く
    // 五段活用 (g行)
    {"I-SAU", "いそ", 'g', VERB_TYPE_GODAN},       // 急ぐ
    {"KA-SIA", "かせ", 'g', VERB_TYPE_GODAN},      // 稼ぐ
    {"KA-TU", "かつ", 'g', VERB_TYPE_GODAN},       // 担ぐ
    {"SA-SA", "ささ", 'g', VERB_TYPE_GODAN},       // 捧ぐ
    {"SA-SKA", "さわ", 'g', VERB_TYPE_GODAN},      // 騒ぐ
    {"TU-NA", "つな", 'g', VERB_TYPE_GODAN},       // 繋ぐ
    {"TKU-SA", "ふさ", 'g', VERB_TYPE_GODAN},      // 塞ぐ
    {"TKU-SIA", "ふせ", 'g', VERB_TYPE_GODAN},     // 防ぐ
    // 五段活用 (s行)
    {"A-KA", "あか", 's', VERB_TYPE_GODAN},        // 明かす
    {"I-KA", "いか", 's', VERB_TYPE_GODAN},        // 活かす
    {"I-TA", "いた", 's', VERB_TYPE_GODAN},        // 致す
    {"AU-TAU", "おと", 's', VERB_TYPE_GODAN},      // 落とす
    {"KA-KA", "かか", 's', VERB_TYPE_GODAN},       // 欠かす
    {"KA-KU", "かく", 's', VERB_TYPE_GODAN},       // 隠す
    {"KU-STA", "くら", 's', VERB_TYPE_GODAN},      // 暮らす
    {"KAU-STAU", "ころ", 's', VERB_TYPE_GODAN},    // 殺す
    {"KAU-SKA", "こわ", 's', VERB_TYPE_GODAN},     // 壊す
    {"TA-AU", "たお", 's', VERB_TYPE_GODAN},       // 倒す
    {"TU-TKNU", "つぶ", 's', VERB_TYPE_GODAN},     // 潰す
    {"TIA-STA", "てら", 's', VERB_TYPE_GODAN},     // 照らす
    {"NA-AU", "なお", 's', VERB_TYPE_GODAN},       // 直す
    {"NI-KNA", "にが", 's', VERB_TYPE_GODAN},      // 逃がす
    {"NAU-KAU", "のこ", 's', VERB_TYPE_GODAN},     // 残す
    {"NAU-TKNA", "のば", 's', VERB_TYPE_GODAN},    // 伸ばす
    {"TKA-NA", "はな", 's', VERB_TYPE_GODAN},      // 話す
    {"SKNA-KA", "まか", 's', VERB_TYPE_GODAN},     // 任す
    {"SKNI-NA", "みな", 's', VERB_TYPE_GODAN},     // 見なす
    {"YU-STA", "ゆら", 's', VERB_TYPE_GODAN},      // 揺らす
    {"SKA-TA", "わた", 's', VERB_TYPE_GODAN},      // 渡す
    // 五段活用 (t行)
    {"U-KNA", "うが", 't', VERB_TYPE_GODAN},       // 穿つ
    {"SAU-TNA", "そだ", 't', VERB_TYPE_GODAN},     // 育つ
    {"TA-SKNAU", "たも", 't', VERB_TYPE_GODAN},    // 保つ
    {"SKNIA-TNA", "めだ", 't', VERB_TYPE_GODAN},   // 目立つ
    // 五段活用 (b行)
    {"A-SAU", "あそ", 'b', VERB_TYPE_GODAN},       // 遊ぶ
    {"IA-STA", "えら", 'b', VERB_TYPE_GODAN},      // 選ぶ
    {"AU-YAU", "およ", 'b', VERB_TYPE_GODAN},      // 及ぶ
    {"NA-STA", "なら", 'b', VERB_TYPE_GODAN},      // 並ぶ
    {"TKA-KAU", "はこ", 'b', VERB_TYPE_GODAN},     // 運ぶ
    {"TKAU-STAU", "ほろ", 'b', VERB_TYPE_GODAN},   // 滅ぶ
    {"SKNA-NA", "まな", 'b', VERB_TYPE_GODAN},     // 学ぶ
    {"SKNU-SU", "むす", 'b', VERB_TYPE_GODAN},     // 結ぶ
    {"YAU-STAU", "よろこ", 'b', VERB_TYPE_GODAN},  // 喜ぶ
    // 五段活用 (m行)
    {"I-NA", "いな", 'm', VERB_TYPE_GODAN},        // 否む
    {"U-STA", "うらや", 'm', VERB_TYPE_GODAN},     // 羨む
    {"KA-KNA", "かが", 'm', VERB_TYPE_GODAN},      // 屈む
    {"KA-SU", "かす", 'm', VERB_TYPE_GODAN},       // 霞む
    {"SU-SU", "すす", 'm', VERB_TYPE_GODAN},       // 進む
    {"TA-NAU", "たの", 'm', VERB_TYPE_GODAN},      // 頼む
    {"TIU-TKNA", "ついば", 'm', VERB_TYPE_GODAN},  // 啄む
    {"TU-TU", "つつ", 'm', VERB_TYPE_GODAN},       // 包む
    {"TU-SKNA", "つま", 'm', VERB_TYPE_GODAN},     // 摘む
    {"NA-YA", "なや", 'm', VERB_TYPE_GODAN},       // 悩む
    {"NU-SU", "ぬす", 'm', VERB_TYPE_GODAN},       // 盗む
    {"NAU-SNAU", "のぞ", 'm', VERB_TYPE_GODAN},    // 望む
    {"TKA-SA", "はさ", 'm', VERB_TYPE_GODAN},      // 挟む
    {"TKA-STA", "はら", 'm', VERB_TYPE_GODAN},     // 孕む
    {"TKI-KAU", "ひっこ", 'm', VERB_TYPE_GODAN},   // 引っ込む
    {"TKU-KU", "ふく", 'm', VERB_TYPE_GODAN},      // 含む
    {"TKAU-TKAU", "ほほえ", 'm', VERB_TYPE_GODAN}, // 微笑む
    {"YA-SU", "やす", 'm', VERB_TYPE_GODAN},       // 休む
    // 五段活用 (r行)
    {"I-", "い", 'r', VERB_TYPE_GODAN},            // 要る
    {"KA-KNI", "かぎ", 'r', VERB_TYPE_GODAN},      // 限る
    {"KNA-TKNA", "がんば", 'r', VERB_TYPE_GODAN},  // 頑張る
    {"KNA-", "がんば", 'r', VERB_TYPE_GODAN},      // 頑張る
    {"KIA-", "け", 'r', VERB_TYPE_GODAN},          // 蹴る
    {"SYA-TKNIA", "しゃべ", 'r', VERB_TYPE_GODAN}, // しゃべる
    {"SU-TKNIA", "すべ", 'r', VERB_TYPE_GODAN},    // 滑る
    {"T-KA", "たすか", 'r', VERB_TYPE_GODAN},      // 助かる
    {"NA-", "な", 'r', VERB_TYPE_GODAN},           // なる
    {"TKY-", "はい", 'r', VERB_TYPE_GODAN},        // 入る
    {"TKA-TKNA", "はばか", 'r', VERB_TYPE_GODAN},  // 憚る
    {"YA-", "や", 'r', VERB_TYPE_GODAN},           // やる
    {"-YA", "や", 'r', VERB_TYPE_GODAN},           // やる
    {"SKA-", "わか", 'r', VERB_TYPE_GODAN},        // 分かる
    // 五段活用 (w行)
    {"-A", "あ", 'w', VERB_TYPE_GODAN},            // 会う
    {"A-STA", "あら", 'w', VERB_TYPE_GODAN},       // 洗う
    {"A-SAU", "あらそ", 'w', VERB_TYPE_GODAN},     // 争う
    {"IU-", "い", 'w', VERB_TYPE_GODAN},           // 言う
    {"I-SNA", "いざな", 'w', VERB_TYPE_GODAN},     // 誘う
    {"I-SKA", "いわ", 'w', VERB_TYPE_GODAN},       // 祝う
    {"U-SI", "うしな", 'w', VERB_TYPE_GODAN},      // 失う
    {"U-TA", "うた", 'w', VERB_TYPE_GODAN},        // 歌う
    {"U-KNA", "うたが", 'w', VERB_TYPE_GODAN},     // 疑う
    {"U-YA", "うやま", 'w', VERB_TYPE_GODAN},      // 敬う
    {"AU-SKNAU", "おも", 'w', VERB_TYPE_GODAN},    // 思う
    {"AU-", "おも", 'w', VERB_TYPE_GODAN},         // 思う
    {"KA-NA", "かな", 'w', VERB_TYPE_GODAN},       // 叶う
    {"KA-SKNA", "かま", 'w', VERB_TYPE_GODAN},     // 構う
    {"KU-STU", "くる", 'w', VERB_TYPE_GODAN},      // 狂う
    {"SI-A", "しあ", 'w', VERB_TYPE_GODAN},        // 仕合う
    {"SI-TA", "したが", 'w', VERB_TYPE_GODAN},     // 従う
    {"SI-SKNA", "しま", 'w', VERB_TYPE_GODAN},     // 仕舞う
    {"SAU-STAU", "そろ", 'w', VERB_TYPE_GODAN},    // 揃う
    {"TA-TA", "たたか", 'w', VERB_TYPE_GODAN},     // 戦う
    {"TI-KA", "ちか", 'w', VERB_TYPE_GODAN},       // 誓う
    {"TI-KNA", "ちが", 'w', VERB_TYPE_GODAN},      // 違う
    {"TU-KA", "つか", 'w', VERB_TYPE_GODAN},       // 使う
    {"TU-TI", "つちか", 'w', VERB_TYPE_GODAN},     // 培う
    {"TU-TNAU", "つど", 'w', VERB_TYPE_GODAN},     // 集う
    {"TIA-STA", "てら", 'w', VERB_TYPE_GODAN},     // 衒う
    {"TNIA-A", "であ", 'w', VERB_TYPE_GODAN},      // 出会う
    {"TAU-NA", "ともな", 'w', VERB_TYPE_GODAN},    // 伴う
    {"NA-STA", "なら", 'w', VERB_TYPE_GODAN},      // 習う
    {"NI-A", "にあ", 'w', VERB_TYPE_GODAN},        // 似合う
    {"NI-SKA", "にぎわ", 'w', VERB_TYPE_GODAN},    // 賑わう
    {"NIA-KNA", "ねが", 'w', VERB_TYPE_GODAN},     // 願う
    {"NAU-STAU", "のろ", 'w', VERB_TYPE_GODAN},    // 呪う
    {"TKA-STA", "はら", 'w', VERB_TYPE_GODAN},     // 払う
    {"TKI-STAU", "ひろ", 'w', VERB_TYPE_GODAN},    // 拾う
    {"SKNA-TAU", "まと", 'w', VERB_TYPE_GODAN},    // 纏う
    {"SKNI-A", "みあ", 'w', VERB_TYPE_GODAN},      // 見合う
    {"SKNU-KA", "むか", 'w', VERB_TYPE_GODAN},     // 向かう
    {"SKNAU-STA", "もら", 'w', VERB_TYPE_GODAN},   // 貰う
    {"SKA-STA", "わら", 'w', VERB_TYPE_GODAN},     // 笑う
    // 上一段活用 (k行)
    {"TN-KI", "で", 'k', VERB_TYPE_KAMI},          // 出来る
    // 上一段活用 (z行)
    {"IA-SNI", "えん", 'z', VERB_TYPE_KAMI},       // 演じる
    {"KA-SNI", "かん", 'z', VERB_TYPE_KAMI},       // 感じる
    {"KI-SNI", "きん", 'z', VERB_TYPE_KAMI},       // 禁じる
    {"SI-SNI", "しん", 'z', VERB_TYPE_KAMI},       // 信じる
    {"TNA-SNI", "だん", 'z', VERB_TYPE_KAMI},      // 断じる
    // 上一段活用 (m行)
    {"KA-SKNI", "かんが", 'm', VERB_TYPE_KAMI},    // 鑑みる
    // 下一段活用 (k行)
    {"T-KIA", "たす", 'k', VERB_TYPE_SIMO},        // 助ける
    {"TU-TN", "つづ", 'k', VERB_TYPE_SIMO},        // 続ける
    {"TN-KIA", "つづ", 'k', VERB_TYPE_SIMO},       // 続ける
    // 下一段活用 (g行)
    {"K-KNIA", "かか", 'g', VERB_TYPE_SIMO},       // 掲げる
    // 下一段活用 (s行)
    {"SKNA-KA", "まか", 's', VERB_TYPE_SIMO},      // 任せる
    // 下一段活用 (d行)
    {"TN-", "", 'd', VERB_TYPE_SIMO},              // 出る
    // 下一段活用 (b行)
    {"KU-STA", "くら", 'b', VERB_TYPE_SIMO},       // 比べる
    // 下一段活用 (m行)
    {"SA-TNA", "さだ", 'm', VERB_TYPE_SIMO},       // 定める
    {"TKA-SNI", "はじ", 'm', VERB_TYPE_SIMO},      // 始める
    {"SKNAU-TAU", "もと", 'm', VERB_TYPE_SIMO},    // 求める
    // 下一段活用 (r行)

    {"KAU-STIA", "こわ", 'r', VERB_TYPE_SIMO},     // 壊れる
    {"SKA-SU", "わす", 'r', VERB_TYPE_SIMO},       // 忘れる
    // 下一段活用 (w行)
    {"AU-SA", "おさ", 'w', VERB_TYPE_SIMO},        // 抑える
    {"AU-TKNAU", "おぼ", 'w', VERB_TYPE_SIMO},     // 覚える
    {"KA-KNA", "かんが", 'w', VERB_TYPE_SIMO},     // 考える
    {"KA-", "かんが", 'w', VERB_TYPE_SIMO},        // 考える
    {"KU-SKA", "くわ", 'w', VERB_TYPE_SIMO},       // 加える
    {"KAU-TA", "こた", 'w', VERB_TYPE_SIMO},       // 答える
    {"SA-SA", "ささ", 'w', VERB_TYPE_SIMO},        // 支える
    {"SKNA-KNA", "まちが", 'w', VERB_TYPE_SIMO},   // 間違える
    // 特殊活用
    {"I-K", "", '\0', VERB_TYPE_SPECIAL},          // 行く（特殊）
    {"A-", "", '\0', VERB_TYPE_SPECIAL},           // ある（特殊）
    {"K-", "", '\0', VERB_TYPE_KAHEN},             // 来る（カ変）
};

#define VERB_DICT_SIZE (sizeof(verb_dict) / sizeof(verb_dict[0]))

// 「です」の活用マップ
typedef struct {
    const char *particle;  // right_particle
    const char *output;    // 出力
} desu_conjugate_t;

static const desu_conjugate_t desu_conjugate[] = {
    {"", "です"},
    {"n", "でして"},
    {"t", "でした"},
    {"k", "でしょう"},
    {"nt", "です."},
    {"nk", "ですが"},
    {"tk", "ですか?"},
    {"ntk", "ですね"},
    {NULL, NULL}
};

// 助詞の追加音（ん/つ/く/っ/ち/き/ー）を取得
static const char *get_particle_extra(const char *particle) {
    static const char *particle_key_list[] = {"", "n", "t", "k", "tk", "nt", "nk", "ntk"};
    static const char *second_sound_list[] = {"", "ん", "つ", "く", "っ", "ち", "き", "ー"};

    for (size_t i = 0; i < sizeof(particle_key_list) / sizeof(particle_key_list[0]); i++) {
        if (strcmp(particle, particle_key_list[i]) == 0) {
            return second_sound_list[i];
        }
    }
    return "";
}

// 補助動詞・助動詞マップ
typedef struct {
    const char *pattern;  // left_particle-right_particle
    conj_form_t conj_form;        // 活用形
    const char *suffix;   // 接尾辞
} auxiliary_map_t;

static const auxiliary_map_t auxiliary_exception[] = {
    {"tk-", CONJ_KANOU, "る"},              // 可能
    {"tk-n", CONJ_KANOU, "ない"},           // 可能+否定
    {"tk-t", CONJ_KANOU, "た"},             // 可能+過去
    {"tk-k", CONJ_KANOU, "ます"},           // 可能+丁寧
    {"tk-nt", CONJ_KANOU, "なかった"},      // 可能+否定+過去
    {"tk-nk", CONJ_KANOU, "ません"},        // 可能+否定+丁寧
    {"tk-tk", CONJ_KANOU, "ました"},        // 可能+過去+丁寧
    {"tk-ntk", CONJ_KANOU, "て"},           // 可能+て
    {"ntk-", CONJ_MASU, ""},                // 連用
    {"ntk-n", CONJ_NAI, "ず"},              // 否定
    {"ntk-t", CONJ_KATEI, "ば"},            // 仮定
    {"ntk-k", CONJ_IKOU, ""},               // 意向
    {"ntk-nt", CONJ_NAI, "なければ"},       // 否定+仮定
    {"ntk-nk", CONJ_NAI, "なく"},           // 否定+連用
    {"ntk-tk", CONJ_TE_TA, "てください"},   // 丁寧命令
    {"ntk-ntk", CONJ_MEIREI, ""},           // 命令
    {"", -1, ""}  // 終端
};

// 左側補助動詞マップ: [活用形, 補助動詞の語幹, 活用段, 活用行]
typedef struct {
    const char *particle;
    conj_form_t conj_form;
    const char *stem;
    int verb_type;  // 1=五段, 2=上一段, 3=下一段
    char gyou;
} left_auxiliary_info_t;

static const left_auxiliary_info_t left_auxiliary[] = {
    {"n", CONJ_TE_TA, "て", 2, 'w'},      // ～ている
    {"t", CONJ_SHIEKI, "", 3, 's'},        // ～させる（使役）
    {"k", CONJ_UKEMI, "", 3, 'r'},        // ～られる（受身）
    {"nt", CONJ_TE_TA, "てもら", 1, 'w'}, // ～てもらう
    {"nk", CONJ_TE_TA, "てしま", 1, 'w'}, // ～てしまう
    {NULL, -1, "", 1, '\0'}
};

// 右側助動詞マップ
typedef struct {
    const char *particle;
    conj_form_t conj_form;
    const char *suffix;
} right_auxiliary_t;

static const right_auxiliary_t right_auxiliary[] = {
    {"", CONJ_JISHO, ""},
    {"n", CONJ_NAI, "ない"},
    {"t", CONJ_TE_TA, "た"},
    {"k", CONJ_MASU, "ます"},
    {"nt", CONJ_NAI, "なかった"},
    {"nk", CONJ_MASU, "ません"},
    {"tk", CONJ_MASU, "ました"},
    {"ntk", CONJ_TE_TA, "て"},
    {NULL, -1, ""}
};

// 左側補助動詞情報を取得
static const left_auxiliary_info_t *get_left_auxiliary(const char *particle) {
    if (particle == NULL) return NULL;
    for (int i = 0; left_auxiliary[i].particle != NULL; i++) {
        if (strcmp(particle, left_auxiliary[i].particle) == 0) {
            return &left_auxiliary[i];
        }
    }
    return NULL;
}

// 右側補助動詞情報を取得
static const right_auxiliary_t *get_right_auxiliary(const char *particle) {
    if (particle == NULL) return NULL;
    for (int i = 0; right_auxiliary[i].particle != NULL; i++) {
        if (strcmp(particle, right_auxiliary[i].particle) == 0) {
            return &right_auxiliary[i];
        }
    }
    return NULL;
}

// 「です」の活用を取得
static const char *get_desu_conjugate(const char *particle) {
    if (particle == NULL) return NULL;
    for (int i = 0; desu_conjugate[i].particle != NULL; i++) {
        if (strcmp(particle, desu_conjugate[i].particle) == 0) {
            return desu_conjugate[i].output;
        }
    }
    return NULL;
}

// 活用形と助動詞を取得
static void get_conjugation_info(const char *left_particle, const char *right_particle,
                                 int *conj_form, char *suffix) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "%s-%s", left_particle, right_particle);

    // 例外パターンをチェック
    for (int i = 0; auxiliary_exception[i].conj_form != -1; i++) {
        if (strcmp(pattern, auxiliary_exception[i].pattern) == 0) {
            *conj_form = auxiliary_exception[i].conj_form;
            strcpy(suffix, auxiliary_exception[i].suffix);
            return;
        }
    }

    // 右側助動詞マップ
    for (int i = 0; right_auxiliary[i].particle != NULL; i++) {
        if (strcmp(right_particle, right_auxiliary[i].particle) == 0) {
            *conj_form = right_auxiliary[i].conj_form;
            strcpy(suffix, right_auxiliary[i].suffix);
            return;
        }
    }

    // デフォルト
    *conj_form = CONJ_JISHO;
    suffix[0] = '\0';
}

    // 左側補助動詞を付与する共通処理
    static void append_left_auxiliary(char *out, const left_auxiliary_info_t *left_aux, const right_auxiliary_t *right_aux) {
        if (left_aux == NULL) return;

        strcat(out, left_aux->stem);

        int aux_conj = (right_aux != NULL) ? right_aux->conj_form : CONJ_JISHO;
        if (left_aux->verb_type == 1) {
            int idx_aux = gyou_to_index(left_aux->gyou);
            strcat(out, godan_conjugate[idx_aux][aux_conj]);
        } else if (left_aux->verb_type == 2) {
            int idx_aux = gyou_to_index(left_aux->gyou);
            strcat(out, kami_conjugate[idx_aux][aux_conj]);
        } else if (left_aux->verb_type == 3) {
            int idx_aux = gyou_to_index_simo(left_aux->gyou);
            strcat(out, simo_conjugate[idx_aux][aux_conj]);
        }

        if (right_aux != NULL) {
            strcat(out, right_aux->suffix);
        }
    }

// 撥音便（五段 g/n/b/m のて・た形）
// stem_len: 語幹の長さ（バイト数）。
static void apply_godan_te_ta_voicing(char *str, size_t stem_len) {
    char *pos = str + stem_len;
    
    // 語幹直後の「んて」 → 「んで」
    if (strstr(pos, "んて") == pos) {
        memcpy(pos, "んで", 6);
    }
    // 語幹直後の「いて」 → 「いで」
    if (strstr(pos, "いて") == pos) {
        memcpy(pos, "いで", 6);
    }
    
    pos = str + stem_len;
    // 語幹直後の「んた」 → 「んだ」
    if (strstr(pos, "んた") == pos) {
        memcpy(pos, "んだ", 6);
    }
    // 語幹直後の「いた」 → 「いだ」
    if (strstr(pos, "いた") == pos) {
        memcpy(pos, "いだ", 6);
    }
}

// メイン動詞活用関数
verb_result_t mejiro_verb_conjugate(const char *left_conso, const char *left_vowel,
                                    const char *left_particle,
                                    const char *right_conso, const char *right_vowel,
                                    const char *right_particle,
                                    const char *left_kana, const char *right_kana) {
    verb_result_t result = {{0}, false};

    char stroke[64];
    snprintf(stroke, sizeof(stroke), "%s%s-%s%s", left_conso, left_vowel, right_conso, right_vowel);

    const left_auxiliary_info_t *left_aux = get_left_auxiliary(left_particle);
    const right_auxiliary_t *right_aux = get_right_auxiliary(right_particle);

    int conj_form = CONJ_JISHO;
    char suffix[64] = {0};
    if (left_aux != NULL) {
        conj_form = left_aux->conj_form;
        suffix[0] = '\0';
    } else {
        get_conjugation_info(left_particle, right_particle, &conj_form, suffix);
    }

    // 「です」処理: right_conso == 'TN' && strlen(right_vowel) == 0
    if (strcmp(right_conso, "TN") == 0 && strlen(right_vowel) == 0) {
        char base[128] = {0};
        strcat(base, left_kana);
        // 左側の助詞追加音は含める（例: TAn-TN* → たんです）
        strcat(base, get_particle_extra(left_particle));
        // 右側の助詞追加音は含めない（例: -TNk* → でしょう）

        strcpy(result.output, base);
        const char *desu_form = get_desu_conjugate(right_particle);
        if (desu_form != NULL) {
            strcat(result.output, desu_form);
            result.success = true;
            return result;
        }
    }

    const verb_entry_t *verb = NULL;
    for (size_t i = 0; i < VERB_DICT_SIZE; i++) {
        if (strcmp(stroke, verb_dict[i].stroke) == 0) {
            verb = &verb_dict[i];
            break;
        }
    }

    // 辞書にある場合
    if (verb != NULL) {
        if (verb->type == VERB_TYPE_SPECIAL) {
            if (strcmp(stroke, "I-K") == 0) {
                strcpy(result.output, iku_conjugate[conj_form]);
            } else if (strcmp(stroke, "A-") == 0) {
                strcpy(result.output, aru_conjugate[conj_form]);
            } else {
                return result;
            }
        } else {
            strcpy(result.output, verb->stem);
            if (verb->type == VERB_TYPE_GODAN) {
                int idx = gyou_to_index(verb->gyou);
                strcat(result.output, godan_conjugate[idx][conj_form]);
            } else if (verb->type == VERB_TYPE_KAMI) {
                int idx = gyou_to_index(verb->gyou);
                strcat(result.output, kami_conjugate[idx][conj_form]);
            } else if (verb->type == VERB_TYPE_SIMO) {
                int idx = gyou_to_index_simo(verb->gyou);
                strcat(result.output, simo_conjugate[idx][conj_form]);
            } else if (verb->type == VERB_TYPE_KAHEN) {
                strcat(result.output, kahen_conjugate[conj_form]);
            }
        }

        if (left_aux != NULL) {
            append_left_auxiliary(result.output, left_aux, right_aux);
        } else {
            strcat(result.output, suffix);
        }
        if (conj_form == CONJ_TE_TA) {
            if (verb != NULL && verb->type == VERB_TYPE_GODAN && (verb->gyou == 'g' || verb->gyou == 'n' || verb->gyou == 'b' || verb->gyou == 'm')) {
                apply_godan_te_ta_voicing(result.output, strlen(verb->stem));
            }
        }
        result.success = true;
        return result;
    }

    // 辞書に無い場合、入力パターンから動詞を推論
    if (strlen(right_kana) == 0) {
        if (strlen(left_kana) > 0) {
            strcpy(result.output, left_kana);
        } else {
            result.output[0] = '\0';
        }
        strcat(result.output, sahen_conjugate[conj_form]);
        char *shizu_pos = strstr(result.output, "しず");
        if (shizu_pos) {
            memcpy(shizu_pos, "せず", 6);
        }
        if (left_aux != NULL) {
            append_left_auxiliary(result.output, left_aux, right_aux);
        } else {
            strcat(result.output, suffix);
        }
        result.success = true;
        return result;
    }

    if (strlen(right_conso) > 0 && strlen(right_vowel) == 0 && strlen(right_kana) > 0) {
        char gyou = kana_to_gyou(right_kana);
        if (gyou != '\0' && (gyou == 'k' || gyou == 'g' || gyou == 's' || gyou == 't' ||
                              gyou == 'n' || gyou == 'b' || gyou == 'm' || gyou == 'r' || gyou == 'w')) {
            if (strlen(left_kana) > 0) {
                strcpy(result.output, left_kana);
            } else {
                result.output[0] = '\0';
            }
            int idx = gyou_to_index(gyou);
            strcat(result.output, godan_conjugate[idx][conj_form]);
            if (left_aux != NULL) {
                append_left_auxiliary(result.output, left_aux, right_aux);
            } else {
                strcat(result.output, suffix);
            }
            if (conj_form == CONJ_TE_TA && (gyou == 'g' || gyou == 'n' || gyou == 'b' || gyou == 'm')) {
                apply_godan_te_ta_voicing(result.output, strlen(left_kana));
            }
            result.success = true;
            return result;
        }
    }

    if (strcmp(right_vowel, "I") == 0) {
        char gyou = 'w';
        if (strlen(right_kana) > 0) {
            gyou = kana_to_gyou(right_kana);
            if (gyou == '\0') gyou = 'w';
        }
        if (gyou == 'k' || gyou == 'g' || gyou == 'z' || gyou == 't' ||
            gyou == 'n' || gyou == 'b' || gyou == 'm' || gyou == 'r' || gyou == 'w') {
            if (strlen(left_kana) > 0) {
                strcpy(result.output, left_kana);
            } else {
                result.output[0] = '\0';
            }
            int idx = gyou_to_index(gyou);
            strcat(result.output, kami_conjugate[idx][conj_form]);
            if (left_aux != NULL) {
                append_left_auxiliary(result.output, left_aux, right_aux);
            } else {
                strcat(result.output, suffix);
            }
            result.success = true;
            return result;
        }
    }

    if (strcmp(right_vowel, "IA") == 0) {
        char gyou = 'w';
        if (strlen(right_kana) > 0) {
            gyou = kana_to_gyou(right_kana);
            if (gyou == '\0') gyou = 'w';
        }
        if (gyou == 'k' || gyou == 'g' || gyou == 's' || gyou == 'z' ||
            gyou == 't' || gyou == 'd' || gyou == 'n' || gyou == 'h' ||
            gyou == 'b' || gyou == 'm' || gyou == 'r' || gyou == 'w') {
            if (strlen(left_kana) > 0) {
                strcpy(result.output, left_kana);
            } else {
                result.output[0] = '\0';
            }
            int idx = gyou_to_index_simo(gyou);
            strcat(result.output, simo_conjugate[idx][conj_form]);
            if (left_aux != NULL) {
                append_left_auxiliary(result.output, left_aux, right_aux);
            } else {
                strcat(result.output, suffix);
            }
            result.success = true;
            return result;
        }
    }

    if (strlen(right_conso) > 0 || strlen(right_vowel) > 0) {
        if (strlen(left_kana) > 0) {
            strcpy(result.output, left_kana);
            strcat(result.output, right_kana);
        } else {
            strcpy(result.output, right_kana);
        }
        int idx = gyou_to_index('r');
        strcat(result.output, godan_conjugate[idx][conj_form]);
        char *gozari_pos = strstr(result.output, "ござり");
        if (gozari_pos) {
            memcpy(gozari_pos + 6, "い", 3);
        }
        if (left_aux != NULL) {
            append_left_auxiliary(result.output, left_aux, right_aux);
        } else {
            strcat(result.output, suffix);
        }
        result.success = true;
        return result;
    }

    return result;
}

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

// ZMK does not provide QMK-style send_string(); provide a paced sender.
// Forward declarations
static void send_ascii_letters_only(const char *s);

/*
 * Output pacing:
 * Some boards/hosts will hard-fault or watchdog-reset if we try to "type"
 * long strings as a burst of press/release events in a tight loop.
 * This sender copies the string into a static buffer and emits 1 char at a time
 * from a Zephyr delayable work item.
 *
 * If a new string arrives while a previous send is in progress, we *overwrite*
 * the pending buffer (so the newest output wins). This is intentional for
 * crash isolation and keeps behavior simple.
 */
#ifndef MEJIRO_TX_INTERVAL_MS
#define MEJIRO_TX_INTERVAL_MS 2
#endif

static struct k_work_delayable g_mejiro_tx_work;
static bool g_mejiro_tx_inited = false;
static volatile bool g_mejiro_tx_busy = false;
static char g_mejiro_tx_buf[256];
static size_t g_mejiro_tx_len = 0;
static size_t g_mejiro_tx_pos = 0;

static void send_string(const char *s) {
    if (!s || s[0] == '\0') { return; }
    mejiro_queue_send_ascii(s);
}




static inline void press_key(uint32_t keycode) {
    (void)raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
}

static inline void release_key(uint32_t keycode) {
    (void)raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
}

static inline void mod_tap(uint32_t mod_keycode, uint32_t keycode) {
    press_key(mod_keycode);
    tap_key(keycode);
    release_key(mod_keycode);
}

static void send_mejiro_roma(const char *output);

/* --------------------------------------------------------------------------
 * Mejiro command table (ZMK port)
 *
 * The original Mejiro/QMK table maps "#..." strokes to navigation/edit keys.
 * Here we keep the same stroke strings and send the equivalent ZMK keycodes.
 * -------------------------------------------------------------------------- */


/* =========================
 *  Mejiro crash isolation
 *  - If pressing the Mejiro key causes the firmware to hang/reset after a few seconds,
 *    enable MEJIRO_SAFE_MODE to bypass mejiro_transform_zmk() and confirm the root cause.
 *  - When MEJIRO_SAFE_MODE=1, the stroke string is saved to g_mejiro_last_output, but no transform/output is executed.
 * ========================= */
#ifndef MEJIRO_SAFE_MODE
#define MEJIRO_SAFE_MODE 0
#endif

#ifndef MEJIRO_MAX_ID_LEN
#define MEJIRO_MAX_ID_LEN 63
#endif

static bool mejiro_id_is_sane(const char *s) {
    if (!s || s[0] == '\0') return false;
    /* Accept only a conservative charset to avoid pathological cases */
    for (size_t i = 0; s[i] != '\0'; i++) {
        const char c = s[i];
        const bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-' || c == '#' || c == '+' || c == '/' || c == '.';
        if (!ok) return false;
        if (i >= MEJIRO_MAX_ID_LEN) return false;
    }
    return true;
}

static char g_mejiro_last_output[256];
static uint16_t g_mejiro_history_len[32];
static uint8_t g_mejiro_history_count = 0;

static inline void mejiro_history_push(uint16_t n) {
    if (n == 0) { return; }
    if (g_mejiro_history_count < ARRAY_SIZE(g_mejiro_history_len)) {
        g_mejiro_history_len[g_mejiro_history_count++] = n;
    } else {
        /* drop oldest (rare) */
        memmove(&g_mejiro_history_len[0], &g_mejiro_history_len[1],
                (ARRAY_SIZE(g_mejiro_history_len) - 1) * sizeof(g_mejiro_history_len[0]));
        g_mejiro_history_len[ARRAY_SIZE(g_mejiro_history_len) - 1] = n;
    }
}
static inline uint16_t mejiro_history_pop(void) {
    if (g_mejiro_history_count == 0) { return 0; }
    return g_mejiro_history_len[--g_mejiro_history_count];
}


typedef enum {
    MJ_CMD_KEY = 0,
    MJ_CMD_MOD_KEY,
    MJ_CMD_REPEAT,
    MJ_CMD_UNDO,
} mj_cmd_kind_t;

typedef struct {
    const char *stroke;
    mj_cmd_kind_t kind;
    uint32_t keycode;
    uint32_t mod; /* only for MJ_CMD_MOD_KEY */
} mj_cmd_t;

#define MJ_KC(u) ZMK_HID_USAGE(HID_USAGE_KEY, (u))

/* Mod keycodes (explicit HID usages, avoids relying on keycode aliases) */
#define MJ_KC_LCTRL ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_LEFTCONTROL)
#define MJ_KC_LALT  ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_LEFTALT)
#define MJ_KC_LSFT  ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_LEFTSHIFT)

static const mj_cmd_t mejiro_commands_zmk[] = {

    /* QMK parity: repeat / undo */
    {"#-",  MJ_CMD_REPEAT, 0, 0},
    {"-U",  MJ_CMD_UNDO,   0, 0},

    /* QMK parity: keycode commands that include '#' */
    {"Sk#-",  MJ_CMD_MOD_KEY, ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_Z), MOD_LCTL},
    {"Kk#-",  MJ_CMD_MOD_KEY, ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_X), MOD_LCTL},
    {"Nk#-",  MJ_CMD_MOD_KEY, ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_C), MOD_LCTL},
    {"Ak#-",  MJ_CMD_MOD_KEY, ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_V), MOD_LCTL},
    {"Ank#-", MJ_CMD_MOD_KEY, ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_V), MOD_LCTL}, /* TODO: add Shift modifier too */
    /* Space */
    { "#*FLPT", MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_SPACEBAR), 0 },

    /* Enter / Backspace / Delete / Tab / Esc */
    { "#FPLT",  MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_RETURN_ENTER), 0 },
    { "#P",     MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE),  0 },
    { "#FR",    MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_DELETE_FORWARD),   0 },
    { "#FT",    MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_TAB),   0 },
    { "#RS",    MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_ESCAPE),   0 },

    /* Arrows */
    { "#R",     MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_LEFTARROW),  0 },
    { "#S",     MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_DOWNARROW),  0 },
    { "#T",     MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_UPARROW),    0 },
    { "#F",     MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_RIGHTARROW), 0 },

    /* Home / End */
    { "#H",     MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_HOME),  0 },
    { "#L",     MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_END),   0 },

    /* Word left/right: Ctrl + Left/Right */
    { "#HR",    MJ_CMD_MOD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_LEFTARROW),  MJ_KC_LCTRL },
    { "#FL",    MJ_CMD_MOD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_RIGHTARROW), MJ_KC_LCTRL },

    /* Page up/down */
    { "#HT",    MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_PAGEUP), 0 },
    { "#LS",    MJ_CMD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_PAGEDOWN), 0 },

    /* Up/down one line by paragraph? (Alt + Up/Down) */
    { "#HP",    MJ_CMD_MOD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_UPARROW),   MJ_KC_LALT },
    { "#LP",    MJ_CMD_MOD_KEY, MJ_KC(HID_USAGE_KEY_KEYBOARD_DOWNARROW), MJ_KC_LALT },

    /* Undo / Repeat */
    { "#D",     MJ_CMD_UNDO,   0, 0 },
    { "#G",     MJ_CMD_REPEAT, 0, 0 },
};

static bool handle_mejiro_command(const char *stroke) {
    if (!stroke || stroke[0] == '\0') {
        return false;
    }

    /* 1) Exact match against the command table (this includes patterns like "Sk#-" etc if present). */
    for (size_t i = 0; i < ARRAY_SIZE(mejiro_commands_zmk); i++) {
        if (strcmp(mejiro_commands_zmk[i].stroke, stroke) != 0) {
            continue;
        }

        switch (mejiro_commands_zmk[i].kind) {
        case MJ_CMD_REPEAT: {
            if (g_mejiro_last_output[0] == '\0') {
                return true;
            }
            send_string(g_mejiro_last_output);
            mejiro_history_push((uint16_t)strlen(g_mejiro_last_output));
            return true;
        }
        case MJ_CMD_UNDO: {
            uint16_t n = mejiro_history_pop();
            for (uint16_t k = 0; k < n; k++) {
                tap_key(ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE));
            }
            return true;
        }
        case MJ_CMD_KEY: {
            tap_key(mejiro_commands_zmk[i].keycode);
            return true;
        }
        case MJ_CMD_MOD_KEY: {
            /* press modifier, tap key, release modifier */
            raise_zmk_keycode_state_changed_from_encoded(mejiro_commands_zmk[i].mod, true, timestamp);
            tap_key(mejiro_commands_zmk[i].keycode);
            raise_zmk_keycode_state_changed_from_encoded(mejiro_commands_zmk[i].mod, false, timestamp);
            return true;
        }
        default:
            return true;
        }
    }

    return false;
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
        const char c = *p;

        
        uint32_t kc = keycode_from_ascii_letter(c);
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
    if (chord & B_Q) APP('Q');   // Q左 (flag)

    APP('X');

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
    if (chord & B_P) APP('P');     // P右 (flag)

    out[n] = 0;
    #undef APP
}


static const uint32_t ng_key[] = {
    [A - A] = B_A,     [B - A] = B_B,         [C - A] = B_C,         [D - A] = B_D,
    [E - A] = B_E,     [F - A] = B_F,         [G - A] = B_G,         [H - A] = B_H,
    [I - A] = B_I,     [J - A] = B_J,         [K - A] = B_K,         [L - A] = B_L,
    [M - A] = B_M,     [N - A] = B_N,         [O - A] = B_O,         [P - A] = B_P,
    [Q - A] = B_Q,     [R - A] = B_R,         [S - A] = B_S,         [T - A] = B_T,
    [U - A] = B_U,     [V - A] = B_V,         [W - A] = B_W,         [X - A] = B_X,
    [Y - A] = B_Y,     [Z - A] = B_Z,         [SEMI - A] = B_SEMI,   [COMMA - A] = B_COMMA, //[SQT - A] = B_SQT,
    //[COMMA - A] = B_COMMA, [DOT - A] = B_DOT, [SLASH - A] = B_SLASH, [SPACE - A] = B_SPACE,
    //[ENTER - A] = B_SPACE,
    [DOT - A] = B_DOT, [SLASH - A] = B_SLASH, [SPACE - A] = B_SPACE, [ENTER - A] = B_SPACE,
    [SQT - A] = B_SQT,
};

// カナ変換テーブル
typedef struct {
    uint32_t shift;
    uint32_t douji;
    uint32_t kana[6];
    void (*func)(void);
} naginata_kanamap;

static naginata_kanamap ngdickana[] = {
    // 清音/単打
    {.shift = NONE    , .douji = B_J            , .kana = {U, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // う
    {.shift = NONE    , .douji = B_K            , .kana = {I, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // い
    {.shift = NONE    , .douji = B_L            , .kana = {S, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // し
    //{.shift = B_SPACE , .douji = B_O            , .kana = {E, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_N            , .kana = {O, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_F            , .kana = {N, N, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ん
    {.shift = NONE    , .douji = B_W            , .kana = {N, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // に
    {.shift = NONE    , .douji = B_H            , .kana = {K, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // く
    {.shift = NONE    , .douji = B_S            , .kana = {T, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // と
    {.shift = NONE    , .douji = B_V            , .kana = {R, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // る
    //{.shift = B_SPACE , .douji = B_U            , .kana = {S, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_R            , .kana = {COMMA, SPACE, NONE, NONE, NONE, NONE}, .func = nofunc }, // 、変換
    {.shift = NONE    , .douji = B_O            , .kana = {G, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // が
    //{.shift = B_SPACE , .douji = B_A            , .kana = {S, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_B            , .kana = {T, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // つ
    {.shift = NONE    , .douji = B_N            , .kana = {T, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // て
    //{.shift = B_SPACE , .douji = B_G            , .kana = {T, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_L            , .kana = {T, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_E            , .kana = {H, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // は
    {.shift = NONE    , .douji = B_D            , .kana = {K, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // か
    {.shift = NONE    , .douji = B_M            , .kana = {T, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // た
    //{.shift = B_SPACE , .douji = B_D            , .kana = {N, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_W            , .kana = {N, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_COMMA        , .kana = {N, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_J            , .kana = {N, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_C            , .kana = {K, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // き
    {.shift = NONE    , .douji = B_X            , .kana = {M, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ま
    //{.shift = B_SPACE , .douji = B_X            , .kana = {H, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_SEMI         , .kana = {H, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_P            , .kana = {H, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ひ
    {.shift = NONE    , .douji = B_Z            , .kana = {S, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // す
    //{.shift = B_SPACE , .douji = B_Z            , .kana = {H, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_F            , .kana = {M, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_S            , .kana = {M, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_B            , .kana = {M, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_R            , .kana = {M, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_K            , .kana = {M, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_H            , .kana = {Y, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_P            , .kana = {Y, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_I            , .kana = {Y, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_DOT          , .kana = {DOT, SPACE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 。変換
    //{.shift = B_SPACE , .douji = B_E            , .kana = {R, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_I            , .kana = {K, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // こ
    {.shift = NONE    , .douji = B_SLASH        , .kana = {B, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぶ
    //{.shift = B_SPACE , .douji = B_SLASH        , .kana = {R, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_A            , .kana = {N, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // の
    //{.shift = B_SPACE , .douji = B_DOT          , .kana = {W, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_C            , .kana = {W, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_COMMA        , .kana = {D, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // で
    {.shift = NONE    , .douji = B_SEMI         , .kana = {N, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // な
    {.shift = NONE    , .douji = B_Q         , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ー
    {.shift = NONE    , .douji = B_T         , .kana = {T, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ち
    {.shift = NONE    , .douji = B_G         , .kana = {X, T, U, NONE, NONE, NONE   }, .func = nofunc }, // っ
    {.shift = NONE    , .douji = B_Y         , .kana = {G, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぐ
    {.shift = NONE    , .douji = B_U         , .kana = {B, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ば
    // げ追加げはシフトに移行。NG SQTでもいけるようになりましたのでお好みで。
    {.shift = NONE    , .douji = B_SQT         , .kana = {G, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // げ

    // 濁音
    //{.shift = NONE    , .douji = B_J|B_F        , .kana = {G, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // が
    //{.shift = NONE    , .douji = B_J|B_W        , .kana = {G, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぎ
    //{.shift = 0UL     , .douji = B_F|B_H        , .kana = {G, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぐ
    //{.shift = NONE    , .douji = B_J|B_S        , .kana = {G, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // げ
    //{.shift = NONE    , .douji = B_J|B_V        , .kana = {G, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ご
    //{.shift = 0UL     , .douji = B_F|B_U        , .kana = {Z, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ざ
    //{.shift = NONE    , .douji = B_J|B_R        , .kana = {Z, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じ
    //{.shift = 0UL     , .douji = B_F|B_O        , .kana = {Z, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ず
    //{.shift = NONE    , .douji = B_J|B_A        , .kana = {Z, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぜ
    //{.shift = NONE    , .douji = B_J|B_B        , .kana = {Z, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぞ
    //{.shift = 0UL     , .douji = B_F|B_N        , .kana = {D, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // だ
    //{.shift = NONE    , .douji = B_J|B_G        , .kana = {D, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぢ
    //{.shift = 0UL     , .douji = B_F|B_L        , .kana = {D, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // づ
    //{.shift = NONE    , .douji = B_J|B_E        , .kana = {D, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // で
    //{.shift = NONE    , .douji = B_J|B_D        , .kana = {D, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ど
    //{.shift = NONE    , .douji = B_J|B_C        , .kana = {B, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ば
    //{.shift = NONE    , .douji = B_J|B_X        , .kana = {B, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // び
    //{.shift = 0UL     , .douji = B_F|B_SEMI     , .kana = {B, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぶ
    //{.shift = 0UL     , .douji = B_F|B_P        , .kana = {B, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // べ
    //{.shift = NONE    , .douji = B_J|B_Z        , .kana = {B, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぼ
    //{.shift = 0UL     , .douji = B_F|B_L|B_SEMI , .kana = {V, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔ
    
    //中指シフト
    {.shift = NONE    , .douji = B_K|B_Q        , .kana = {F, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぁ
    {.shift = NONE    , .douji = B_K|B_W        , .kana = {G, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ご
    {.shift = NONE     , .douji = B_K|B_E        , .kana = {H, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふ
    {.shift = NONE    , .douji = B_K|B_R        , .kana = {F, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぃ
    {.shift = NONE    , .douji = B_K|B_T        , .kana = {F, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぇ
    {.shift = NONE     , .douji = B_D|B_Y        , .kana = {W, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぃ
    {.shift = NONE    , .douji = B_D|B_U        , .kana = {P, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぱ
    {.shift = NONE     , .douji = B_D|B_I        , .kana = {Y, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // よ
    {.shift = NONE    , .douji = B_D|B_O        , .kana = {M, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // み
    {.shift = NONE    , .douji = B_D|B_P        , .kana = {W, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぇ
    {.shift = NONE    , .douji = B_D|B_SQT     , .kana = {W, H, O, NONE, NONE, NONE      }, .func = nofunc }, // うぉ
    {.shift = NONE    , .douji = B_K|B_A        , .kana = {H, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ほ
    {.shift = NONE     , .douji = B_K|B_S        , .kana = {J, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じ
    {.shift = NONE    , .douji = B_K|B_D        , .kana = {R, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // れ
    {.shift = NONE    , .douji = B_K|B_F        , .kana = {M, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // も
    {.shift = NONE    , .douji = B_K|B_G        , .kana = {Y, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゆ
    {.shift = NONE    , .douji = B_D|B_H        , .kana = {H, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // へ
    {.shift = NONE     , .douji = B_D|B_J     , .kana = {A, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // あ
    //{.shift = NONE     , .douji = B_D|B_K     , .kana = {H, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // れ
    //{.shift = NONE     , .douji = B_D|B_L     , .kana = {O, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // お
    {.shift = NONE     , .douji = B_D|B_SEMI        , .kana = {E, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // え
    {.shift = NONE     , .douji = B_K|B_Z        , .kana = {D, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // づ
    {.shift = NONE    , .douji = B_K|B_X        , .kana = {Z, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぞ
    {.shift = NONE    , .douji = B_K|B_C        , .kana = {B, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぼ
    {.shift = NONE    , .douji = B_K|B_V        , .kana = {M, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // む
    {.shift = NONE    , .douji = B_K|B_B        , .kana = {F, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぉ
    {.shift = NONE    , .douji = B_D|B_N        , .kana = {S, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // せ
    {.shift = NONE    , .douji = B_D|B_M        , .kana = {N, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ね
    {.shift = NONE    , .douji = B_D|B_COMMA        , .kana = {B, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // べ
    {.shift = NONE    , .douji = B_D|B_DOT        , .kana = {P, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぷ
    {.shift = NONE    , .douji = B_D|B_SLASH        , .kana = {V, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔ


    // 半濁音
    //{.shift = NONE    , .douji = B_M|B_C        , .kana = {P, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぱ
    //{.shift = NONE    , .douji = B_M|B_X        , .kana = {P, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぴ
    //{.shift = NONE    , .douji = B_V|B_SEMI     , .kana = {P, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぷ
    //{.shift = NONE    , .douji = B_V|B_P        , .kana = {P, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぺ
    //{.shift = NONE    , .douji = B_M|B_Z        , .kana = {P, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぽ

    // 薬指シフト
    {.shift = NONE    , .douji = B_L|B_Q        , .kana = {D, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぢ
    {.shift = NONE    , .douji = B_L|B_W        , .kana = {M, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // め
    {.shift = NONE    , .douji = B_L|B_E        , .kana = {K, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // け
    {.shift = NONE    , .douji = B_L|B_R        , .kana = {T, H, I, NONE, NONE, NONE   }, .func = nofunc }, // てぃ
    {.shift = NONE    , .douji = B_L|B_T        , .kana = {D, H, I, NONE, NONE, NONE   }, .func = nofunc }, // でぃ
    {.shift = NONE    , .douji = B_S|B_Y        , .kana = {S, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // しぇ
    {.shift = NONE    , .douji = B_S|B_U        , .kana = {P, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぺ
    {.shift = NONE    , .douji = B_S|B_I        , .kana = {D, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ど
    {.shift = NONE    , .douji = B_S|B_O        , .kana = {Y, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // や
    {.shift = NONE    , .douji = B_S|B_P        , .kana = {J, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じぇ
    //{.shift = NONE    , .douji = B_S|B_X1        , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぱ
    {.shift = NONE    , .douji = B_L|B_A        , .kana = {W, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // を
    {.shift = NONE    , .douji = B_L|B_S        , .kana = {S, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // さ
    {.shift = NONE    , .douji = B_L|B_D        , .kana = {O, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // お
    {.shift = NONE    , .douji = B_L|B_F        , .kana = {R, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // り
    {.shift = NONE    , .douji = B_L|B_G        , .kana = {Z, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ず
    {.shift = NONE    , .douji = B_S|B_H        , .kana = {B, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // び
    {.shift = NONE    , .douji = B_S|B_J        , .kana = {R, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ら
    //{.shift = NONE    , .douji = B_S|B_K        , .kana = {J, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じ
    //{.shift = NONE    , .douji = B_S|B_L        , .kana = {S, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // さ
    {.shift = NONE    , .douji = B_S|B_SEMI        , .kana = {S, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // そ
    {.shift = NONE    , .douji = B_L|B_Z        , .kana = {Z, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぜ
    {.shift = NONE    , .douji = B_L|B_X        , .kana = {Z, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ざ
    {.shift = NONE    , .douji = B_L|B_C        , .kana = {G, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぎ
    {.shift = NONE    , .douji = B_L|B_V        , .kana = {R, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ろ
    {.shift = NONE    , .douji = B_L|B_B        , .kana = {N, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぬ
    {.shift = NONE    , .douji = B_S|B_N        , .kana = {W, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // わ
    {.shift = NONE    , .douji = B_S|B_M        , .kana = {D, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // だ
    {.shift = NONE    , .douji = B_S|B_COMMA        , .kana = {P, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぴ
    {.shift = NONE    , .douji = B_S|B_DOT        , .kana = {P, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぽ
    {.shift = NONE    , .douji = B_S|B_SLASH        , .kana = {T, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // ちぇ


    // Iシフト
    {.shift = NONE    , .douji = B_I|B_Q        , .kana = {H, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ひゅ
    {.shift = NONE    , .douji = B_I|B_W        , .kana = {S, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // しゅ
    {.shift = NONE    , .douji = B_I|B_E        , .kana = {S, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // しょ
    {.shift = NONE    , .douji = B_I|B_R        , .kana = {K, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // きゅ
    {.shift = NONE    , .douji = B_I|B_T        , .kana = {T, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ちゅ
    {.shift = NONE    , .douji = B_I|B_A        , .kana = {H, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ひょ
    {.shift = NONE    , .douji = B_I|B_F        , .kana = {K, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // きょ
    {.shift = NONE    , .douji = B_I|B_G        , .kana = {T, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ちょ

    {.shift = NONE    , .douji = B_I|B_Z        , .kana = {H, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ひゃ

    {.shift = NONE    , .douji = B_I|B_C        , .kana = {S, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // しゃ
    {.shift = NONE    , .douji = B_I|B_V        , .kana = {K, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // きゃ
    {.shift = NONE    , .douji = B_I|B_B        , .kana = {T, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ちゃ

    {.shift = NONE    , .douji = B_I|B_X        , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ー追加
    // Oシフト
    {.shift = NONE    , .douji = B_O|B_Q        , .kana = {R, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // りゅ
    {.shift = NONE    , .douji = B_O|B_W        , .kana = {J, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // じゅ
    {.shift = NONE    , .douji = B_O|B_E        , .kana = {J, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // じょ
    {.shift = NONE    , .douji = B_O|B_R        , .kana = {G, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ぎゅ
    {.shift = NONE    , .douji = B_O|B_T        , .kana = {N, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // にゅ
    {.shift = NONE    , .douji = B_O|B_A        , .kana = {R, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // りょ
    {.shift = NONE    , .douji = B_O|B_F        , .kana = {G, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ぎょ
    {.shift = NONE    , .douji = B_O|B_G        , .kana = {N, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // にょ

    {.shift = NONE    , .douji = B_O|B_Z        , .kana = {R, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // りゃ

    {.shift = NONE    , .douji = B_O|B_C        , .kana = {Z, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // じゃ
    {.shift = NONE    , .douji = B_O|B_V        , .kana = {G, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ぎゃ
    {.shift = NONE    , .douji = B_O|B_B        , .kana = {N, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // にゃ

    {.shift = NONE    , .douji = B_O|B_X        , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ー追加
    // IO3キー同時押しシフト
    {.shift = NONE    , .douji = B_I|B_O|B_Q        , .kana = {P, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ぴゅ
    {.shift = NONE    , .douji = B_I|B_O|B_W        , .kana = {M, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // みゅ
    {.shift = NONE    , .douji = B_I|B_O|B_E        , .kana = {M, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // みょ
    {.shift = NONE    , .douji = B_I|B_O|B_R        , .kana = {B, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // びゅ
    {.shift = NONE    , .douji = B_I|B_O|B_T        , .kana = {D, H, U, NONE, NONE, NONE   }, .func = nofunc }, // でゅ
    {.shift = NONE    , .douji = B_I|B_O|B_A        , .kana = {P, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ぴょ
    {.shift = NONE    , .douji = B_I|B_O|B_F        , .kana = {B, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // びょ
    {.shift = NONE    , .douji = B_I|B_O|B_G        , .kana = {V, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぁ

    {.shift = NONE    , .douji = B_I|B_O|B_Z        , .kana = {P, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ぴゃ

    {.shift = NONE    , .douji = B_I|B_O|B_C        , .kana = {M, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // みゃ
    {.shift = NONE    , .douji = B_I|B_O|B_V        , .kana = {B, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // びゃ
    {.shift = NONE    , .douji = B_I|B_O|B_B        , .kana = {V, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぃ


    {.shift = NONE    , .douji = B_I|B_O|B_X        , .kana = {LS(INT1), NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ＿追加
    // SQTシフト B_SQT 
    {.shift = NONE    , .douji = B_SQT|B_Q        , .kana = {S, U, X, I, NONE, NONE   }, .func = nofunc }, // スィ
    {.shift = NONE    , .douji = B_SQT|B_W        , .kana = {H, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // ひぇ
    {.shift = NONE    , .douji = B_SQT|B_E        , .kana = {G, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // ぎぇ
    {.shift = NONE    , .douji = B_SQT|B_R        , .kana = {P, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // ぴぇ
    {.shift = NONE    , .douji = B_SQT|B_T        , .kana = {T, H, A, NONE, NONE, NONE   }, .func = nofunc }, // てゃ
    {.shift = NONE    , .douji = B_SQT|B_A        , .kana = {G, W, A, NONE, NONE, NONE   }, .func = nofunc }, // グァ
    {.shift = NONE    , .douji = B_SQT|B_F        , .kana = {BSPC, BSPC, BSPC, NONE, NONE, NONE   }, .func = nofunc }, // BS3
    {.shift = NONE    , .douji = B_SQT|B_G        , .kana = {G, W, O, NONE, NONE, NONE   }, .func = nofunc }, // ぐぉ

    {.shift = NONE    , .douji = B_SQT|B_Z        , .kana = {G, U, X, W, A, NONE   }, .func = nofunc }, // ぐゎ

    {.shift = NONE    , .douji = B_SQT|B_X        , .kana = {G, E, X, E, NONE, NONE   }, .func = nofunc }, // げぇ
    {.shift = NONE    , .douji = B_SQT|B_C        , .kana = {G, W, E, NONE, NONE, NONE   }, .func = nofunc }, // ぐぇ
    {.shift = NONE    , .douji = B_SQT|B_V        , .kana = {T, H, E, NONE, NONE, NONE   }, .func = nofunc }, // てぇ
    {.shift = NONE    , .douji = B_SQT|B_B        , .kana = {D, H, E, NONE, NONE, NONE   }, .func = nofunc }, // でぇ

    {.shift = NONE    , .douji = B_SQT|B_S        , .kana = {K, W, A, NONE, NONE, NONE   }, .func = nofunc }, // クァ

    // 小書き
    //{.shift = NONE    , .douji = B_Q|B_H        , .kana = {X, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ゃ
    //{.shift = NONE    , .douji = B_Q|B_P        , .kana = {X, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ゅ
    //{.shift = NONE    , .douji = B_Q|B_I        , .kana = {X, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ょ
    //{.shift = NONE    , .douji = B_Q|B_J        , .kana = {X, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぁ
    //{.shift = NONE    , .douji = B_Q|B_K        , .kana = {X, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぃ
    //{.shift = NONE    , .douji = B_Q|B_L        , .kana = {X, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぅ
    //{.shift = NONE    , .douji = B_Q|B_O        , .kana = {X, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぇ
    //{.shift = NONE    , .douji = B_Q|B_N        , .kana = {X, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぉ
    //{.shift = NONE    , .douji = B_Q|B_DOT      , .kana = {X, W, A, NONE, NONE, NONE      }, .func = nofunc }, // ゎ
    //{.shift = NONE    , .douji = B_G            , .kana = {X, T, U, NONE, NONE, NONE      }, .func = nofunc }, // っ
    //{.shift = NONE    , .douji = B_Q|B_S        , .kana = {X, K, E, NONE, NONE, NONE      }, .func = nofunc }, // ヶ入れなくていいよね
    //{.shift = NONE    , .douji = B_Q|B_F        , .kana = {X, K, A, NONE, NONE, NONE      }, .func = nofunc }, // ヵ入れなくていいよね

    // 清音拗音 濁音拗音 半濁拗音
    //{.shift = NONE    , .douji = B_R|B_H        , .kana = {S, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // しゃ
    //{.shift = NONE    , .douji = B_R|B_P        , .kana = {S, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // しゅ
    //{.shift = NONE    , .douji = B_R|B_I        , .kana = {S, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // しょ
    //{.shift = NONE    , .douji = B_J|B_R|B_H    , .kana = {Z, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // じゃ
    //{.shift = NONE    , .douji = B_J|B_R|B_P    , .kana = {Z, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // じゅ
    //{.shift = NONE    , .douji = B_J|B_R|B_I    , .kana = {Z, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // じょ
    //{.shift = NONE    , .douji = B_W|B_H        , .kana = {K, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // きゃ
    //{.shift = NONE    , .douji = B_W|B_P        , .kana = {K, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // きゅ
    //{.shift = NONE    , .douji = B_W|B_I        , .kana = {K, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // きょ
    //{.shift = NONE    , .douji = B_J|B_W|B_H    , .kana = {G, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぎゃ
    //{.shift = NONE    , .douji = B_J|B_W|B_P    , .kana = {G, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぎゅ
    //{.shift = NONE    , .douji = B_J|B_W|B_I    , .kana = {G, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぎょ
    //{.shift = NONE    , .douji = B_G|B_H        , .kana = {T, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ちゃ
    //{.shift = NONE    , .douji = B_G|B_P        , .kana = {T, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ちゅ
    //{.shift = NONE    , .douji = B_G|B_I        , .kana = {T, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ちょ
    //{.shift = NONE    , .douji = B_J|B_G|B_H    , .kana = {D, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぢゃいるこれ？
    //{.shift = NONE    , .douji = B_J|B_G|B_P    , .kana = {D, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぢゅいるこれ？
    //{.shift = NONE    , .douji = B_J|B_G|B_I    , .kana = {D, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぢょいるこれ？
    //{.shift = NONE    , .douji = B_D|B_H        , .kana = {N, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // にゃ
    //{.shift = NONE    , .douji = B_D|B_P        , .kana = {N, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // にゅ
    //{.shift = NONE    , .douji = B_D|B_I        , .kana = {N, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // にょ
    //{.shift = NONE    , .douji = B_X|B_H        , .kana = {H, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ひゃ
    //{.shift = NONE    , .douji = B_X|B_P        , .kana = {H, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ひゅ
    //{.shift = NONE    , .douji = B_X|B_I        , .kana = {H, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ひょ
    //{.shift = NONE    , .douji = B_J|B_X|B_H    , .kana = {B, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // びゃ
    //{.shift = NONE    , .douji = B_J|B_X|B_P    , .kana = {B, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // びゅ
    //{.shift = NONE    , .douji = B_J|B_X|B_I    , .kana = {B, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // びょ
    //{.shift = NONE    , .douji = B_M|B_X|B_H    , .kana = {P, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぴゃ
    //{.shift = NONE    , .douji = B_M|B_X|B_P    , .kana = {P, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぴゅ
    //{.shift = NONE    , .douji = B_M|B_X|B_I    , .kana = {P, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぴょ
    //{.shift = NONE    , .douji = B_S|B_H        , .kana = {M, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // みゃ
    //{.shift = NONE    , .douji = B_S|B_P        , .kana = {M, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // みゅ
    //{.shift = NONE    , .douji = B_S|B_I        , .kana = {M, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // みょ
    //{.shift = NONE    , .douji = B_E|B_H        , .kana = {R, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // りゃ
    //{.shift = NONE    , .douji = B_E|B_P        , .kana = {R, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // りゅ
    //{.shift = NONE    , .douji = B_E|B_I        , .kana = {R, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // りょ

    // 清音外来音 濁音外来音
    //{.shift = NONE    , .douji = B_M|B_E|B_K    , .kana = {T, H, I, NONE, NONE, NONE      }, .func = nofunc }, // てぃ
    //{.shift = NONE    , .douji = B_M|B_E|B_P    , .kana = {T, E, X, Y, U, NONE            }, .func = nofunc }, // てゅ
    //{.shift = NONE    , .douji = B_J|B_E|B_K    , .kana = {D, H, I, NONE, NONE, NONE      }, .func = nofunc }, // でぃ
    //{.shift = NONE    , .douji = B_J|B_E|B_P    , .kana = {D, H, U, NONE, NONE, NONE      }, .func = nofunc }, // でゅ
    //{.shift = NONE    , .douji = B_M|B_D|B_L    , .kana = {T, O, X, U, NONE, NONE         }, .func = nofunc }, // とぅ
    //{.shift = NONE    , .douji = B_J|B_D|B_L    , .kana = {D, O, X, U, NONE, NONE         }, .func = nofunc }, // どぅ
    //{.shift = NONE    , .douji = B_M|B_R|B_O    , .kana = {S, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // しぇ
    //{.shift = NONE    , .douji = B_M|B_G|B_O    , .kana = {T, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // ちぇ
    //{.shift = NONE    , .douji = B_J|B_R|B_O    , .kana = {Z, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // じぇ
    //{.shift = NONE    , .douji = B_J|B_G|B_O    , .kana = {D, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // ぢぇnoneed
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_J , .kana = {F, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぁ
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_K , .kana = {F, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぃ
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_O , .kana = {F, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぇ
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_N , .kana = {F, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぉ
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_P , .kana = {F, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ふゅ一応あるから
    //{.shift = NONE    , .douji = B_V|B_K|B_O    , .kana = {I, X, E, NONE, NONE, NONE      }, .func = nofunc }, // いぇはない
    //{.shift = NONE    , .douji = B_V|B_L|B_K    , .kana = {W, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぃ
    //{.shift = NONE    , .douji = B_V|B_L|B_O    , .kana = {W, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぇ
    //{.shift = NONE    , .douji = B_V|B_L|B_N    , .kana = {U, X, O, NONE, NONE, NONE      }, .func = nofunc }, // うぉ
    //{.shift = NONE    , .douji = B_F|B_L|B_J    , .kana = {V, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぁ
    //{.shift = NONE    , .douji = B_F|B_L|B_K    , .kana = {V, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぃ
    //{.shift = NONE    , .douji = B_F|B_L|B_O    , .kana = {V, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぇはないnoneed
    //{.shift = NONE    , .douji = B_F|B_L|B_N    , .kana = {V, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぉはないnoneed
    //{.shift = NONE    , .douji = B_F|B_L|B_P    , .kana = {V, U, X, Y, U, NONE            }, .func = nofunc }, // ゔゅはない
    //{.shift = NONE    , .douji = B_V|B_H|B_J    , .kana = {K, U, X, A, NONE, NONE         }, .func = nofunc }, // くぁはないnoneed
    //{.shift = NONE    , .douji = B_V|B_H|B_K    , .kana = {K, U, X, I, NONE, NONE         }, .func = nofunc }, // くぃはない
    //{.shift = NONE    , .douji = B_V|B_H|B_O    , .kana = {K, U, X, E, NONE, NONE         }, .func = nofunc }, // くぇはない
    //{.shift = NONE    , .douji = B_V|B_H|B_N    , .kana = {K, U, X, O, NONE, NONE         }, .func = nofunc }, // くぉはない
    //{.shift = NONE    , .douji = B_V|B_H|B_DOT  , .kana = {K, U, X, W, A, NONE            }, .func = nofunc }, // くゎはない
    //{.shift = NONE    , .douji = B_F|B_H|B_J    , .kana = {G, U, X, A, NONE, NONE         }, .func = nofunc }, // ぐぁはないいらないんじゃ
    //{.shift = NONE    , .douji = B_F|B_H|B_K    , .kana = {G, U, X, I, NONE, NONE         }, .func = nofunc }, // ぐぃいらない
    //{.shift = NONE    , .douji = B_F|B_H|B_O    , .kana = {G, U, X, E, NONE, NONE         }, .func = nofunc }, // ぐぇはないいらないんじゃ
    //{.shift = NONE    , .douji = B_F|B_H|B_N    , .kana = {G, U, X, O, NONE, NONE         }, .func = nofunc }, // ぐぉいらない
    //{.shift = NONE    , .douji = B_F|B_H|B_DOT  , .kana = {G, U, X, W, A, NONE            }, .func = nofunc }, // ぐゎはないnoneed
    //{.shift = NONE    , .douji = B_V|B_L|B_J    , .kana = {T, S, A, NONE, NONE, NONE      }, .func = nofunc }, // つぁはない

    // げうぉ外来音とネットスラングを親指のNGスペースにて
    {.shift = B_SPACE , .douji = B_Q            , .kana = {X, W, A, NONE, NONE, NONE   }, .func = nofunc }, // ゎ
    {.shift = B_SPACE , .douji = B_W            , .kana = {X, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ゅ
    {.shift = B_SPACE , .douji = B_E            , .kana = {X, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ょ
    {.shift = B_SPACE , .douji = B_R            , .kana = {V, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ヴュ
    {.shift = B_SPACE , .douji = B_T            , .kana = {T, H, U, NONE, NONE, NONE   }, .func = nofunc }, // てゅ
    {.shift = B_SPACE , .douji = B_Y            , .kana = {T, S, E, NONE, NONE, NONE   }, .func = nofunc }, // ツェ
    {.shift = B_SPACE , .douji = B_U            , .kana = {V, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ヴェ
    {.shift = B_SPACE , .douji = B_I            , .kana = {Q, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // くぇ
    {.shift = B_SPACE , .douji = B_O            , .kana = {X, K, E, NONE, NONE, NONE   }, .func = nofunc }, // ヶ
    {.shift = B_SPACE , .douji = B_P            , .kana = {X, K, A, NONE, NONE, NONE   }, .func = nofunc }, // ヵ
    {.shift = B_SPACE , .douji = B_A            , .kana = {X, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぁ
    {.shift = B_SPACE , .douji = B_S           , .kana = {X, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぃ
    {.shift = B_SPACE , .douji = B_D           , .kana = {X, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぅ
    {.shift = B_SPACE , .douji = B_F           , .kana = {X, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぇ
    {.shift = B_SPACE , .douji = B_G           , .kana = {X, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぉ
    {.shift = B_SPACE , .douji = B_H           , .kana = {T, S, O, NONE, NONE, NONE   }, .func = nofunc }, // つぉ
    {.shift = B_SPACE , .douji = B_J           , .kana = {W, H, O, NONE, NONE, NONE   }, .func = nofunc }, // うぉ
    {.shift = B_SPACE , .douji = B_K           , .kana = {Q, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // くぉ
    {.shift = B_SPACE , .douji = B_L           , .kana = {G, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // げ
    {.shift = B_SPACE , .douji = B_SEMI           , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ー
    {.shift = B_SPACE , .douji = B_SQT          , .kana = {G, E, MINUS, NONE, NONE, NONE   }, .func = nofunc }, // げー
    {.shift = B_SPACE , .douji = B_Z           , .kana = {K, U, X, W, A, NONE   }, .func = nofunc }, // くゎ
    {.shift = B_SPACE , .douji = B_X           , .kana = {Y, E, MINUS, NONE, NONE, NONE   }, .func = nofunc }, // いぇー
    {.shift = B_SPACE , .douji = B_C           , .kana = {X, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ゃ
    {.shift = B_SPACE , .douji = B_V           , .kana = {F, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // フュ
    {.shift = B_SPACE , .douji = B_B           , .kana = {V, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぉ
    {.shift = B_SPACE , .douji = B_N           , .kana = {T, S, I, NONE, NONE, NONE   }, .func = nofunc }, // つぃ
    {.shift = B_SPACE , .douji = B_M           , .kana = {T, S, A, NONE, NONE, NONE   }, .func = nofunc }, // つぁ
    {.shift = B_SPACE , .douji = B_COMMA           , .kana = {Q, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // くぃ
    {.shift = B_SPACE , .douji = B_DOT           , .kana = {T, W, U, NONE, NONE, NONE   }, .func = nofunc }, // とぅ
    {.shift = B_SPACE , .douji = B_SLASH           , .kana = {D, W, U, NONE, NONE, NONE   }, .func = nofunc }, // ドゥ

    // 追加
    {.shift = NONE    , .douji = B_SPACE        , .kana = {SPACE, NONE, NONE, NONE, NONE, NONE  }, .func = nofunc},
    //{.shift = B_SPACE , .douji = B_V            , .kana = {COMMA, ENTER, NONE, NONE, NONE, NONE }, .func = nofunc},
    //{.shift = NONE    , .douji = B_Q            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc},
    //{.shift = B_SPACE , .douji = B_M            , .kana = {DOT, ENTER, NONE, NONE, NONE, NONE   }, .func = nofunc},
    //{.shift = NONE    , .douji = B_U            , .kana = {BSPC, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc},

    {.shift = NONE    , .douji = B_V|B_M        , .kana = {ENTER, NONE, NONE, NONE, NONE, NONE  }, .func = nofunc}, // enter
    // {.shift = B_SPACE, .douji = B_V|B_M, .kana = {ENTER, NONE, NONE, NONE, NONE, NONE}, .func = nofunc}, // enter+シフト(連続シフト)

    //{.shift = NONE    , .douji = B_T            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_T}, //
    //{.shift = NONE    , .douji = B_Y            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_Y}, //
    //{.shift = B_SPACE , .douji = B_T            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_ST}, //
    //{.shift = B_SPACE , .douji = B_Y            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_SY}, //

{.shift = NONE    , .douji = B_H|B_J        , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = naginata_on}, // 　かなオン
    // {.shift = NONE, .douji = B_F | B_G, .kana = {NONE, NONE, NONE, NONE, NONE, NONE}, .func = naginata_off}, // 　かなオフ

    // 編集モード
    {.shift = B_J|B_K    , .douji = B_Q     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKQ    }, // ^{End}
    {.shift = B_J|B_K    , .douji = B_W     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKW    }, // ／{改行}
    {.shift = B_J|B_K    , .douji = B_E     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKE    }, // /*ディ*/
    {.shift = B_J|B_K    , .douji = B_R     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKR    }, // ^s
    {.shift = B_J|B_K    , .douji = B_T     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKT    }, // ・
    {.shift = B_J|B_K    , .douji = B_A     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKA    }, // ……{改行}
    //{.shift = B_J|B_K    , .douji = B_A     , .kana = {RBKT, BSLH, ENTER, LEFT, NONE, NONE} , .func = nofunc    }, // 「」
    {.shift = B_J|B_K    , .douji = B_S     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKS    }, // 『{改行}
    //{.shift = B_J|B_K    , .douji = B_S     , .kana = {LS(N8), LS(N9), ENTER, LEFT, NONE, NONE} , .func = nofunc    }, // （）
    {.shift = B_J|B_K    , .douji = B_D     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKD    }, // ？{改行}
    {.shift = B_J|B_K    , .douji = B_F     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKF    }, // 「{改行}
    {.shift = B_J|B_K    , .douji = B_G     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKG    }, // ({改行}
    //{.shift = B_J|B_K    , .douji = B_Z     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKZ    }, // ――{改行}
    {.shift = B_J|B_K    , .douji = B_Z     , .kana = {LS(N5), NONE, NONE, NONE, NONE, NONE} , .func = nofunc    }, // %
    //{.shift = B_J|B_K    , .douji = B_X     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKX    }, // 』{改行}
    {.shift = B_J|B_K    , .douji = B_X     , .kana = {LS(EQUAL), NONE, NONE, NONE, NONE, NONE} , .func = nofunc    }, // ～
    {.shift = B_J|B_K    , .douji = B_C     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKC    }, // ！{改行}
    {.shift = B_J|B_K    , .douji = B_V     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKV    }, // 」{改行}
    {.shift = B_J|B_K    , .douji = B_B     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKB    }, // ){改行}
    {.shift = B_D|B_F    , .douji = B_Y     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFY    }, // {Home}
    {.shift = B_D|B_F    , .douji = B_U     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFU    }, // +{End}{BS}
    {.shift = B_D|B_F    , .douji = B_I     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFI    }, // {vk1Csc079}
    //{.shift = B_D|B_F    , .douji = B_I     , .kana = {LWIN, SLASH, NONE, NONE, NONE, NONE} , .func = nofunc    }, // {vk1Csc079}
    {.shift = B_D|B_F    , .douji = B_O     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFO    }, // {Del}
    {.shift = B_D|B_F    , .douji = B_P     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFP    }, // +{Esc 2}
    {.shift = B_D|B_F    , .douji = B_H     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFH    }, // {Enter}{End}
    {.shift = B_D|B_F    , .douji = B_J     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFJ    }, // {↑}
    {.shift = B_D|B_F    , .douji = B_K     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFK    }, // +{↑}
    {.shift = B_D|B_F    , .douji = B_L     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFL    }, // +{↑ 7}
    //{.shift = B_D|B_F    , .douji = B_SEMI  , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFSCLN }, // ^i
    {.shift = B_D|B_F    , .douji = B_SEMI  , .kana = {F7, NONE, NONE, NONE, NONE, NONE} , .func = nofunc }, // ^i
    {.shift = B_D|B_F    , .douji = B_N     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFN    }, // {End}
    {.shift = B_D|B_F    , .douji = B_M     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFM    }, // {↓}
    {.shift = B_D|B_F    , .douji = B_COMMA , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFCOMM }, // +{↓}
    {.shift = B_D|B_F    , .douji = B_DOT   , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFDOT  }, // +{↓ 7}
    //{.shift = B_D|B_F    , .douji = B_SLASH , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFSLSH }, // ^u
    {.shift = B_D|B_F    , .douji = B_SLASH  , .kana = {F6, NONE, NONE, NONE, NONE, NONE} , .func = nofunc }, // ^u
    {.shift = B_M|B_COMMA, .douji = B_Q     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCQ    }, // ｜{改行}
    {.shift = B_M|B_COMMA, .douji = B_W     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCW    }, // 　　　×　　　×　　　×{改行 2}
    {.shift = B_M|B_COMMA, .douji = B_E     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCE    }, // {Home}{→}{End}{Del 2}{←}
    {.shift = B_M|B_COMMA, .douji = B_R     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCR    }, // {Home}{改行}{Space 1}{←}
    {.shift = B_M|B_COMMA, .douji = B_T     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCT    }, // 〇{改行}
    {.shift = B_M|B_COMMA, .douji = B_A     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCA    }, // 《{改行}
    {.shift = B_M|B_COMMA, .douji = B_S     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCS    }, // 【{改行}
    {.shift = B_M|B_COMMA, .douji = B_D     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCD    }, // {Home}{→}{End}{Del 4}{←}
    {.shift = B_M|B_COMMA, .douji = B_F     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCF    }, // {Home}{改行}{Space 3}{←}
    {.shift = B_M|B_COMMA, .douji = B_G     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCG    }, // {Space 3}
    {.shift = B_M|B_COMMA, .douji = B_Z     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCZ    }, // 》{改行}
    {.shift = B_M|B_COMMA, .douji = B_X     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCX    }, // 】{改行}
    {.shift = B_M|B_COMMA, .douji = B_C     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCC    }, // 」{改行}{改行}
    {.shift = B_M|B_COMMA, .douji = B_V     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCV    }, // 」{改行}{改行}「{改行}
    {.shift = B_M|B_COMMA, .douji = B_B     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCB    }, // 」{改行}{改行}{Space}
    {.shift = B_C|B_V    , .douji = B_Y     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVY    }, // +{Home}
    {.shift = B_C|B_V    , .douji = B_U     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVU    }, // ^x
    {.shift = B_C|B_V    , .douji = B_I     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVI    }, // {vk1Csc079}
    {.shift = B_C|B_V    , .douji = B_O     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVO    }, // ^v
    {.shift = B_C|B_V    , .douji = B_P     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVP    }, // ^z
    {.shift = B_C|B_V    , .douji = B_H     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVH    }, // ^c
    {.shift = B_C|B_V    , .douji = B_J     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVJ    }, // {←}
    {.shift = B_C|B_V    , .douji = B_K     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVK    }, // {→}
    {.shift = B_C|B_V    , .douji = B_L     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVL    }, // {改行}{Space}+{Home}^x{BS}
    {.shift = B_C|B_V    , .douji = B_SEMI  , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVSCLN }, // ^y
    {.shift = B_C|B_V    , .douji = B_N     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVN    }, // +{End}
    {.shift = B_C|B_V    , .douji = B_M     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVM    }, // +{←}
    {.shift = B_C|B_V    , .douji = B_COMMA , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVCOMM }, // +{→}
    {.shift = B_C|B_V    , .douji = B_DOT   , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVDOT  }, // +{← 7}
    {.shift = B_C|B_V    , .douji = B_SLASH , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVSLSH }, // +{→ 7}

};

// Helper function for counting matches/candidates
static int count_kana_entries(NGList *keys, bool exact_match) {
  if (keys->size == 0) return 0;

  int count = 0;
  uint32_t keyset0 = 0UL, keyset1 = 0UL, keyset2 = 0UL;
  
  // keysetを配列にしたらバイナリサイズが増えた
  switch (keys->size) {
    case 1:
      keyset0 = ng_key[keys->elements[0] - A];
      break;
    case 2:
      keyset0 = ng_key[keys->elements[0] - A];
      keyset1 = ng_key[keys->elements[1] - A];
      break;
    default:
      keyset0 = ng_key[keys->elements[0] - A];
      keyset1 = ng_key[keys->elements[1] - A];
      keyset2 = ng_key[keys->elements[2] - A];
      break;
  }

  for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[i]; i++) {
    bool matches = false;

    switch (keys->size) {
      case 1:
        if (exact_match) {
          matches = (ngdickana[i].shift == keyset0) || 
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == keyset0);
        } else {
          matches = ((ngdickana[i].shift & keyset0) == keyset0) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & keyset0) == keyset0);
        }
        break;
      case 2:
        if (exact_match) {
          matches = (ngdickana[i].shift == (keyset0 | keyset1)) ||
                   (ngdickana[i].shift == keyset0 && ngdickana[i].douji == keyset1) ||
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == (keyset0 | keyset1));
        } else {
          matches = (ngdickana[i].shift == (keyset0 | keyset1)) ||
                   (ngdickana[i].shift == keyset0 && (ngdickana[i].douji & keyset1) == keyset1) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & (keyset0 | keyset1)) == (keyset0 | keyset1));
          // しぇ、ちぇ、など2キーで確定してはいけない
          if (matches && (ngdickana[i].shift | ngdickana[i].douji) != (keyset0 | keyset1)) {
            count = 2;
          }
        }
        break;
      default:
        if (exact_match) {
          matches = (ngdickana[i].shift == (keyset0 | keyset1) && ngdickana[i].douji == keyset2) ||
                   (ngdickana[i].shift == keyset0 && ngdickana[i].douji == (keyset1 | keyset2)) ||
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == (keyset0 | keyset1 | keyset2));
        } else {
          matches = (ngdickana[i].shift == (keyset0 | keyset1) && (ngdickana[i].douji & keyset2) == keyset2) ||
                   (ngdickana[i].shift == keyset0 && (ngdickana[i].douji & (keyset1 | keyset2)) == (keyset1 | keyset2)) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & (keyset0 | keyset1 | keyset2)) == (keyset0 | keyset1 | keyset2));
        }
        break;
    }

    if (matches) {
      count++;
      if (count > 1) break;
    }
  }

  return count;
}

int number_of_matches(NGList *keys) {  
  int result = count_kana_entries(keys, true);
  return result;
}

int number_of_candidates(NGList *keys) {
  int result = count_kana_entries(keys, false);
  return result;
}

// キー入力を文字に変換して出力する
void ng_type(NGList *keys) {
    LOG_DBG(">NAGINATA NG_TYPE");

    if (keys->size == 0)
        return;

    if (keys->size == 1 && keys->elements[0] == ENTER) {
        LOG_DBG(" NAGINATA type keycode 0x%02X", ENTER);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
        return;
    }

    uint32_t keyset = 0UL;
    for (int i = 0; i < keys->size; i++) {
        keyset |= ng_key[keys->elements[i] - A];
    }

    for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[0]; i++) {
        if ((ngdickana[i].shift | ngdickana[i].douji) == keyset) {
            if (ngdickana[i].kana[0] != NONE) {
                for (int k = 0; k < 6; k++) {
                    if (ngdickana[i].kana[k] == NONE)
                        break;
                    LOG_DBG(" NAGINATA type keycode 0x%02X", ngdickana[i].kana[k]);
                    raise_zmk_keycode_state_changed_from_encoded(ngdickana[i].kana[k], true,
                                                                 timestamp);
                    raise_zmk_keycode_state_changed_from_encoded(ngdickana[i].kana[k], false,
                                                                 timestamp);
                }
            } else {
                ngdickana[i].func();
            }
            LOG_DBG("<NAGINATA NG_TYPE");
            return;
        }
    }

    // JIみたいにJIを含む同時押しはたくさんあるが、JIのみの同時押しがないとき
    // 最後の１キーを別に分けて変換する
    NGList a, b;
    initializeList(&a);
    initializeList(&b);
    for (int i = 0; i < keys->size - 1; i++) {
        addToList(&a, keys->elements[i]);
    }
    addToList(&b, keys->elements[keys->size - 1]);
    ng_type(&a);
    ng_type(&b);

    LOG_DBG("<NAGINATA NG_TYPE");
}

// 薙刀式の入力処理
bool naginata_press(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    LOG_DBG(">NAGINATA PRESS");

    uint32_t keycode = binding->param1;

    switch (keycode) {
    case A ... Z:
    case SPACE:
    case ENTER:
    case DOT:
    case COMMA:
    case SLASH:
    case SEMI:
    case SQT:        
        n_pressed_keys++;
        pressed_keys |= ng_key[keycode - A]; // キーの重ね合わせ
        chord_keys |= ng_key[keycode - A];   // chord union for this stroke

        if (keycode == SPACE || keycode == ENTER) {
            NGList a;
            initializeList(&a);
            addToList(&a, keycode);
            addToListArray(&nginput, &a);
        } else {
            NGList a;
            NGList b;
            if (nginput.size > 0) {
                copyList(&(nginput.elements[nginput.size - 1]), &a);
                copyList(&a, &b);
                addToList(&b, keycode);
            }

            // 前のキーとの同時押しの可能性があるなら前に足す
            // 同じキー連打を除外
            if (nginput.size > 0 && a.elements[a.size - 1] != keycode &&
                number_of_candidates(&b) > 0) {
                removeFromListArrayAt(&nginput, nginput.size - 1);
                addToListArray(&nginput, &b);
                // 前のキーと同時押しはない
            } else {
                // 連続シフトではない
                NGList e;
                initializeList(&e);
                addToList(&e, keycode);
                addToListArray(&nginput, &e);
            }
        }

        // 連続シフト
        static uint32_t rs[10][2] = {{D, F},     {C, V}, {J, K}, {M, COMMA}, {SPACE, 0},
                                     {ENTER, 0}, {F, 0}, {V, 0}, {J, 0},     {M, 0}};

        uint32_t keyset = 0UL;
        for (int i = 0; i < nginput.elements[0].size; i++) {
            keyset |= ng_key[nginput.elements[0].elements[i] - A];
        }
        for (int i = 0; i < 10; i++) {
            NGList rskc;
            initializeList(&rskc);
            addToList(&rskc, rs[i][0]);
            if (rs[i][1] > 0) {
                addToList(&rskc, rs[i][1]);
            }

            int c = includeList(&rskc, keycode);
            uint32_t brs = 0UL;
            for (int j = 0; j < rskc.size; j++) {
                brs |= ng_key[rskc.elements[j] - A];
            }

            NGList l = nginput.elements[nginput.size - 1];
            for (int j = 0; j < l.size; j++) {
                addToList(&rskc, l.elements[j]);
            }

            if (c < 0 && ((brs & pressed_keys) == brs) && (keyset & brs) != brs && number_of_matches(&rskc) > 0) {
                nginput.elements[nginput.size - 1] = rskc;
                break;
            }
        }
        // NOTE: Mejiro finalize-on-release: do NOT commit on press.
        // if (nginput.size > 1 || number_of_candidates(&(nginput.elements[0])) == 1) {
        //     ng_type(&(nginput.elements[0]));
        //     removeFromListArrayAt(&nginput, 0);
        // }
        break;
    }

    LOG_DBG("<NAGINATA PRESS");

    return true;
}

bool naginata_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    LOG_DBG(">NAGINATA RELEASE");

    uint32_t keycode = binding->param1;

    switch (keycode) {
    case A ... Z:
    case SPACE:
    case ENTER:
    case DOT:
    case COMMA:
    case SLASH:
    case SEMI:
    case SQT:
        if (n_pressed_keys > 0)
            n_pressed_keys--;
        if (n_pressed_keys == 0)
            pressed_keys = 0UL;

        pressed_keys &= ~ng_key[keycode - A]; // キーの重ね合わせ

        
if (pressed_keys == 0UL) {
    // Mejiro finalize-on-release: build stroke from the full chord and send debug letters.
    char stroke[64];
    build_mejiro_id(chord_keys, stroke, sizeof(stroke));

    // Output the stroke letters (letters-only) so we can verify mapping + timing.
    // Example: QST...X...P
    send_mejiro_output(stroke);

    // Clear buffered chord for next stroke.
    chord_keys = 0UL;

    // Clear pending Naginata candidate buffer (we keep its construction intact for now,
    // but we do not use it for output in this Mejiro-movement mode).
    while (nginput.size > 0) {
        removeFromListArrayAt(&nginput, 0);
    }
} else {
    // NOTE: Mejiro finalize-on-release: do NOT commit while still holding keys.
    // if (nginput.size > 0 && number_of_candidates(&(nginput.elements[0])) == 1) {
    //     ng_type(&(nginput.elements[0]));
    //     removeFromListArrayAt(&nginput, 0);
    // }
}break;
    }

    LOG_DBG("<NAGINATA RELEASE");

    return true;
}

// 薙刀式

static int behavior_naginata_init(const struct device *dev) {
    LOG_DBG("NAGINATA INIT");

    initializeListArray(&nginput);
    pressed_keys = 0UL;
    chord_keys = 0UL;
    n_pressed_keys = 0;
    naginata_config.os =  NG_MACOS;

    return 0;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    if (g_mejiro_safe_mode == 3) {
        return 0;
    }

    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    // F15が押されたらnaginata_config.os=NG_WINDOWS
    switch (binding->param1) {
        case F15:
            naginata_config.os = NG_WINDOWS;
            return ZMK_BEHAVIOR_OPAQUE;
        case F16:
            naginata_config.os = NG_MACOS;
            return ZMK_BEHAVIOR_OPAQUE;
        case F17:
            naginata_config.os = NG_LINUX;
            return ZMK_BEHAVIOR_OPAQUE;
        case F18:
            naginata_config.tategaki = true;
            return ZMK_BEHAVIOR_OPAQUE;
        case F19:
            naginata_config.tategaki = false;
            return ZMK_BEHAVIOR_OPAQUE;
    }

    timestamp = event.timestamp;
    naginata_press(binding, event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    if (g_mejiro_safe_mode == 3) {
        return 0;
    }

    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    timestamp = event.timestamp;
    naginata_release(binding, event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_naginata_driver_api = {
    .binding_pressed = on_keymap_binding_pressed, .binding_released = on_keymap_binding_released};

#define KP_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_naginata_init, NULL, NULL, NULL, POST_KERNEL,              \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_naginata_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)



// ================================
// Embedded Mejiro transform (ZMK)
//  - Adapted from QMK sources you provided
//  - Commands/abbrev/verb are currently stubbed (incremental integration)
// ================================
typedef struct {
    char output[128];
    size_t kana_length;
    bool success;
} mejiro_result_t_zmk;



/* 
 * Temporary stub: accept any argument list used by the Mejiro port.
 * Returns success=false so the caller falls back to the non-verb path.
 */

// Stubs: (next step we can wire real tables)

static void kana_to_roma_zmk(const char *kana_input, char *roma_output, size_t output_size);
static mejiro_result_t_zmk mejiro_transform_zmk(const char *mejiro_id);
static void mejiro_clear_pending_tsu_zmk(void);

// Safe append helper for kana_to_roma_zmk (always NUL-terminates)
static void str_append_bounded(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return;
    }
    size_t dlen = strnlen(dst, dst_size);
    if (dlen >= dst_size - 1) {
        dst[dst_size - 1] = '\0';
        return;
    }
    if (!src) {
        return;
    }
    size_t rem = dst_size - 1 - dlen;
    size_t slen = strnlen(src, rem);
    memcpy(dst + dlen, src, slen);
    dst[dlen + slen] = '\0';
}

/*
 * Mejiro transformation logic implementation
 */
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

/* ------------------------------------------------------------
 *  Async ASCII sender (prevents long blocking loops that can
 *  cause BLE supervision timeout resets ~4s after key press).
 *  Debug-first: sends only ASCII letters (a-z/A-Z) and space.
 * ------------------------------------------------------------ */
#include <zephyr/kernel.h>

static struct k_work_delayable g_mejiro_send_work;
static char g_mejiro_send_buf[128];
static volatile size_t g_mejiro_send_len;
static volatile size_t g_mejiro_send_pos;

static void mejiro_send_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    size_t pos = g_mejiro_send_pos;
    if (pos >= g_mejiro_send_len) {
        return;
    }

    char c = g_mejiro_send_buf[pos];

    /* We reuse existing minimal sender to avoid large keycode maps here. */
    char one[2] = {c, '\0'};
    send_ascii_letters_only(one);

    g_mejiro_send_pos = pos + 1;

    /* Send next char shortly after to yield CPU/BLE. */
    (void)k_work_schedule(&g_mejiro_send_work, K_MSEC(2));
}

static void mejiro_queue_send_ascii(const char *s) {
    if (!s) {
        return;
    }

    /* Copy (truncate) */
    size_t w = 0;
    for (size_t r = 0; s[r] != '\0' && w < (sizeof(g_mejiro_send_buf) - 1); r++) {
        char c = s[r];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == ' ')) {
            g_mejiro_send_buf[w++] = c;
        }
    }
    g_mejiro_send_buf[w] = '\0';
    g_mejiro_send_len = w;
    g_mejiro_send_pos = 0;

    /* (Re)start worker */
    k_work_cancel_delayable(&g_mejiro_send_work);
    k_work_init_delayable(&g_mejiro_send_work, mejiro_send_work_handler);
    (void)k_work_schedule(&g_mejiro_send_work, K_NO_WAIT);
}


// -----------------------------------------------------------------------------
// Forward declarations for helpers used before definition
// -----------------------------------------------------------------------------
static uint32_t keycode_from_ascii_letter(char c);


/* --------------------------------------------------------------------------
 * Compatibility helpers for "single-file" builds.
 *  - Some upstream variants expose `mejiro_commands` but not an explicit count.
 *  - Provide a safe macro alias when possible.
 * -------------------------------------------------------------------------- */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* If no count symbol is provided, derive it from the command table (when it is
 * a real array in this translation unit).
 */
#ifndef mejiro_command_count
#define mejiro_command_count 0
#endif


// 前回の母音を保存（省略時に使用）
static char last_vowel_stroke[8] = "A";  // デフォルトは"A"

// 「っ」の持ち越し状態
static bool pending_tsu = false;

// 「っ」の持ち越し状態をクリア（外部から呼び出し可能）
void mejiro_clear_pending_tsu_zmk(void) {
    pending_tsu = false;
}

// ひらがな→ヘボン式ローマ字変換テーブル
typedef struct {
    const char *kana;
    const char *roma;
} kana_roma_t;

static const kana_roma_t kana_roma_table[] = {
    // 基本五十音
    {"あ", "a"}, {"い", "i"}, {"う", "u"}, {"え", "e"}, {"お", "o"},
    {"か", "ka"}, {"き", "ki"}, {"く", "ku"}, {"け", "ke"}, {"こ", "ko"},
    {"さ", "sa"}, {"し", "shi"}, {"す", "su"}, {"せ", "se"}, {"そ", "so"},
    {"た", "ta"}, {"ち", "chi"}, {"つ", "tsu"}, {"て", "te"}, {"と", "to"},
    {"な", "na"}, {"に", "ni"}, {"ぬ", "nu"}, {"ね", "ne"}, {"の", "no"},
    {"は", "ha"}, {"ひ", "hi"}, {"ふ", "fu"}, {"へ", "he"}, {"ほ", "ho"},
    {"ま", "ma"}, {"み", "mi"}, {"む", "mu"}, {"め", "me"}, {"も", "mo"},
    {"や", "ya"}, {"ゆ", "yu"}, {"よ", "yo"},
    {"ら", "ra"}, {"り", "ri"}, {"る", "ru"}, {"れ", "re"}, {"ろ", "ro"},
    {"わ", "wa"}, {"ゐ", "wi"}, {"ゑ", "we"}, {"を", "wo"}, {"ん", "nn"},

    // 濁音
    {"が", "ga"}, {"ぎ", "gi"}, {"ぐ", "gu"}, {"げ", "ge"}, {"ご", "go"},
    {"ざ", "za"}, {"じ", "ji"}, {"ず", "zu"}, {"ぜ", "ze"}, {"ぞ", "zo"},
    {"だ", "da"}, {"ぢ", "di"}, {"づ", "du"}, {"で", "de"}, {"ど", "do"},
    {"ば", "ba"}, {"び", "bi"}, {"ぶ", "bu"}, {"べ", "be"}, {"ぼ", "bo"},

    // 半濁音
    {"ぱ", "pa"}, {"ぴ", "pi"}, {"ぷ", "pu"}, {"ぺ", "pe"}, {"ぽ", "po"},

    // 拗音(きゃ系)
    {"きゃ", "kya"}, {"きゅ", "kyu"}, {"きょ", "kyo"},
    {"しゃ", "sha"}, {"しゅ", "shu"}, {"しょ", "sho"},
    {"ちゃ", "cha"}, {"ちゅ", "chu"}, {"ちょ", "cho"},
    {"にゃ", "nya"}, {"にゅ", "nyu"}, {"にょ", "nyo"},
    {"ひゃ", "hya"}, {"ひゅ", "hyu"}, {"ひょ", "hyo"},
    {"みゃ", "mya"}, {"みゅ", "myu"}, {"みょ", "myo"},
    {"りゃ", "rya"}, {"りゅ", "ryu"}, {"りょ", "ryo"},
    {"ぎゃ", "gya"}, {"ぎゅ", "gyu"}, {"ぎょ", "gyo"},
    {"じゃ", "ja"}, {"じゅ", "ju"}, {"じょ", "jo"},
    {"ぢゃ", "dya"}, {"ぢゅ", "dyu"}, {"ぢょ", "dyo"},
    {"びゃ", "bya"}, {"びゅ", "byu"}, {"びょ", "byo"},
    {"ぴゃ", "pya"}, {"ぴゅ", "pyu"}, {"ぴょ", "pyo"},

    // 特殊音
    {"ゔ", "vu"}, {"ゔぁ", "va"}, {"ゔぃ", "vi"}, {"ゔぇ", "ve"}, {"ゔぉ", "vo"},
    {"ゔゅ", "vyu"},
    {"うぃ", "wi"}, {"うぇ", "we"}, {"うぉ", "wo"},
    {"ふぁ", "fa"}, {"ふぃ", "fi"}, {"ふぇ", "fe"}, {"ふぉ", "fo"},
    {"ふゃ", "fya"}, {"ふゅ", "fyu"}, {"ふょ", "fyo"},

    // 小書き文字
    {"ぁ", "xa"}, {"ぃ", "xi"}, {"ぅ", "xu"}, {"ぇ", "xe"}, {"ぉ", "xo"},
    {"ゃ", "xya"}, {"ゅ", "xyu"}, {"ょ", "xyo"},
    {"っ", "xtu"}, {"ゎ", "xwa"},

    // 長音符
    {"ー", "-"},

    // 句読点
    {"、", ","}, {"。", "."}, {"!", "!"}, {"!", "!"}, {"?", "?"}, {"?", "?"},

    {NULL, NULL}
};

static size_t utf8_char_count(const char *s) {
    size_t count = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if ((c & 0xC0) != 0x80) {
            count++;
        }
        s++;
    }
    return count;
}

// ひらがなをヘボン式ローマ字に変換（最長一致 + 促音対応）
void kana_to_roma_zmk(const char *kana_input, char *roma_output, size_t output_size) {
    roma_output[0] = '\0';
    const char *p = kana_input;

    while (*p && strlen(roma_output) < output_size - 10) {
        // 促音（小さい「っ」）の処理：次の音の頭子音を重ねる
        if (strncmp(p, "っ", 3) == 0) {
            // 次のエントリを最長一致で先読み
            const kana_roma_t *best = NULL;
            size_t best_len = 0;
            for (const kana_roma_t *entry = kana_roma_table; entry->kana != NULL; entry++) {
                size_t kana_len = strlen(entry->kana);
                if (kana_len > best_len && strncmp(p + 3, entry->kana, kana_len) == 0) {
                    best = entry;
                    best_len = kana_len;
                }
            }
            if (best && best->roma && best->roma[0] != '\0') {
                char c = best->roma[0];
                // 母音頭の場合は重ねられないので「xtu」を出力
                if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
                    str_append_bounded(roma_output, output_size, "xtu");
                } else {
                    // 子音頭の場合は子音を重ねる
                    size_t len = strlen(roma_output);
                    if (len < output_size - 2) {
                        roma_output[len] = c;
                        roma_output[len + 1] = '\0';
                    }
                }
            } else {
                // 次の文字がない場合も「xtu」を出力
                str_append_bounded(roma_output, output_size, "xtu");
            }
            p += 3; // 「っ」を消費して続行
            continue;
        }

        // 最長一致検索（拗音などを優先）
        const kana_roma_t *best = NULL;
        size_t best_len = 0;
        for (const kana_roma_t *entry = kana_roma_table; entry->kana != NULL; entry++) {
            size_t kana_len = strlen(entry->kana);
            if (kana_len > best_len && strncmp(p, entry->kana, kana_len) == 0) {
                best = entry;
                best_len = kana_len;
            }
        }

        if (best) {
            str_append_bounded(roma_output, output_size, best->roma);
            p += best_len;
            continue;
        }

        if ((unsigned char)*p < 128) {
            char tmp[2] = { *p, '\0' };
            str_append_bounded(roma_output, output_size, tmp);
            p++;
        } else {
            p++;
        }
    }
}

// 子音ストローク→ローマ字マッピング
typedef struct {
    const char *stroke;
    const char *roma;
} conso_map_t;

static const conso_map_t conso_table[] = {
    {"", ""}, {"S", "s"}, {"T", "t"}, {"K", "k"}, {"N", "n"},
    {"ST", "r"}, {"SK", "w"}, {"TK", "h"},
    {"SN", "z"}, {"TN", "d"}, {"KN", "g"}, {"TKN", "b"},
    {"STK", "p"}, {"STN", "l"}, {"SKN", "m"}, {"STKN", "f"}
};

// 母音ストローク→ローマ字マッピング
typedef struct {
    const char *stroke;
    const char *roma;
    uint8_t index;
} vowel_map_t;

static const vowel_map_t vowel_table[] = {
    {"A", "a", 0}, {"I", "i", 1}, {"U", "u", 2}, {"IA", "e", 3}, {"AU", "o", 4},
    {"YA", "ya", 5}, {"YU", "yu", 6}, {"YAU", "yo", 7}
};

// 行段→ひらがなマッピング
static const char *kana_table[][8] = {
    {"あ", "い", "う", "え", "お", "や", "ゆ", "よ"},     // ""
    {"か", "き", "く", "け", "こ", "きゃ", "きゅ", "きょ"}, // k
    {"さ", "し", "す", "せ", "そ", "しゃ", "しゅ", "しょ"}, // s
    {"た", "ち", "つ", "て", "と", "ちゃ", "ちゅ", "ちょ"}, // t
    {"な", "に", "ぬ", "ね", "の", "にゃ", "にゅ", "にょ"}, // n
    {"は", "ひ", "ふ", "へ", "ほ", "ひゃ", "ひゅ", "ひょ"}, // h
    {"ま", "み", "む", "め", "も", "みゃ", "みゅ", "みょ"}, // m
    {"ら", "り", "る", "れ", "ろ", "りゃ", "りゅ", "りょ"}, // r
    {"わ", "うぃ", "ゔ", "うぇ", "うぉ", "わ", "ゔゅ", "を"}, // w
    {"が", "ぎ", "ぐ", "げ", "ご", "ぎゃ", "ぎゅ", "ぎょ"}, // g
    {"ざ", "じ", "ず", "ぜ", "ぞ", "じゃ", "じゅ", "じょ"}, // z
    {"だ", "ぢ", "づ", "で", "ど", "ぢゃ", "ぢゅ", "ぢょ"}, // d
    {"ば", "び", "ぶ", "べ", "ぼ", "びゃ", "びゅ", "びょ"}, // b
    {"ぱ", "ぴ", "ぷ", "ぺ", "ぽ", "ぴゃ", "ぴゅ", "ぴょ"}, // p
    {"ふぁ", "ふぃ", "ふゅ", "ふぇ", "ふぉ", "ふゃ", "ふゅ", "ふょ"}, // f
    {"ぁ", "ぃ", "ぅ", "ぇ", "ぉ", "ゃ", "ゅ", "ょ"}      // l
};

static const char *roma_order[] = {"", "k", "s", "t", "n", "h", "m", "r", "w", "g", "z", "d", "b", "p", "f", "l"};

// 二重母音マッピング
typedef struct {
    const char *stroke;
    const char *first_vowel;
    const char *suffix;
} diphthong_map_t;

static const diphthong_map_t diphthong_table[] = {
    {"Y", "a", "い"},
    {"YI", "yo", "う"},
    {"YIA", "e", "い"},
    {"YIU", "yu", "う"},
    {"YIAU", "u", "う"},
    {"IU", "u", "い"},
    {"IAU", "o", "う"}
};

// 複雑二重母音マッピング（親指を使う外来音）
typedef struct {
    const char *stroke;  // 母音+助詞ストローク
    const char *first_vowel;
    const char *suffix;
} complex_diphthong_map_t;

static const complex_diphthong_map_t complex_diphthong_table[] = {
    // ～ん
    {"IAUn", "a", "うん"},     // ~oun, ~own
    {"YIn", "i", "んぐ"},      // ~ing
    {"YIUn", "ya", "る"},      // ~ial
    {"YIAUn", "a", "る"},      // ~al
    // ～っ
    {"IAUtk", "a", "う"},      // ~ou, ~ow
    {"YItk", "i", "ずむ"},    // ~ism
    {"YIUtk", "i", "すと"},   // ~ist
    {"Ytk", "a", "いざ－"}, // ~izer
    {"YIAtk", "e", "んす"},   // ~ense, ~ence
    {"YIAUtk", "u", "る"},     // ~ul
    // ～ー
    {"IAUntk", "a", "ぶる"},  // ~able
    {"YIntk", "i", "かる"},   // ~ical
    {"YIUntk", "i", "てぃ－"}, // ~ity
    {"YIAUntk", "o", "じ－"}   // ~ogy
};

// 例外的なかなのマッピング
typedef struct {
    const char *stroke;  // 子音+母音ストローク
    const char *kana;
} exception_kana_t;

static const exception_kana_t exception_kana_table[] = {
    {"TNYI", "どぅ"},
    {"TNYIU", "とぅ"},
    {"TNYA", "てぃ"},
    {"TNYAU", "でぃ"},
    {"TNYU", "でゅ"},
    {"TNIU", "ちぇ"},
    {"TNYIAU", "てゅ"},

    {"SKYI", "ゐ"},
    {"SKYIU", "ゑ"},
    {"SKYA", "いぇ"},
    {"SKYAU", "を"},
    {"SKYU", "ゔゅ"},
    {"SKIU", "ゆい"},
    {"SKIAU", "ゎ"},
    {"SKYIAU", "うぁ"},

    {"STKNYI", "ゔぉ"},
    {"STKNYIU", "ゔぇ"},
    {"STKNYA", "しぇ"},
    {"STKNYAU", "じぇ"},
    {"STKNYU", "ゔぃ"},
    {"STKNIU", "ゔぁ"},

    {"STNIU", "つぃ"},
    {"STNIAU", "つぉ"},
    {"STNYIAU", "てゃ"},
    {"STNY", "つぁ"},
    {"STNYIA", "つぇ"},
    {NULL, NULL}
};

// 追加音リスト
static const char *second_sound_list[] = {"", "ん", "つ", "く", "っ", "ち", "き", "ー"};
static const char *particle_key_list[] = {"", "n", "t", "k", "tk", "nt", "nk", "ntk"};

// 左側助詞
static const char *l_particle[] = {"", "、", "に", "の", "で", "と", "を", "へ"};
// 右側助詞
static const char *r_particle[] = {"", "、", "は", "が", "も", "は、", "が、", "も、"};

// メジロIDをパース（左側・右側に分離）
static void parse_mejiro_id(const char *id, char *left, size_t left_sz, char *right, size_t right_sz) {
    if (left_sz == 0 || right_sz == 0) {
        return;
    }

    const char *hyphen = (id != NULL) ? strchr(id, '-') : NULL;

    if (id == NULL) {
        left[0] = ' ';
        right[0] = ' ';
        return;
    }

    if (hyphen == NULL) {
        /* Entire id goes to left. */
        strncpy(left, id, left_sz - 1);
        left[left_sz - 1] = ' ';
        right[0] = ' ';
        return;
    }

    /* Split at '-' */
    size_t left_len = (size_t)(hyphen - id);
    if (left_len >= left_sz) {
        left_len = left_sz - 1;
    }
    strncpy(left, id, left_len);
    left[left_len] = ' ';

    const char *rhs = hyphen + 1;
    strncpy(right, rhs, right_sz - 1);
    right[right_sz - 1] = ' ';
}

// 子音ストロークからローマ字を取得
static const char *get_conso_roma(const char *stroke) {
    for (uint8_t i = 0; i < sizeof(conso_table)/sizeof(conso_table[0]); i++) {
        if (strcmp(stroke, conso_table[i].stroke) == 0) {
            return conso_table[i].roma;
        }
    }
    return NULL;
}

// 母音ストロークから母音インデックスを取得
static int get_vowel_index(const char *stroke) {
    // 二重母音チェック
    for (uint8_t i = 0; i < sizeof(diphthong_table)/sizeof(diphthong_table[0]); i++) {
        if (strcmp(stroke, diphthong_table[i].stroke) == 0) {
            // first_vowelから基本母音インデックスを取得
            for (uint8_t j = 0; j < sizeof(vowel_table)/sizeof(vowel_table[0]); j++) {
                if (strcmp(vowel_table[j].roma, diphthong_table[i].first_vowel) == 0) {
                    return vowel_table[j].index;
                }
            }
        }
    }

    // 基本母音チェック
    for (uint8_t i = 0; i < sizeof(vowel_table)/sizeof(vowel_table[0]); i++) {
        if (strcmp(stroke, vowel_table[i].stroke) == 0) {
            return vowel_table[i].index;
        }
    }
    return -1;
}

// ローマ字からkana_tableのインデックスを取得
static int get_roma_index(const char *roma) {
    for (uint8_t i = 0; i < sizeof(roma_order)/sizeof(roma_order[0]); i++) {
        if (strcmp(roma, roma_order[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// 二重母音のsuffixを取得
static const char *get_diphthong_suffix(const char *stroke) {
    for (uint8_t i = 0; i < sizeof(diphthong_table)/sizeof(diphthong_table[0]); i++) {
        if (strcmp(stroke, diphthong_table[i].stroke) == 0) {
            return diphthong_table[i].suffix;
        }
    }
    return "";
}

// 複雑二重母音をチェック（母音+助詞ストローク）
static bool check_complex_diphthong(const char *vowel_particle, const char **first_vowel, const char **suffix) {
    for (uint8_t i = 0; i < sizeof(complex_diphthong_table)/sizeof(complex_diphthong_table[0]); i++) {
        if (strcmp(vowel_particle, complex_diphthong_table[i].stroke) == 0) {
            *first_vowel = complex_diphthong_table[i].first_vowel;
            *suffix = complex_diphthong_table[i].suffix;
            return true;
        }
    }
    return false;
}

// 例外的なかなをチェック（子音+母音ストローク）
static const char *check_exception_kana(const char *conso_vowel) {
    for (uint8_t i = 0; exception_kana_table[i].stroke != NULL; i++) {
        if (strcmp(conso_vowel, exception_kana_table[i].stroke) == 0) {
            return exception_kana_table[i].kana;
        }
    }
    return NULL;
}

// 追加音を取得
static const char *get_second_sound(const char *particle) {
    for (uint8_t i = 0; i < sizeof(particle_key_list)/sizeof(particle_key_list[0]); i++) {
        if (strcmp(particle, particle_key_list[i]) == 0) {
            return second_sound_list[i];
        }
    }
    return "";
}


// 子音・母音・助詞から、かな文字列を生成する共通関数
// include_extra_sound: 追加音を含めるかどうか（左+助詞パターンではfalse）
static void convert_to_kana(const char *conso, const char *vowel, const char *particle_str,
                           bool include_extra_sound, char *out) {
    out[0] = '\0';  // 初期化

    // 例外的なかなのマッピングをチェック
    char conso_vowel[32];
    strcpy(conso_vowel, conso);
    strcat(conso_vowel, vowel);
    const char *exception_kana = check_exception_kana(conso_vowel);
    
    // STN+母音なしは「何も出力しない」特例（追加音のみ必要なケース）
    if (strlen(conso) > 0 && strlen(vowel) == 0 && strcmp(conso, "STN") == 0) {
        if (include_extra_sound) {
            const char *extra = get_second_sound(particle_str);
            strcpy(out, extra);
        } else {
            out[0] = '\0';
        }
        return;
    }

    if (exception_kana != NULL) {
        // 例外かなを使用
        strcpy(out, exception_kana);
        if (include_extra_sound) {
            const char *extra = get_second_sound(particle_str);
            strcat(out, extra);
        }
    } else {
        // 複雑二重母音をチェック（母音+助詞）
        char vowel_particle[32];
        strcpy(vowel_particle, vowel);
        strcat(vowel_particle, particle_str);
        const char *first_vowel = NULL;
        const char *suffix = NULL;
        bool is_complex = check_complex_diphthong(vowel_particle, &first_vowel, &suffix);

        const char *c_roma = get_conso_roma(conso);
        if (c_roma == NULL) c_roma = "";

        int v_idx;
        const char *extra_sound = "";

        if (is_complex) {
            // 複雑二重母音の場合、first_vowelからインデックスを取得
            v_idx = -1;
            for (uint8_t j = 0; j < sizeof(vowel_table)/sizeof(vowel_table[0]); j++) {
                if (strcmp(vowel_table[j].roma, first_vowel) == 0) {
                    v_idx = vowel_table[j].index;
                    break;
                }
            }
            if (v_idx < 0) v_idx = 0;
            // 複雑二重母音は追加音なし
        } else {
            // 通常の二重母音または基本母音
            v_idx = get_vowel_index(vowel);
            if (v_idx < 0) v_idx = 0;
            suffix = get_diphthong_suffix(vowel);
            if (include_extra_sound) {
                extra_sound = get_second_sound(particle_str);
            }
        }

        int c_idx = get_roma_index(c_roma);
        if (c_idx < 0) c_idx = 0;

        const char *base_kana = kana_table[c_idx][v_idx];

        // 英語モードの場合、ち→てぃ、ぢ→でぃに置き換え
        if (is_complex) {
            if (strcmp(base_kana, "ち") == 0) {
                base_kana = "てぃ";
            } else if (strcmp(base_kana, "ぢ") == 0) {
                base_kana = "でぃ";
            }
        }

        strcpy(out, base_kana);
        strcat(out, suffix);
        strcat(out, extra_sound);
    }
}

// 助詞変換
static void transform_joshi(const char *left_stroke, const char *right_stroke, char *out) {
    // right_strokeからnを除去
    char right_tk[16] = {0};
    bool has_comma = false;

    if (strstr(right_stroke, "n") != NULL) {
        has_comma = true;
        // nを除去
        const char *p = right_stroke;
        char *q = right_tk;
        while (*p) {
            if (*p != 'n') *q++ = *p;
            p++;
        }
        *q = '\0';
    } else {
        strcpy(right_tk, right_stroke);
    }

    // インデックス取得
    int l_idx = -1, r_idx = -1;
    for (uint8_t i = 0; i < sizeof(particle_key_list)/sizeof(particle_key_list[0]); i++) {
        if (strcmp(left_stroke, particle_key_list[i]) == 0) l_idx = i;
        if (strcmp(right_tk, particle_key_list[i]) == 0) r_idx = i;
    }

    if (l_idx < 0 || r_idx < 0) {
        out[0] = '\0';
        return;
    }

    // 助詞組み立て
    if (strcmp(left_stroke, "n") == 0) {
        strcpy(out, r_particle[r_idx]);
        strcat(out, "、");
    } else if ((strcmp(right_stroke, "k") == 0 || strcmp(right_stroke, "nk") == 0) &&
               strcmp(left_stroke, "") != 0 && strcmp(left_stroke, "k") != 0 && strcmp(left_stroke, "ntk") != 0) {
        strcpy(out, "の");
        strcat(out, l_particle[l_idx]);
        if (has_comma) strcat(out, "、");
    } else {
        strcpy(out, l_particle[l_idx]);
        strcat(out, r_particle[r_idx]);
        if (has_comma) strcat(out, "、");
    }
}

mejiro_result_t_zmk mejiro_transform_zmk(const char *mejiro_id) {
    mejiro_result_t_zmk result = {{0}, 0, false};

    char left[64] = {0};
    char right[64] = {0};
    parse_mejiro_id(mejiro_id, left, sizeof(left), right, sizeof(right));

    // 削除操作（-U、-AU）の場合は持ち越しの「っ」をクリア
    if (strcmp(mejiro_id, "-U") == 0 || strcmp(mejiro_id, "-AU") == 0) {
        pending_tsu = false;
    }

    // 全押し（STKNYIAUntk#-STKNYIAUntk*）は「何も出力しない」ことで誤入力をキャンセル
    if (strcmp(left, "STKNYIAUntk#") == 0 && strcmp(right, "STKNYIAUntk*") == 0) {
        return result;  // output is empty, success remains false
    }
    
    // 左側のパース: 子音(STKN) + 母音(YIAU) + 助詞(ntk)
    char l_conso[16] = {0};
    char l_vowel[16] = {0};
    char l_particle_str[16] = {0};

    const char *p = left;
    while (*p && (*p == 'S' || *p == 'T' || *p == 'K' || *p == 'N')) {
        size_t len = strlen(l_conso);
        l_conso[len] = *p;
        l_conso[len + 1] = '\0';
        p++;
    }
    while (*p && (*p == 'Y' || *p == 'I' || *p == 'A' || *p == 'U')) {
        size_t len = strlen(l_vowel);
        l_vowel[len] = *p;
        l_vowel[len + 1] = '\0';
        p++;
    }
    while (*p && (*p == 'n' || *p == 't' || *p == 'k' || *p == '#')) {
        size_t len = strlen(l_particle_str);
        l_particle_str[len] = *p;
        l_particle_str[len + 1] = '\0';
        p++;
    }

    // 右側のパース: 子音(STKN) + 母音(YIAU) + 助詞(ntk) + 末尾のアスタリスク(*)
    char r_conso[16] = {0};
    char r_vowel[16] = {0};
    char r_particle_str[16] = {0};
    bool has_asterisk = false;

    p = right;
    while (*p && (*p == 'S' || *p == 'T' || *p == 'K' || *p == 'N')) {
        size_t len = strlen(r_conso);
        r_conso[len] = *p;
        r_conso[len + 1] = '\0';
        p++;
    }
    while (*p && (*p == 'Y' || *p == 'I' || *p == 'A' || *p == 'U')) {
        size_t len = strlen(r_vowel);
        r_vowel[len] = *p;
        r_vowel[len + 1] = '\0';
        p++;
    }
    while (*p && (*p == 'n' || *p == 't' || *p == 'k')) {
        size_t len = strlen(r_particle_str);
        r_particle_str[len] = *p;
        r_particle_str[len + 1] = '\0';
        p++;
    }
    if (*p == '*') {
        has_asterisk = true;
        p++;
    }

    // 動詞活用チェック（通常の変換より先に）。Plover版同様、アスタリスクがある場合のみ動詞略語を試行。
    if (has_asterisk) {
        // 完全なストロークを構築（アスタリスクを除く）
        char full_stroke[128];
        snprintf(full_stroke, sizeof(full_stroke), "%s%s%s-%s%s%s",
                 l_conso, l_vowel, l_particle_str,
                 r_conso, r_vowel, r_particle_str);
        
        // 一般略語用のストローク（助詞なし）
        char abbr_stroke[128];
        snprintf(abbr_stroke, sizeof(abbr_stroke), "%s%s-%s%s",
                 l_conso, l_vowel, r_conso, r_vowel);
        
        // ユーザー略語チェック（最優先、助詞込み）
        abbreviation_result_t user_abbr = mejiro_user_abbreviation(full_stroke);
        if (user_abbr.success) {
            result.kana_length = utf8_char_count(user_abbr.output);
            kana_to_roma_zmk(user_abbr.output, result.output, sizeof(result.output));
            result.success = true;
            return result;
        }

        // 一般略語チェック（助詞なし）
        abbreviation_result_t abstract_abbr = mejiro_abstract_abbreviation(abbr_stroke);
        if (abstract_abbr.success) {
            // 一般略語の出力に助詞を追加
            char kana_output[256] = {0};
            strcpy(kana_output, abstract_abbr.output);
            result.kana_length = utf8_char_count(kana_output);
            
            // 助詞がある場合は追加
            if (strlen(l_particle_str) > 0 || strlen(r_particle_str) > 0) {
                // 助詞パターンを構築
                char particle_pattern[32];
                snprintf(particle_pattern, sizeof(particle_pattern), "%s-%s", l_particle_str, r_particle_str);
                
                // 特定の助詞パターンに対して語尾に変換
                if (strcmp(particle_pattern, "n-") == 0) {
                    strcat(kana_output, "である");
                } else if (strcmp(particle_pattern, "-n") == 0) {
                    strcat(kana_output, "だ");
                } else if (strcmp(particle_pattern, "n-n") == 0) {
                    strcat(kana_output, "だった");
                } else if (strcmp(particle_pattern, "-ntk") == 0) {
                    strcat(kana_output, "です");
                } else if (strcmp(particle_pattern, "n-ntk") == 0) {
                    strcat(kana_output, "でした");
                } else {
                    // その他の助詞は通常通り処理
                    char joshi_output[128] = {0};
                    transform_joshi(l_particle_str, r_particle_str, joshi_output);
                    strcat(kana_output, joshi_output);
                }
                result.kana_length = utf8_char_count(kana_output);
            }
            
            kana_to_roma_zmk(kana_output, result.output, sizeof(result.output));
            result.success = true;
            return result;
        }

        // 動詞略語チェック
        char left_kana_temp[64] = {0};
        char right_kana_temp[64] = {0};
        // 左側の仮名を生成（動詞語幹用）
        if (strlen(l_conso) > 0 || strlen(l_vowel) > 0) {
            convert_to_kana(l_conso, l_vowel, "", false, left_kana_temp);
        }
        // 右側の仮名を生成（動詞語幹用）
        if (strlen(r_conso) > 0 || strlen(r_vowel) > 0) {
            convert_to_kana(r_conso, r_vowel, "", false, right_kana_temp);
        }

        verb_result_t verb_result = mejiro_verb_conjugate(
            l_conso, l_vowel, l_particle_str,
            r_conso, r_vowel, r_particle_str,
            left_kana_temp, right_kana_temp
        );

        if (verb_result.success) {
            // 動詞活用結果をローマ字に変換して返す
            char kana_output[128];
            strcpy(kana_output, verb_result.output);
            result.kana_length = utf8_char_count(kana_output);
            kana_to_roma_zmk(kana_output, result.output, sizeof(result.output));
            result.success = true;
            return result;
        }
    }

    // 助詞のみの場合は助詞変換
    bool is_particle_only = (strlen(l_conso) == 0 && strlen(l_vowel) == 0 &&
                             strlen(r_conso) == 0 && strlen(r_vowel) == 0 &&
                             (strlen(l_particle_str) > 0 || strlen(r_particle_str) > 0));

    // 左+助詞かどうかを先に判定（左側の追加音を除外するため）
    // ただし、ntk-nの場合は除外（これは左側の追加音「ーん」として処理すべき）
    bool is_left_plus_particle = ((strlen(l_conso) > 0 || strlen(l_vowel) > 0) &&
                                  strlen(r_conso) == 0 && strlen(r_vowel) == 0 &&
                                  strlen(r_particle_str) > 0 &&
                                  !(strcmp(l_particle_str, "ntk") == 0 && strcmp(r_particle_str, "n") == 0));

    // ntk-nの特殊ケース: 左の追加音と右の助詞として処理
    bool is_ntk_n = (strlen(l_conso) > 0 || strlen(l_vowel) > 0) &&
                   strlen(r_conso) == 0 && strlen(r_vowel) == 0 &&
                   strcmp(l_particle_str, "ntk") == 0 && strcmp(r_particle_str, "n") == 0;

    // 母音の補完処理
    // 左側の母音が空で、左側に子音がある場合、前回の母音を使用
    if (strlen(l_vowel) == 0 && strlen(l_conso) > 0 && strcmp(l_conso, "STN") != 0) {
        strcpy(l_vowel, last_vowel_stroke);
    }
    // 右側の母音が空で、右側に子音がある場合、左の母音を使用（左も空なら前回の母音）
    if (strlen(r_vowel) == 0 && strlen(r_conso) > 0 && strcmp(r_conso, "STN") != 0) {
        if (strlen(l_vowel) > 0) {
            strcpy(r_vowel, l_vowel);
        } else {
            strcpy(r_vowel, last_vowel_stroke);
        }
    }

    // 使用した母音を保存（右が優先、なければ左、どちらもなければ保存しない）
    if (strlen(r_vowel) > 0) {
        strcpy(last_vowel_stroke, r_vowel);
    } else if (strlen(l_vowel) > 0) {
        strcpy(last_vowel_stroke, l_vowel);
    }
    
    // 「っ」の持ち越し判定（左側と右側を別々に判定）
    // ただし、複雑二重母音（母音+助詞）に該当する場合は「っ」を出力しないため、持ち越し対象から除外する
    char left_vowel_particle[32] = {0};
    strcpy(left_vowel_particle, l_vowel);
    strcat(left_vowel_particle, l_particle_str);
    const char *dummy_first_vowel_left = NULL;
    const char *dummy_suffix_left = NULL;
    bool is_complex_left = check_complex_diphthong(left_vowel_particle, &dummy_first_vowel_left, &dummy_suffix_left);

    char right_vowel_particle[32] = {0};
    strcpy(right_vowel_particle, r_vowel);
    strcat(right_vowel_particle, r_particle_str);
    const char *dummy_first_vowel_right = NULL;
    const char *dummy_suffix_right = NULL;
    bool is_complex_right = check_complex_diphthong(right_vowel_particle, &dummy_first_vowel_right, &dummy_suffix_right);

    bool has_final_tsu_left = ((strlen(l_conso) > 0 || strlen(l_vowel) > 0) &&
                               strcmp(l_particle_str, "tk") == 0 &&
                               strlen(r_conso) == 0 && strlen(r_vowel) == 0 && strlen(r_particle_str) == 0 &&
                               !is_complex_left);
    bool has_final_tsu_right = ((strlen(r_conso) > 0 || strlen(r_vowel) > 0) &&
                                strcmp(r_particle_str, "tk") == 0 &&
                                !is_left_plus_particle &&
                                !is_complex_right);
    bool has_final_tsu = has_final_tsu_left || has_final_tsu_right;
    
    if (is_particle_only) {
        transform_joshi(l_particle_str, r_particle_str, result.output);
    } else {
        // 前回持ち越しの「っ」を先頭に追加
        if (pending_tsu) {
            strcpy(result.output, "っ");
            pending_tsu = false;
        } else {
            result.output[0] = '\0';
        }
        
        // 「っ」の持ち越し判定
        // 左側のみでtk助詞があり、右側がない場合（複雑二重母音は除外）
        if ((strlen(l_conso) > 0 || strlen(l_vowel) > 0) &&
            strcmp(l_particle_str, "tk") == 0 &&
            strlen(r_conso) == 0 && strlen(r_vowel) == 0 && strlen(r_particle_str) == 0 &&
            !is_complex_left) {
            has_final_tsu = true;
        }
        // 右側にtk助詞がある場合（左+助詞パターンでない、かつ複雑二重母音は除外）
        if ((strlen(r_conso) > 0 || strlen(r_vowel) > 0) &&
            strcmp(r_particle_str, "tk") == 0 &&
            !is_left_plus_particle &&
            !is_complex_right) {
            has_final_tsu = true;
        }
        
        // 左側変換
        if (strlen(l_conso) > 0 || strlen(l_vowel) > 0) {
            char left_kana[64] = {0};
            // 左側自体が「っ」で終わる場合のみ追加音を除外
            bool include_extra = !is_left_plus_particle && !has_final_tsu_left;
            convert_to_kana(l_conso, l_vowel, l_particle_str, include_extra, left_kana);
            strcat(result.output, left_kana);
        }

        // 左+助詞: 左側にかながあり、右側には助詞のみがある場合
        // ただし、左の追加音が「ntk」で右の助詞が「n」の場合（追加音が「ー」+「ん」）は例外
        if (is_left_plus_particle) {

            // コマンドテーブルは単体ビルドでは無効化: transform_joshi で助詞を生成
            char joshi_output[64] = {0};
            transform_joshi(l_particle_str, r_particle_str, joshi_output);
            strcat(result.output, joshi_output);
        }
        // 左のかながなくても、左の追加音キーがあってかつ右のかながある場合
        // 左の追加音を先に出力
        if (strlen(l_conso) == 0 && strlen(l_vowel) == 0 &&
            strlen(l_particle_str) > 0 &&
            (strlen(r_conso) > 0 || strlen(r_vowel) > 0)) {
            const char *left_extra_sound = get_second_sound(l_particle_str);
            strcat(result.output, left_extra_sound);
        }

        // 右側変換
        if (strlen(r_conso) > 0 || strlen(r_vowel) > 0) {
            char right_kana[64] = {0};
            // 右側自体が「っ」で終わる場合のみ追加音を除外
            bool include_extra = !has_final_tsu_right;
            convert_to_kana(r_conso, r_vowel, r_particle_str, include_extra, right_kana);
            strcat(result.output, right_kana);
        }
    }
    
    // 「っ」の持ち越し設定または単体出力
    if (has_final_tsu) {
        // STNtkのような母音なしで「っ」単体の場合は「っ」を出力
        if (strlen(l_conso) > 0 && strlen(l_vowel) == 0 &&
            strcmp(l_conso, "STN") == 0 && strcmp(l_particle_str, "tk") == 0 &&
            strlen(r_conso) == 0 && strlen(r_vowel) == 0) {
            strcpy(result.output, "っ");
        } else {
            // それ以外は次回に持ち越し（今回の出力から「っ」を除去）
            pending_tsu = true;
            // 出力の末尾の「っ」を削除
            char *tsu_pos = strstr(result.output, "っ");
            if (tsu_pos != NULL && tsu_pos[3] == '\0') {  // 末尾の「っ」のみ
                *tsu_pos = '\0';
            }
        }
    }
    
    // ntk-nの特殊ケース: 右の助詞「n」を「ん」として追加
    if (is_ntk_n) {
        strcat(result.output, "ん");
    }


    // ひらがな出力をローマ字に変換しつつ、ひらがな文字数を保持
    // 右だけの入力の場合は変換失敗として扱う（ただし右側の助詞単体は除く）
    bool is_right_only = (strlen(l_conso) == 0 && strlen(l_vowel) == 0 &&
                          strlen(l_particle_str) == 0 &&
                          (strlen(r_conso) > 0 || strlen(r_vowel) > 0));
    
    // 持ち越し状態で出力が空の場合は成功として扱わない
    if (strlen(result.output) > 0 && !is_right_only) {
        char kana_output[128];
        strcpy(kana_output, result.output);
        result.kana_length = utf8_char_count(kana_output);
        kana_to_roma_zmk(kana_output, result.output, sizeof(result.output));
        result.success = true;
    } else if (pending_tsu) {
        // 持ち越し中は空出力だが成功扱い
        result.kana_length = 0;
        result.success = true;
    } else {
        result.kana_length = 0;
        result.success = false;
    }

    return result;
}



static void send_mejiro_output(const char *mejiro_id) {
    if (!mejiro_id || mejiro_id[0] == '\0') {
        return;
    }

    /* Sanity check: if the stroke contains unexpected chars/length, do nothing. */
    if (!mejiro_id_is_sane(mejiro_id)) {
        return;
    }

        /* SAFE MODE
     *  0: normal
     *  1: output a fixed ASCII marker ("a") via the same key-tap path
     *     (confirms the behavior triggers + output path works, without transform)
     *  2: do nothing except store last_output (isolates output path completely; no transform)
     *  3: run transform, but DO NOT output anything (isolates transform path)
     *  4: run transform, but output only the first character (limits output pressure)
     */
#if MEJIRO_SAFE_MODE == 1
    send_ascii_letters_only("a");
    strncpy(g_mejiro_last_output, "a", sizeof(g_mejiro_last_output) - 1);
    g_mejiro_last_output[sizeof(g_mejiro_last_output) - 1] = ' ';
    mejiro_history_push(1);
    return;
#elif MEJIRO_SAFE_MODE == 2
    strncpy(g_mejiro_last_output, mejiro_id, sizeof(g_mejiro_last_output) - 1);
    g_mejiro_last_output[sizeof(g_mejiro_last_output) - 1] = ' ';
    return;
#endif

    /* Commands have priority. In QMK, command-table patterns that include '#' execute only once. */
    if (handle_mejiro_command(mejiro_id)) {
        return;
    }

    const bool has_hash = (strchr(mejiro_id, '#') != NULL);

    char id_no_hash[MEJIRO_MAX_ID_LEN + 1];
    const char *id_for_translate = mejiro_id;

    if (has_hash) {
        /* If the input contained '#', but it wasn't a command-table exact match,
         * we remove '#' and translate the rest, then we will output twice.
         */
        size_t w = 0;
        for (size_t r = 0; mejiro_id[r] != '\0' && w < (sizeof(id_no_hash) - 1); r++) {
            if (mejiro_id[r] != '#') {
                id_no_hash[w++] = mejiro_id[r];
            }
        }
        id_no_hash[w] = '\0';
        id_for_translate = id_no_hash;

        /* Re-check after stripping. */
        if (!mejiro_id_is_sane(id_for_translate)) {
            return;
        }
    }

    const mejiro_result_t_zmk r = mejiro_transform_zmk(id_for_translate);

    if (!r.success || r.output == NULL || r.output[0] == '\0') {
        /* no output (we keep last output as-is) */
        return;
    }

#if MEJIRO_SAFE_MODE == 3
    // Transform ran, but we intentionally do not output.
    strncpy(g_mejiro_last_output, r.output, sizeof(g_mejiro_last_output) - 1);
    g_mejiro_last_output[sizeof(g_mejiro_last_output) - 1] = '\0';
    return;
#elif MEJIRO_SAFE_MODE == 4
    // Transform ran, but output only the first character.
    char one[2] = { r.output[0], '\0' };
    send_string(one);
    g_mejiro_last_output[0] = r.output[0];
    g_mejiro_last_output[1] = '\0';
    mejiro_history_push(1);
    // If '#' was present, emulate "double output" by sending the first char twice.
    if (has_hash) {
        send_string(one);
    }
    return;
#endif

    /* NOTE: send_string() is currently a stub in this file.
     * If you later implement send_string() to actually emit key events,
     * keep the output length bounded to avoid flooding the event queue.
     */

    if (!has_hash) {
        send_string(r.output);
        strncpy(g_mejiro_last_output, r.output, sizeof(g_mejiro_last_output) - 1);
        g_mejiro_last_output[sizeof(g_mejiro_last_output) - 1] = '\0';
        mejiro_history_push((uint16_t)strlen(r.output));
        return;
    }

    /* Double-output behavior when '#' was present (and not handled as a command-table exact match). */
    send_string(r.output);
    send_string(r.output);

    strncpy(g_mejiro_last_output, r.output, sizeof(g_mejiro_last_output) - 1);
    g_mejiro_last_output[sizeof(g_mejiro_last_output) - 1] = '\0';
    mejiro_history_push((uint16_t)(strlen(r.output) * 2));
}
