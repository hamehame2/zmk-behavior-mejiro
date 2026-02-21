#include <zephyr/kernel.h>
#include <zmk_naginata/nglist.h>

void initList(NGList *list) {
    list->size = 0;
}

bool addToList(NGList *list, uint32_t element) {
    if (list->size >= MAX_LIST_SIZE) {
        return false;
    }
    list->elements[list->size++] = element;
    return true;
}

bool addToListAt(NGList *list, uint32_t element, int idx) {
    if (idx < 0 || idx > list->size) {
        return false;
    }
    if (list->size >= MAX_LIST_SIZE) {
        return false;
    }
    for (int i = list->size; i > idx; i--) {
        list->elements[i] = list->elements[i - 1];
    }
    list->elements[idx] = element;
    list->size++;
    return true;
}

int includeList(NGList *list, uint32_t element) {
    for (int i = 0; i < list->size; i++) {
        if (list->elements[i] == element) {
            return i;
        }
    }
    return -1;
}

bool removeFromList(NGList *list, uint32_t element) {
    int idx = includeList(list, element);
    if (idx < 0) {
        return false;
    }
    return removeFromListAt(list, idx);
}

bool removeFromListAt(NGList *list, int idx) {
    if (idx < 0 || idx >= list->size) {
        return false;
    }
    for (int i = idx; i < list->size - 1; i++) {
        list->elements[i] = list->elements[i + 1];
    }
    list->size--;
    return true;
}

void copyList(NGList *src, NGList *dst) {
    dst->size = src->size;
    for (int i = 0; i < src->size && i < MAX_LIST_SIZE; i++) {
        dst->elements[i] = src->elements[i];
    }
}

/* helpers */
bool compareList0(NGList *list, uint32_t a0) {
    return (list->size == 1 && list->elements[0] == a0);
}
bool compareList01(NGList *list, uint32_t a0, uint32_t a1) {
    return (list->size == 2 && list->elements[0] == a0 && list->elements[1] == a1);
}
