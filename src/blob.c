#define _GNU_SOURCE
#include "blob.h"

#include "bitset.h"
#include "array.h"
#include "util.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/ioctl.h>
#include <linux/fs.h>

#define PAGE_SIZE 4096

#define ceil_div_ul(a, b) ((a - 1) / b + 1)

typedef struct superblob_page {
    uint32_t magic;
    uint32_t page_shift;
    uint32_t cluster_shift;
    uint32_t clusters;
    uint32_t md_shift;
    uint32_t next;
    uint8_t res20[4072];
} __attribute__((aligned(PAGE_SIZE))) superblob_page_t;

static_assert(sizeof(superblob_page_t) == PAGE_SIZE);

typedef struct blob_page {
    uint32_t next;
    uint8_t uuid[16];
    uint32_t n_clusters;
    uint32_t clusters;
    uint8_t res28[4068];
} __attribute__((aligned(PAGE_SIZE))) blob_page_t;

static_assert(sizeof(blob_page_t) == PAGE_SIZE);

typedef struct cluster_page {
    uint32_t next;
    uint32_t clusters[512];
    uint8_t res2052[2044];
} __attribute__((aligned(PAGE_SIZE))) cluster_page_t;

static_assert(sizeof(cluster_page_t) == PAGE_SIZE);

int page_write(int fd, void *page, uint32_t index) {
    int n_written = pwrite(fd, page, PAGE_SIZE, (uint64_t) index * PAGE_SIZE);
    if (n_written < 0) {
        return n_written;
    }

    return 0;
}

int page_read(int fd, void *page, uint32_t index) {
    int n_read = pread(fd, page, PAGE_SIZE, (uint64_t) index * PAGE_SIZE);
    if (n_read < 0) {
        return n_read;
    }

    return 0;
}

int blobstore_write_blob_page(blobstore_t *bs, blob_t *blob, blob_t *next) {
    blob_page_t blob_page = {0};
    blob_page.next = next ? next->page_index: 0;
    blob_page.n_clusters = array_size(&blob->clusters);
    blob_page.clusters = array_get(&blob->cluster_page_indices, 0);
    memcpy(blob_page.uuid, blob->uuid, 16);
    
    if (page_write(bs->fd, &blob_page, blob->page_index) < 0) {
        return -1;
    }

    return 0;
}

int blobstore_write_superblob_page(blobstore_t *bs, blob_t *head) {
    superblob_page_t superblob_page = {0};
    superblob_page.magic = 0x12345678;
    superblob_page.page_shift = bs->page_shift;
    superblob_page.cluster_shift = bs->cluster_shift;
    superblob_page.md_shift = bs->md_shift;
    superblob_page.next = 0;
    superblob_page.clusters = bitset_capacity(&bs->clusters);
    if (head) {
        superblob_page.next = head->page_index;
    }

    return page_write(bs->fd, &superblob_page, 0);
}

size_t llog2(size_t x) {
    size_t res = 0;
    while (x >>= 1) ++res;
    return res;
}

int blobstore_init(blobstore_t *bs, int fd) {
    bs->fd = fd;

    uint64_t size;
    if (ioctl(fd, BLKGETSIZE64, &size) < 0) {
        perror("failed to get block device size");
        exit(1);
    }

    size_t logical_block_size;
    if (ioctl(fd, BLKBSZGET, &logical_block_size) < 0) {
        perror("failed to get block device logical block size");
        exit(1);
    }

    bs->page_shift = llog2(logical_block_size);
    bs->cluster_shift = 8;
    bs->md_shift = 0;
    bs->head = NULL;

    size_t n_md_pages = 1UL << bs->cluster_shift << bs->md_shift;
    if (bitset_init(&bs->md_pages, n_md_pages) < 0) return -1;
    bitset_set(&bs->md_pages, 0, 1);

    size_t n_clusters = size >> bs->page_shift >> bs->cluster_shift;
    if (bitset_init(&bs->clusters, n_clusters) < 0) return -1;
    for (size_t i = 0; i < (1UL << bs->md_shift); i++) {
        bitset_set(&bs->clusters, i, 1);
    }

    if (blobstore_write_superblob_page(bs, NULL) < 0) {
        return -1;
    }

    return 0;
}

void blob_deinit(blob_t *blob) {
    array_deinit(&blob->clusters);
    array_deinit(&blob->cluster_page_indices);
}

