#ifndef ARRAY_H
#define ARRAY_H

#include <stdint.h>
#include <stddef.h>

typedef struct array {
    size_t size;
    uint32_t *data;
} array_t;

int array_init(array_t *arr, size_t n);

void array_deinit(array_t *arr);

size_t array_size(array_t *arr);

uint32_t array_get(array_t *arr, size_t i);

void array_set(array_t *arr, size_t i, uint32_t v);

uint32_t* array_get_ref(array_t *arr, size_t i);

#endif
