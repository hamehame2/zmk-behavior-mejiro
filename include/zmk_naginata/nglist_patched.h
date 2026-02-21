#pragma once
#include <zephyr/kernel.h>

/*
 * NGList / NGListArray
 *
 * IMPORTANT:
 *  - These headers are made CONSISTENT with the implementation (.c) and the
 *    behavior_naginata_fixed34/35 usage patterns:
 *      - NGListArray has member name `elements` (NOT `lists`)
 *      - addToListArray() takes NGList* (NOT value) because behavior passes &a
 *      - Provide both initList()/initializeList() for backward compatibility
 */

/* Maximum number of keys in one chord/list */
#define LIST_SIZE 5
#define MAX_LIST_SIZE LIST_SIZE

typedef struct {
    uint32_t elements[LIST_SIZE];
    int size;
} NGList;

/* init/compat */
void initList(NGList *list);
static inline void initializeList(NGList *list) { initList(list); }

/* operations */
bool addToList(NGList *list, uint32_t element);
bool addToListAt(NGList *list, uint32_t element, int idx);
int includeList(NGList *list, uint32_t element);
bool removeFromList(NGList *list, uint32_t element);
bool removeFromListAt(NGList *list, int idx);
void copyList(NGList *src, NGList *dst);

/* small helpers used by behavior */
bool compareList0(NGList *list, uint32_t a0);
bool compareList01(NGList *list, uint32_t a0, uint32_t a1);
