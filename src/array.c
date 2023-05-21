#include "array.h"

#include <assert.h>
#include <stdlib.h>

int array_init(array_t *arr, size_t n) {
    arr->data = (uint32_t*) calloc(n, sizeof(uint32_t));
    if (arr->data == NULL) return -1;

    arr->size = n;
    return 0;
}

void array_deinit(array_t *arr) {
    free(arr->data);
    arr->data = 0;
    arr->size = 0;
}

size_t array_size(array_t *arr) {
    return arr->size;
}

uint32_t array_get(array_t *arr, size_t i) {
    assert(i < arr->size);
    return arr->data[i];
}

uint32_t* array_get_ref(array_t *arr, size_t i) {
    assert(i < arr->size);
    return &arr->data[i];
}

void array_set(array_t *arr, size_t i, uint32_t v) {
    assert(i < arr->size);
    arr->data[i] = v;
}
