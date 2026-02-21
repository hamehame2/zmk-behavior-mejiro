#include <zephyr/kernel.h>
#include <zmk_naginata/nglistarray.h>

void initListArray(NGListArray *array) {
    array->size = 0;
}

static bool nglist_equal(NGList *a, NGList *b) {
    if (a->size != b->size) {
        return false;
    }
    for (int i = 0; i < a->size; i++) {
        if (a->elements[i] != b->elements[i]) {
            return false;
        }
    }
    return true;
}

bool addToListArray(NGListArray *array, NGList *list) {
    if (array->size >= MAX_LIST_ARRAY_SIZE) {
        return false;
    }
    array->elements[array->size++] = *list;
    return true;
}

bool addToListArrayAt(NGListArray *array, NGList *list, int idx) {
    if (idx < 0 || idx > array->size) {
        return false;
    }
    if (array->size >= MAX_LIST_ARRAY_SIZE) {
        return false;
    }
    for (int i = array->size; i > idx; i--) {
        array->elements[i] = array->elements[i - 1];
    }
    array->elements[idx] = *list;
    array->size++;
    return true;
}

NGList getFromListArray(NGListArray *array, int index) {
    if (index < 0 || index >= array->size) {
        NGList empty;
        initList(&empty);
        return empty;
    }
    return array->elements[index];
}

int includeListArray(NGListArray *array, NGList *list) {
    for (int i = 0; i < array->size; i++) {
        if (nglist_equal(&array->elements[i], list)) {
            return i;
        }
    }
    return -1;
}

bool removeFromListArrayAt(NGListArray *array, int idx) {
    if (idx < 0 || idx >= array->size) {
        return false;
    }
    for (int i = idx; i < array->size - 1; i++) {
        array->elements[i] = array->elements[i + 1];
    }
    array->size--;
    return true;
}
