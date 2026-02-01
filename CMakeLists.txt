/*
 * Mejiro transformation logic
 */
#pragma once

/* ZMK module side: do NOT include QMK headers.
 * We only need basic C types here.
 */
#include <stdbool.h>
#include <stddef.h>

// メジロ式変換結果
typedef struct {
    char output[128];
    size_t kana_length; // 変換前ひらがなの文字数
    bool success;
} mejiro_result_t;

// メジロIDからひらがなへの変換
mejiro_result_t mejiro_transform(const char *mejiro_id);

// ひらがなをヘボン式ローマ字に変換
void kana_to_roma(const char *kana, char *out, size_t out_len);

// 「っ」の保留状態をクリア
void mejiro_clear_pending_tsu(void);
