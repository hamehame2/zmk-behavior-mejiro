#pragma once
#include <zephyr/kernel.h>
#include <zmk_naginata/nglist.h>

/* Maximum number of buffered chord-lists (input history) */
#ifndef MAX_LIST_ARRAY_SIZE
#define MAX_LIST_ARRAY_SIZE 16
#endif

typedef struct {
    NGList elements[MAX_LIST_ARRAY_SIZE];
    int size;
} NGListArray;

/* init/compat */
void initListArray(NGListArray *array);
static inline void initializeListArray(NGListArray *array) { initListArray(array); }

/* operations (pointer API: behavior passes &a etc.) */
bool addToListArray(NGListArray *array, NGList *list);
bool addToListArrayAt(NGListArray *array, NGList *list, int idx);
int includeListArray(NGListArray *array, NGList *list);
bool removeFromListArrayAt(NGListArray *array, int idx);

/* optional helper */
NGList getFromListArray(NGListArray *array, int index);
