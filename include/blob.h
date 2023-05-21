#ifndef BLOB_H
#define BLOB_H

#include "array.h"
#include "bitset.h"

#include <stdint.h>

typedef struct blob {
    struct blob *next;
    struct blob *prev;
    uint32_t page_index;
    uint8_t uuid[16];
    array_t cluster_page_indices;
    array_t clusters;
} blob_t;

typedef struct blobstore {
    int fd;
    uint32_t page_shift;
    uint32_t cluster_shift;
    uint32_t md_shift;
    blob_t *head;
    bitset_t md_pages;
    bitset_t clusters;
} blobstore_t;

int blobstore_create_blob(blobstore_t *bs, uint32_t n_clusters);

int blobstore_init(blobstore_t *bs, int fd);

void blobstore_deinit(blobstore_t *bs);

int blobstore_open(blobstore_t *bs, int fd);

int blob_nonzero(blob_t *blob, bitset_t *set);

int blobstore_delete_blob(blobstore_t *bs, blob_t *blob);

#endif