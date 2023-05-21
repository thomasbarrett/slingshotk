#ifndef BITSET_H
#define BITSET_H

#include <stdint.h>
#include <stddef.h>

typedef struct bitset {
    size_t capacity;
    size_t size;
    uint32_t *words;
} bitset_t;

int bitset_init(bitset_t *set, size_t capacity);

void bitset_deinit(bitset_t *set);

int bitset_get(bitset_t *set, size_t i);

void bitset_set(bitset_t *set, size_t i, int b);

size_t bitset_capacity(bitset_t *set);

size_t bitset_size(bitset_t *set);

int bitset_alloc(bitset_t *set, uint32_t *arr, size_t n);

void bitset_free(bitset_t *set, uint32_t *arr, size_t n);

#endif
