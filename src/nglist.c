#include <zephyr/kernel.h>
#include <zmk_naginata/nglist.h>

void initList(NGList *list) {
    list->size = 0;
}

bool addToList(NGList *list, uint32_t element) {
    if (list->size >= MAX_LIST_SIZE) {
        return false;
    }

    list->elements[list->size] = element;
    list->size++;
    return true;
}

bool addToListAt(NGList *list, uint32_t element, int idx) {
    if (idx < 0 || idx > list->size) {
        return false;
    }
    if (list->size >= MAX_LIST_SIZE) {
        return false;
    }

    /* shift right from tail to idx (backward) */
    for (int i = list->size; i > idx; i--) {
        list->elements[i] = list->elements[i - 1];
    }

    list->elements[idx] = element;
    list->size++;
    return true;
}

uint32_t getFromList(NGList *list, int index) {
    if (index < 0 || index >= list->size) {
        return UINT32_MAX;
    }
    return list->elements[index];
}

int getIndexFromList(NGList *list, uint32_t element) {
    for (int i = 0; i < list->size; i++) {
        if (list->elements[i] == element) {
            return i;
        }
    }
    return -1;
}

bool isInList(NGList *list, uint32_t element) {
    return getIndexFromList(list, element) != -1;
}

bool removeFromList(NGList *list, uint32_t element) {
    int index = getIndexFromList(list, element);
    if (index == -1) {
        return false;
    }

    for (int i = index; i < list->size - 1; i++) {
        list->elements[i] = list->elements[i + 1];
    }

    list->size--;
    return true;
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
