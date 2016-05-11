#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "list.h"
#include "myAlloc.h"

//#define THREAD_SAFE
#define TEST_LIST_SORT
#if defined(NWDEBUG) && defined(TEST_LIST_SORT)
#define list_debug(str...) printf(str)
#else
#define list_debug(str...)
#endif

#define ALIGN_8BYTES(x)     (((int)(x) + 7) & ~7)
#define POOLSIZE_TO_EFFTSIZE(x)     ((x) - sizeof(size_t))
#define EFFTSIZE_TO_POOLSIZE(x)     ((x) + sizeof(size_t))

typedef struct _free_slot_t free_slot_t;
typedef struct _free_pool_t free_pool_t;

struct _free_slot_t
{
    list_t          list;
    uint8_t         unitMem[0];
};

struct _free_pool_t
{
    size_t          size;
    list_t          list;
    uint8_t         unitMem[0];
};

int init_slot_allocator(slot_alloc_t *allocator)
{
    uint8_t *mem;
    uint32_t i;
    size_t nmemb;
    size_t size;

    free_slot_t *ptr = NULL;

    if (NULL == allocator || allocator->slotSize <= 0 ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
        return -1;

    size = allocator->slotSize;

    // align
	size = (size + 7) & ~7;
    if (size < sizeof(free_slot_t))
        size = sizeof(free_slot_t);

    nmemb = allocator->chunkSize / size;

    mem = (uint8_t *)allocator->chunkMem;

    INIT_LIST_HEAD(&allocator->freeSlots);

    for (i=0; i<nmemb; i++, mem+=size)
    {
        ptr = (free_slot_t *)mem;
        list_add_tail(&(ptr->list), &(allocator->freeSlots));
    }

    // update slot size
    allocator->slotSize = size;

    return 0;
}

int uninit_slot_allocator(slot_alloc_t *allocator)
{
    if (NULL == allocator || allocator->slotSize <= 0 ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
        return -1;

    return 0;
}

int init_pool_allocator(pool_alloc_t *allocator)
{
    int ret = -1;
    uint8_t *memStart;
    uint8_t *chunkEnd;

    free_pool_t *node = NULL;

    if (NULL == allocator ||
        NULL == allocator->chunkMem || allocator->chunkSize <= EFFTSIZE_TO_POOLSIZE(0))
        return ret;

    do
    {
        // FIXME: while applying to misaliagned memory page, chunkSize is not correct
        memStart = (uint8_t *)ALIGN_8BYTES(allocator->chunkMem);
        chunkEnd = (uint8_t *)allocator->chunkMem + allocator->chunkSize;

        if (memStart >= chunkEnd)
            break;

        INIT_LIST_HEAD(&allocator->freeSlots);
        node = (free_pool_t *)memStart;

        node->size = POOLSIZE_TO_EFFTSIZE(allocator->chunkSize); // for size information
        list_add_tail(&(node->list), &(allocator->freeSlots));

        ret = 0;
    }while(0);

    return ret;
}

int uninit_pool_allocator(pool_alloc_t *allocator)
{
    if (NULL == allocator ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
        return -1;

    return 0;
}

void * slot_malloc(slot_alloc_t *allocator, size_t size)
{
    free_slot_t *slot;

    if (NULL == allocator || allocator->slotSize <= 0 ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
        return NULL;

    if (size <=0 || size > allocator->slotSize)
        return NULL;

    if (list_empty(&(allocator->freeSlots)))
    {
        list_debug("no free slot\n");
        return NULL;
    }

    slot = list_entry(allocator->freeSlots.next, free_slot_t, list);

    list_del(&(slot->list));

    return (void *)slot;
}

void * pool_malloc(pool_alloc_t *allocator, size_t size)
{
    free_pool_t *node;
    free_pool_t *cur;
    uint8_t *mem = NULL;

    if (NULL == allocator ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
        return NULL;

    // align
    if (size <= 0)
        return NULL;

    // Then, adjust the delay queue for any entries whose time is up:
    list_for_each_entry(node, &(allocator->freeSlots), list)
        if (node->size >= size)
            break;

    if (node->size >= size)
    {
        list_del(&(node->list));
        mem = (uint8_t *)node + sizeof(size_t);
        if (node->size > ALIGN_8BYTES(size) + sizeof(free_pool_t) )
        {   // split
            cur = (free_pool_t *)(mem + ALIGN_8BYTES(size));

            cur->size = POOLSIZE_TO_EFFTSIZE(node->size - ALIGN_8BYTES(size)); // for size information
            node->size = ALIGN_8BYTES(size);

//            list_debug("split block %p(%d)\n", cur, cur->size);

            list_for_each_entry(node, &(allocator->freeSlots), list)
                if (node->size >= cur->size)
                    break;

            list_add(&(cur->list), &(node->list));
        }
    }

    return (void *)mem;
}

void slot_free(slot_alloc_t *allocator, void *mem)
{
    free_slot_t *slot;
    uint8_t *chunkEnd;

    if (NULL == allocator || allocator->slotSize <= 0 ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
        return;

    if (NULL == mem)
        return;

    chunkEnd = (uint8_t *)allocator->chunkMem + allocator->chunkSize;
    if ((uint8_t *)mem >= chunkEnd || mem < allocator->chunkMem)
    {
        list_debug("%p memory not allocated by this allocator\n", mem);
        return;
    }

    slot = (free_slot_t *)mem;

    list_add_tail(&(slot->list), &(allocator->freeSlots));

    return;
}

void pool_free(pool_alloc_t *allocator, void *mem)
{
    free_pool_t *node;
    free_pool_t *cur;
    uint8_t *chunkEnd;


    if (NULL == allocator ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
        return;

    if (NULL == mem)
        return;

    chunkEnd = (uint8_t *)allocator->chunkMem + allocator->chunkSize;
    if ((uint8_t *)mem >= chunkEnd || mem < allocator->chunkMem)
    {
        list_debug("%p memory not allocated by this allocator\n", mem);
        return;
    }

    cur = list_entry(mem, free_pool_t, list);
    if (cur->size > allocator->chunkSize)
    {
        list_debug("memory leak!\n");
        return;
    }

    // first do combine
    while(1)
    {
        int defragFlag = 0;
        list_for_each_entry(node, &(allocator->freeSlots), list)
        {
            if ((uint8_t *)node + EFFTSIZE_TO_POOLSIZE(node->size) == (uint8_t *)cur)
            {
                node->size += EFFTSIZE_TO_POOLSIZE(cur->size);
                cur = node;

                defragFlag = 1;
                break;
            }
            else if ((uint8_t *)cur + EFFTSIZE_TO_POOLSIZE(cur->size) == (uint8_t *)node)
            {
                cur->size += EFFTSIZE_TO_POOLSIZE(node->size);

                defragFlag = 1;
                break;
            }
        }

        if (defragFlag)
        {
            list_del(&(node->list));
            list_debug("cur->size grow to %d\n", cur->size);
        }
        else
            break;
    }

    // add to proper postion in free list
    list_for_each_entry(node, &(allocator->freeSlots), list)
        if (node->size >= cur->size)
            break;

    list_add(&(cur->list), &(node->list));

    return;
}

char * pool_strdup(pool_alloc_t *allocator, const char *s)
{
    size_t len = strlen(s);
    char *mem = NULL;

    if (s)
    {
        if (len >= 0)
            mem = pool_malloc(allocator, len + 1);

        if (mem)
            snprintf(mem, len + 1, "%s", s);
    }

    return mem;
}

#ifdef TEST_LIST_SORT
void slot_alloc_dump(slot_alloc_t *allocator)
{
    int i;
    free_slot_t* cur;

    if (NULL == allocator || allocator->slotSize <= 0 ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
    {
        list_debug("invalid allocator\n");
        return;
    }

    list_debug("dump %p\n", allocator);

    i = 0;
    list_for_each_entry(cur, &(allocator->freeSlots), list)
        list_debug("%p --> unit %d(%d bytes)\n", cur, ++i, allocator->slotSize);

    list_debug("done\n");

    return;
}

void pool_alloc_dump(pool_alloc_t *allocator)
{
    int i;
    size_t totalSize;
    free_pool_t* cur;

    if (NULL == allocator ||
        NULL == allocator->chunkMem || allocator->chunkSize <= 0)
    {
        list_debug("invalid allocator\n");
        return;
    }

    list_debug("dump %p\n", allocator);

    i = 0;
    totalSize = 0;
    list_for_each_entry(cur, &(allocator->freeSlots), list)
    {
        list_debug("%p --> unit %d(%d bytes)\n", cur, ++i, cur->size);
        totalSize += EFFTSIZE_TO_POOLSIZE(cur->size);
    }

    if (totalSize != allocator->chunkSize)
        list_debug("allocator size %d/%d\n", totalSize, allocator->chunkSize);

    list_debug("done\n");

    return;
}
#endif

#if 0

#define CONST_TEST_MEMORY   2560

int main()
{
    int i;
    int ret;
    void *mem[2];
    pool_alloc_t myallocator;

    void *chunkMem = malloc(CONST_TEST_MEMORY);
    memset(chunkMem, 0, CONST_TEST_MEMORY);

    if (!chunkMem)
    {
        printf("can't allocate memory!\n");
        exit(-1);
    }

    myallocator.chunkMem = chunkMem;
    myallocator.chunkSize = CONST_TEST_MEMORY;
//    myallocator.slotSize = 123;

    ret = init_pool_allocator(&myallocator);

    if (0 == ret)
        pool_alloc_dump(&myallocator);
/*
    for (i=0; i<5; i++)
    {
        mem = pool_malloc(&myallocator, 123);

        memset(mem[0], 0, 123);
        pool_alloc_dump(&myallocator);
        pool_free(&myallocator, mem[0]);
        pool_alloc_dump(&myallocator);
    }
*/
    mem[0] = pool_strdup(&myallocator, "this is a hello world test\na new line test\n");

    mem[1] = pool_strdup(&myallocator, "this one \0 you can't see it\n");

    printf(mem[0]);
    printf(mem[1]);

    pool_free(&myallocator, mem[0]);
    pool_alloc_dump(&myallocator);

    pool_free(&myallocator, mem[1]);
    pool_alloc_dump(&myallocator);

    /*
    slot_free(&myallocator, mem);
    slot_alloc_dump(&myallocator);

    mem = 0x12345678;
    slot_free(&myallocator, mem);

    slot_alloc_dump(&myallocator);

    uninit_slot_allocator(&myallocator);
*/
    uninit_pool_allocator(&myallocator);
    free(chunkMem);

    return 0;

}
#endif