int clusters_read(int fd, blob_t *blob, uint32_t i, uint32_t page_index) {
    cluster_page_t cluster_page;
    if (page_read(fd, &cluster_page, page_index) < 0) {
        return -1;
    }

    array_set(&blob->cluster_page_indices, i, page_index);
    
    size_t n_clusters = array_size(&blob->clusters);
    size_t n = 512 * (i + 1) > n_clusters ? n_clusters % 512: 512;
    uint32_t *ref = array_get_ref(&blob->clusters, 512 * i);
    memcpy(ref, cluster_page.clusters, n * sizeof(uint32_t));
    if (cluster_page.next) {
        if (i + 1 == array_size(&blob->cluster_page_indices)) return -1;
        return clusters_read(fd, blob, i + 1, cluster_page.next);
    }

    return 0;
}

int blob_read_one(int fd, uint32_t page_index, blob_t *blob, uint32_t *next) {
    blob_page_t blob_page;
    if (page_read(fd, &blob_page, page_index) < 0) {
        return -1;
    }

    if (blob_page.n_clusters == 0) return -1;

    blob->page_index = page_index;
    memcpy(blob->uuid, blob_page.uuid, 16);

    if (array_init(&blob->clusters, blob_page.n_clusters) < 0) {
        return -1;
    }

    size_t n_cluster_pages = ceil_div_ul(blob_page.n_clusters, 512);
    if (array_init(&blob->cluster_page_indices, n_cluster_pages) < 0) {
        goto error0;
    }

    if (clusters_read(fd, blob, 0, blob_page.clusters) < 0) {
        goto error1;
    }

    *next = blob_page.next;

    return 0;

error1:
    array_deinit(&blob->cluster_page_indices);
error0:
    array_deinit(&blob->clusters);
    return -1;
}

void blob_list_deinit(blob_t *head) {
    blob_t *iter = head;
    while (iter) {
        blob_t *next = iter->next;
        blob_deinit(iter);
        free(iter);
        iter = next;
    }
}

int blob_read(int fd, uint32_t next_page_index, blob_t **res) {
    blob_t *head = NULL;
    blob_t *prev = NULL;
    while (next_page_index) {
        blob_t *blob = (blob_t*) calloc(1, sizeof(blob_t));
        if (blob == NULL) goto error;
        if (head == NULL) head = blob;

        if (blob_read_one(fd, next_page_index, blob, &next_page_index) < 0) {
            free(blob);
            goto error;
        }

        blob->prev = prev;
        if (blob->prev) {
            blob->prev->next = blob;
        }
        prev = blob;
    }
    
    *res = head;
    return 0;

error:
    blob_list_deinit(head);
    return -1;
}

int blobstore_open(blobstore_t *bs, int fd) {

    uint64_t size;
    if (ioctl(fd, BLKGETSIZE64, &size) < 0) {
        perror("failed to get block device size");
        exit(1);
    }

    size_t logical_block_size;
    if (ioctl(fd, BLKBSZGET, &logical_block_size) < 0) {
        perror("failed to get block device logical block size");
        exit(1);
    }

    superblob_page_t sb;
    if (page_read(fd, &sb, 0) < 0) {
        return -1;
    }

    if (logical_block_size != 1ULL << sb.page_shift) return -1;
    uint64_t cluster_size_bytes = (1ULL << sb.page_shift << sb.cluster_shift);
    if (size < sb.clusters * cluster_size_bytes) return -1;

    bs->fd = fd;
    bs->page_shift = sb.page_shift;
    bs->cluster_shift = sb.cluster_shift;
    bs->md_shift = sb.md_shift;

    size_t n_md_pages = 1UL << bs->cluster_shift << bs->md_shift;
    if (bitset_init(&bs->md_pages, n_md_pages) < 0) return -1;
    bitset_set(&bs->md_pages, 0, 1);

    if (bitset_init(&bs->clusters, sb.clusters) < 0) return -1;
    for (size_t i = 0; i < (1UL << bs->md_shift); i++) {
        bitset_set(&bs->clusters, i, 1);
    }

    if (blob_read(bs->fd, sb.next, &bs->head) < 0) {
        return -1;
    }

    for (blob_t *iter = bs->head; iter; iter = iter->next) {
        bitset_set(&bs->md_pages, iter->page_index, 1);

        size_t n_cluster_pages = array_size(&iter->cluster_page_indices);
        for (size_t i = 0; i < n_cluster_pages; i++) {
            bitset_set(&bs->md_pages, array_get(&iter->cluster_page_indices, i), 1);
        }

        size_t n_clusters = array_size(&iter->clusters);
        for (size_t i = 0; i < n_clusters; i++) {
            uint32_t cluster_id = array_get(&iter->clusters, i);
            if (cluster_id != 0) {
                bitset_set(&bs->clusters, cluster_id, 1);
            }
        }
    }

    return 0;
}

