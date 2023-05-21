#include "bitset.h"

#include <assert.h>
#include <stdlib.h>

int bitset_init(bitset_t *set, size_t capacity) {
    set->size = 0;
    set->words = calloc((capacity - 1) / 32 + 1, sizeof(uint32_t));
    if (set->words == NULL) return -1;
    set->capacity = capacity;
    return 0;
}

void bitset_deinit(bitset_t *set) {
    set->size = 0;
    free(set->words);
    set->words = NULL;
}

int bitset_get(bitset_t *set, size_t i) {
    if (i > set->capacity) return 0;
    size_t word = i >> 5;
    size_t bit = i - (word << 5);
    return (set->words[word] & (1UL << bit)) != 0;
}

void bitset_set(bitset_t *set, size_t i, int b) {
    if (i > set->capacity) return;
    size_t word = i >> 5;
    size_t bit = i - (word << 5);
    set->size += b - bitset_get(set, i);
    set->words[word] &= ~(1 << bit);
    set->words[word] |= ((uint32_t) b & 1) << bit;
}

size_t bitset_capacity(bitset_t *set) {
    return set->capacity;
}

size_t bitset_size(bitset_t *set) {
    return set->size;
}

int bitset_alloc(bitset_t *set, uint32_t *arr, size_t n) {
    size_t j = 0;
    for (size_t i = 0; i < bitset_capacity(set); i++) {
        if (j == n) break;
        if (bitset_get(set, i) == 0) {
            bitset_set(set, i, 1);
            arr[j++] = i;
        }
    }
    
    if (j == n) return 0;

    for (size_t i = 0; i < j; i++) {
        bitset_set(set, arr[i], 0);
    }

    return -1;
}

void bitset_free(bitset_t *set, uint32_t *arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        bitset_set(set, arr[i], 0);
    }
}
