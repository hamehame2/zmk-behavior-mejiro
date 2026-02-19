#include <zephyr/kernel.h>
#include <zmk_naginata/nglistarray.h>

void initListArray(NGListArray *array) {
    array->size = 0;
}

bool addToListArray(NGListArray *array, NGList list) {
    if (array->size >= MAX_LIST_ARRAY_SIZE) {
        return false;
    }

    array->lists[array->size] = list;
    array->size++;
    return true;
}

bool addToListArrayAt(NGListArray *array, NGList list, int idx) {
    if (idx < 0 || idx > array->size) {
        return false;
    }
    if (array->size >= MAX_LIST_ARRAY_SIZE) {
        return false;
    }

    /* shift right from tail to idx (backward) */
    for (int i = array->size; i > idx; i--) {
        array->lists[i] = array->lists[i - 1];
    }

    array->lists[idx] = list;
    array->size++;
    return true;
}

NGList getFromListArray(NGListArray *array, int index) {
    if (index < 0 || index >= array->size) {
        NGList empty;
        initList(&empty);
        return empty;
    }

    return array->lists[index];
}

bool removeFromListArray(NGListArray *array, NGList list) {
    for (int i = 0; i < array->size; i++) {
        /* NOTE: This assumes NGList is comparable by size+elements.
                 If NGList contains padding/garbage, you may need a custom equal(). */
        if (array->lists[i].size == list.size) {
            bool same = true;
            for (int j = 0; j < list.size; j++) {
                if (array->lists[i].elements[j] != list.elements[j]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                for (int k = i; k < array->size - 1; k++) {
                    array->lists[k] = array->lists[k + 1];
                }
                array->size--;
                return true;
            }
        }
    }
    return false;
}

bool removeFromListArrayAt(NGListArray *array, int idx) {
    if (idx < 0 || idx >= array->size) {
        return false;
    }

    for (int i = idx; i < array->size - 1; i++) {
        array->lists[i] = array->lists[i + 1];
    }

    array->size--;
    return true;
}