/**
 * Release all resources associated with the blobstore `bs`.
 * 
 * \param bs the blobstore. 
 */
void blobstore_deinit(blobstore_t *bs) {
    bitset_deinit(&bs->clusters);
    bitset_deinit(&bs->md_pages);
    blob_list_deinit(bs->head);
}

/**
 * Create a blob of the given size in `n_clusters`. 
 * 
 * \param bs the blobstore.
 * \param n_clusters the size of the blob in clusters.
 * \return 0 if success else -1
 */
int blobstore_create_blob(blobstore_t *bs, uint32_t n_clusters) {
    uint32_t page_index;

    if (n_clusters == 0) return -1;
    if (bitset_alloc(&bs->md_pages, &page_index, 1) < 0) {
        return -1;
    }

    blob_t *blob = (blob_t*) calloc(1, sizeof(blob_t));
    if (blob == NULL) {
        goto error1;
    }

    blob->page_index = page_index;
    blob->prev = NULL;
    blob->next = bs->head;

    if (uuid_init_random(blob->uuid) < 0) {
        goto error2;
    }

    if (array_init(&blob->clusters, n_clusters) < 0) {
        goto error2;
    }
    size_t n_cluster_pages = ceil_div_ul(n_clusters, 512);
    if (array_init(&blob->cluster_page_indices, n_cluster_pages) < 0) {
        goto error3;
    }

    uint32_t *ref = array_get_ref(&blob->cluster_page_indices, 0);
    if (bitset_alloc(&bs->md_pages, ref, n_cluster_pages) < 0) {
        goto error4;
    }

    for (size_t i = 0; i < n_cluster_pages; i++) {
        uint32_t next = (i + 1) < n_cluster_pages ? array_get(&blob->cluster_page_indices, i + 1): 0;
        cluster_page_t cluster_page = {0};
        cluster_page.next = next;
        if (page_write(bs->fd, &cluster_page, array_get(&blob->cluster_page_indices, i)) < 0) {
            goto error5;
        }
    }

    if (blobstore_write_blob_page(bs, blob, blob->next) < 0) {
        goto error5;
    }

    if (blobstore_write_superblob_page(bs, blob) < 0) {
        goto error5;
    }

    if (bs->head) {
        bs->head->prev = blob;
    }
    bs->head = blob;

    return 0;

error5:
    bitset_free(&bs->md_pages, ref, n_cluster_pages);
error4:
    array_deinit(&blob->cluster_page_indices);
error3:
    array_deinit(&blob->clusters);
error2:
    free(blob);
error1:
    bitset_free(&bs->md_pages, &page_index, 1);
    return -1;
}

/**
 * Delete `blob` from the blobstore `bs`. This will allow all clusters and
 * metadata used by `blob` to be reused by another `blob`.
 * 
 * \param bs the blobstore
 * \param blob the blob
 */
int blobstore_delete_blob(blobstore_t *bs, blob_t *blob) {
    if (blob == NULL) return -1;

    if (blob->prev) {
        if (blobstore_write_blob_page(bs, blob->prev, blob->next) < 0) {
            return -1;
        }

        blob->prev->next = blob->next;
    } else if (bs->head == blob) {
        if (blobstore_write_superblob_page(bs, blob->next) < 0) {
            return -1;
        }

        bs->head = blob->next;
    } else {
        return -1;
    }

    if (blob->next) {
        blob->next->prev = blob->prev;
    }

    size_t n_cluster_pages = array_size(&blob->cluster_page_indices);
    for (size_t i = 0; i < n_cluster_pages; i++) {
        bitset_set(&bs->md_pages, array_get(&blob->cluster_page_indices, i), 0);
    }

    size_t n_clusters = array_size(&blob->clusters);
    for (size_t i = 0; i < n_clusters; i++) {
        uint32_t cluster_id = array_get(&blob->clusters, i);
        if (cluster_id != 0) {
            bitset_set(&bs->clusters, cluster_id, 0);
        }
    }

    blob_deinit(blob);
    free(blob);

    return 0;
}

/**
 * Initialize `set` to represent the cluster sparsity of `blob`.
 * 
 * \param blob the blob
 * \param set the bitset to initialize.
 * \return 0 if success else -1
 */
int blob_nonzero(blob_t *blob, bitset_t *set) {
    size_t n_clusters = array_size(&blob->clusters);
    if (bitset_init(set, n_clusters) < 0) return -1;
    for (size_t i = 0; i < n_clusters; i++) {
        if (array_get(&blob->clusters, i)) {
            bitset_set(set, i, 1);
        }
    }

    return 0;
}
