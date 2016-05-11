#ifndef NW_SLOT_ALLOC_H
#define NW_SLOT_ALLOC_H

#include <stdio.h>

#include "list.h"

// CAUTION: DO NOT modify any item in
//    'private' section of this structure
typedef struct _slot_alloc_t slot_alloc_t;
typedef struct _pool_alloc_t pool_alloc_t;

struct _slot_alloc_t
{
    size_t      slotSize;
    list_t      freeSlots;

    void        *chunkMem;
    size_t      chunkSize;
};

struct _pool_alloc_t
{
    size_t      reserved;
    list_t      freeSlots;

    void        *chunkMem;
    size_t      chunkSize;
};

#ifdef __cplusplus
extern "C"
{
#endif

int init_slot_allocator(slot_alloc_t *allocator);
int uninit_slot_allocator(slot_alloc_t *allocator);

void * slot_malloc(slot_alloc_t *allocator, size_t size);
void slot_free(slot_alloc_t *allocator, void *mem);

int init_pool_allocator(pool_alloc_t *allocator);
int uninit_pool_allocator(pool_alloc_t *allocator);

void * pool_malloc(pool_alloc_t *allocator, size_t size);
char * pool_strdup(pool_alloc_t *allocator, const char *s);

void pool_free(pool_alloc_t *allocator, void *mem);

void slot_alloc_dump(slot_alloc_t *allocator);
void pool_alloc_dump(pool_alloc_t *allocator);
#ifdef __cplusplus
};
#endif

#endif
