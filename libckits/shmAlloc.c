#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

#include "shmAlloc.h"

//#define THREAD_SAFE
#ifdef TEST_LIST_SORT
//extern int shell_print(const char* format, ...);
#define list_debug(str...) printf(str)
#else
#define list_debug(str...)
#endif

#define __INLINE__ inline
#define __PREFETCH__(x) 1

#define ALIGN_4BYTES(x)             (((int)(x) + 3) & ~3)
#define POOLSIZE_TO_EFFTSIZE(x)     ((x) - sizeof(uint32_t))
#define EFFTSIZE_TO_POOLSIZE(x)     ((x) + sizeof(uint32_t))

#define OFFSET_POINTER(x, base)     (uint32_t)((uint8_t *)(x) - (uint8_t *)(base))
#define OFFSET_NODE(base, off)      (shm_list_t *)((uint8_t *)(base) + (uint32_t)(off))

#define CONST_MAGIC_FLAG            (0xAA55AA55)

typedef struct _shm_alloc_t shm_alloc_t;
typedef struct _shm_list_head shm_list_t;
typedef struct _free_shm_pool_t free_shm_pool_t;

struct _shm_list_head
{
    uint32_t next;
    uint32_t prev;
};

struct _shm_alloc_t
{
    shm_list_t  freeParts;
    uint32_t    candiate;

    uint32_t    magicFlag;
    uint32_t    chunkSize;
};

struct _free_shm_pool_t
{
    size_t      size;       // 低2位用于指示引用数，分配的时候按照四字节对齐
    shm_list_t  list;
    uint8_t     unitMem[0];
};

//////// share memory list part ///////

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static void __INLINE__ __shm_list_add(uint8_t *memStart, shm_list_t *new, shm_list_t *prev, shm_list_t *next)
{
    next->prev = OFFSET_POINTER(new, memStart);
    new->next = OFFSET_POINTER(next, memStart);
    new->prev = OFFSET_POINTER(prev, memStart);
    prev->next = OFFSET_POINTER(new, memStart);
}


/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static __INLINE__ void shm_list_add(uint8_t *memStart, shm_list_t *new, shm_list_t *head)
{
    __shm_list_add(memStart, new, head, OFFSET_NODE(memStart, head->next));
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static __INLINE__ void shm_list_add_tail(uint8_t *memStart, shm_list_t *new, shm_list_t *head)
{
    __shm_list_add(memStart, new, OFFSET_NODE(memStart, head->prev), head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static __INLINE__ void __shm_list_del(uint8_t *memStart, shm_list_t *prev, shm_list_t *next)
{
    next->prev = OFFSET_POINTER(prev, memStart);
    prev->next = OFFSET_POINTER(next, memStart);
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is in an undefined state.
 */
static __INLINE__ void shm_list_del(uint8_t *memStart, shm_list_t *entry)
{
    __shm_list_del(memStart, OFFSET_NODE(memStart, entry->prev), OFFSET_NODE(memStart, entry->next));
    entry->next = 0;
    entry->prev = 0;
}

/**
 * list_entry - get the struct for this entry
 * @ptr:    the &struct list_head pointer.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list_struct within the struct.
 */
#define shm_list_entry(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * list_for_each_entry  -   iterate over list of given type
 * @pos:    the type * to use as a loop counter.
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 */
#define shm_list_for_each_entry(base, pos, head, member)                \
    for (pos = shm_list_entry(OFFSET_NODE((base), (head)->next), typeof(*pos), member); \
         &pos->member != (head);                    \
         pos = shm_list_entry(OFFSET_NODE((base), pos->member.next), typeof(*pos), member))


//////// allocator part /////////
shm_alloc_handle init_shm_allocator(uint8_t *memStart, uint32_t chunkSize)
{
    shm_alloc_t *info = NULL;
    uint8_t *chunkEnd = NULL;
    free_shm_pool_t *node = NULL;

    if (NULL == memStart || chunkSize <= (EFFTSIZE_TO_POOLSIZE(0) + sizeof(shm_alloc_t)))
    {
        printf("paramter is invalid \n");
        return (shm_alloc_handle)info;
    }

    do
    {
        chunkEnd = (uint8_t *)memStart + chunkSize;
        memStart = (uint8_t *)ALIGN_4BYTES(memStart);

        node = (free_shm_pool_t *)(memStart + sizeof(shm_alloc_t));
        if ((uint8_t *)node >= chunkEnd)
            break;

        info = (shm_alloc_t *)memStart;
        if (info->magicFlag == CONST_MAGIC_FLAG)
            break;

        info->chunkSize = chunkEnd - (uint8_t *)node;
        // INIT_SHM_LIST_HEAD
        info->freeParts.prev = OFFSET_POINTER(&(info->freeParts), memStart);
        info->freeParts.next = OFFSET_POINTER(&(info->freeParts), memStart);

        node->size = POOLSIZE_TO_EFFTSIZE(chunkEnd - (uint8_t *)node); // for size information
//        shm_list_add_tail(memStart, &(node->list), &(info->freeSlots));
        info->candiate = OFFSET_POINTER(node, memStart);
        info->magicFlag = CONST_MAGIC_FLAG;
    }while(0);

    return (shm_alloc_handle)info;
}

int uninit_shm_allocator(shm_alloc_handle handle)
{
    shm_alloc_t *info = (shm_alloc_t *)handle;

    if (NULL == info || info->chunkSize <= 0 || info->magicFlag != CONST_MAGIC_FLAG)
        return -1;

    info->magicFlag = ~CONST_MAGIC_FLAG;
    return 0;
}

static int shm_recycle(shm_alloc_t *allocator, free_shm_pool_t *cur)
{
    uint32_t in = 0;
    free_shm_pool_t *node;
    uint8_t *chunkEnd;

//    ASSERT(allocator);

    if (NULL == cur)
        return -1;

//    chunkEnd = (uint8_t *)allocator->chunkMem + allocator->chunkSize;
//    if ((uint8_t *)cur >= chunkEnd || (void *)cur < allocator->chunkMem)
//    {
//        list_debug("%p memory not allocated by this allocator\n", cur);
//        return -1;
//    }
//
//    if (cur->size > allocator->chunkSize)
//    {
//        list_debug("memory leak!\n");
//        return -1;
//    }

    shm_list_for_each_entry(allocator, node, &(allocator->freeParts), list)
//    list_for_each_entry(node, &(allocator->freeParts), list)
    {
        if (node > cur)
            break;
        else
        {
            chunkEnd = (uint8_t *)node + EFFTSIZE_TO_POOLSIZE(node->size);
            if (chunkEnd == (uint8_t *)cur)
            {   // 直接合并
                node->size += EFFTSIZE_TO_POOLSIZE(cur->size);

                in = 1; cur = node;
//                list_debug("cur->size left grow to %d\n", node->size);
            }
            else if (chunkEnd > (uint8_t *)cur)
            {   // 内存泄漏
                list_debug("memory leak\n");
            }
        }
    }

    // 按地址, 占据前面位置
    if (!in)
        shm_list_add_tail((uint8_t *)allocator, &(cur->list), &(node->list));

    // 合并
    if (&(node->list) != &(allocator->freeParts)) // cur 不是最后
    {
//        list_debug("node %p, %d, cur %p\n", node, EFFTSIZE_TO_POOLSIZE(node->size), cur);
        if ((uint8_t *)cur + EFFTSIZE_TO_POOLSIZE(cur->size) == (uint8_t *)node)
        {
            cur->size += EFFTSIZE_TO_POOLSIZE(node->size);
            shm_list_del((uint8_t *)allocator, &(node->list));

//            list_debug("cur->size right grow to %d\n", cur->size);
        }
    }

    return 0;
}

static void shm_harvest(shm_alloc_t *allocator)
{
    size_t maxSize = 0;

    free_shm_pool_t *node = NULL;
    free_shm_pool_t *cur = NULL;

//    ASSERT(allocator);

    // 找个最大的
    shm_list_for_each_entry(allocator, node, &(allocator->freeParts), list)
        if (node->size > maxSize)
        {
            cur = node;
            maxSize = node->size;
        }

//    ASSERT(maxSize && cur);
    if (maxSize && cur)
    {
        shm_list_del((uint8_t *)allocator, &(cur->list));
        allocator->candiate = OFFSET_POINTER(cur, allocator);
        list_debug("select %p (%d bytes)\n", cur, maxSize);
    }
    else
    {
        allocator->candiate = 0;
        list_debug("really run out of memory\n");
    }
}

void * shm_malloc(shm_alloc_handle handle, size_t size)
{
    // info is memStart...
    shm_alloc_t *info = (shm_alloc_t *)handle;

    free_shm_pool_t *cur, *node;
    uint8_t *mem = NULL;
    uint32_t health = 1;

    if (NULL == info || info->chunkSize <= 0 || info->magicFlag != CONST_MAGIC_FLAG || size <= 0)
        return NULL;

    if (!info->candiate)
    {
        health = 0;
        shm_harvest(info);
    }

    do
    {
        if (!info->candiate)
            break;

        node = (free_shm_pool_t *)OFFSET_NODE(info, info->candiate); // 注意这里不是shm_list_t 地址

        if (node->size >= size)
        {
            mem = (uint8_t *)node + sizeof(size_t);
            if (node->size > ALIGN_4BYTES(size) + sizeof(free_shm_pool_t) )
            {   // 可分, split
                cur = (free_shm_pool_t *)(mem + ALIGN_4BYTES(size));

                cur->size = POOLSIZE_TO_EFFTSIZE(node->size - ALIGN_4BYTES(size)); // for size information
                node->size = ALIGN_4BYTES(size);

                info->candiate = OFFSET_POINTER(cur, info);
            }
            else // 不可分
                info->candiate = 0; // 延迟挑候选空间
        }
        else if (health)
        {
            info->candiate = 0;
            shm_recycle(info, node);

            health = 0;
            shm_harvest(info);
        }
        else // 最大也划不出来
            break;
    }while(!mem);

    return (void *)mem;
}

int shm_add_reference(shm_alloc_handle handle, void *mem)
{
    shm_alloc_t *info = (shm_alloc_t *)handle;
    free_shm_pool_t *node;
    free_shm_pool_t *cur;
    uint8_t *chunkEnd;
    size_t size;

    if (NULL == info || info->chunkSize <= 0 || info->magicFlag != CONST_MAGIC_FLAG || NULL == mem)
        return -1;

    chunkEnd = (uint8_t *)info + sizeof(shm_alloc_t) + info->chunkSize;
    if ((uint8_t *)mem >= chunkEnd || (uint8_t *)mem < (uint8_t *)info)
    {
        list_debug("%p memory not allocated by this allocator\n", mem);
        return -1;
    }

    // TODO: 利用 candiate, 遍历 freeSlot 可以做更严厉的检查
    cur = shm_list_entry(mem, free_shm_pool_t, list);
    size = cur->size & (~3);
    if (size > info->chunkSize)
    {
        list_debug("memory leak!\n");
        return -1;
    }

    if (size + 3 <= cur->size)
        return -1;

    ++cur->size;
    return 0;
}

void shm_free(shm_alloc_handle handle, void *mem)
{
    shm_alloc_t *info = (shm_alloc_t *)handle;

    free_shm_pool_t *node;
    free_shm_pool_t *cur;
    uint8_t *chunkEnd;
    size_t size;

    if (NULL == info || info->chunkSize <= 0 || info->magicFlag != CONST_MAGIC_FLAG || NULL == mem)
        return;

    // 外层检查
    chunkEnd = (uint8_t *)info + sizeof(shm_alloc_t) + info->chunkSize;
    if ((uint8_t *)mem >= chunkEnd || (uint8_t *)mem < (uint8_t *)info)
    {
        list_debug("%p memory not allocated by this allocator\n", mem);
        return;
    }

    cur = shm_list_entry(mem, free_shm_pool_t, list);
    size = cur->size & (~3);
    if (size > info->chunkSize)
    {
        list_debug("memory leak!\n");
        return;
    }

    if (cur->size & 3)
    {   // decrease reference
        --cur->size;
        return;
    }

    shm_recycle(info, cur);

    return;
}

inline uint32_t shm_offset(shm_alloc_handle handle, void *mem)
{
    shm_alloc_t *info = (shm_alloc_t *)handle;

    if (NULL == info || info->chunkSize <= 0 || info->magicFlag != CONST_MAGIC_FLAG || NULL == mem)
        return 0xFFFFFFFF;

    return OFFSET_POINTER(mem, info);
}

inline void *shm_pointer(shm_alloc_handle handle, uint32_t offset)
{
    shm_alloc_t *info = (shm_alloc_t *)handle;

    if (NULL == info || info->chunkSize <= 0 || info->magicFlag != CONST_MAGIC_FLAG || \
        (sizeof(shm_alloc_t) + info->chunkSize) <= offset)
        return NULL;

    return (handle + offset);
}

char * shm_strdup(shm_alloc_handle handle, const char *s)
{
    shm_alloc_t *info = (shm_alloc_t *)handle;

    size_t len = strlen(s);
    char *mem = NULL;

    if (len)
        mem = shm_malloc(info, len + 1);

    if (mem)
        snprintf(mem, len + 1, "%s", s);

    return mem;
}

#ifdef TEST_LIST_SORT


void shm_alloc_dump(shm_alloc_t *info)
{
    int i;
    size_t totalSize, unitSize;
    free_shm_pool_t *cur;

    if (NULL == info || info->chunkSize <= 0 || info->magicFlag != CONST_MAGIC_FLAG)
    {
        list_debug("invalid allocator\n");
        return;
    }

    list_debug("***** dump %p *****\n", info);
    cur = (free_shm_pool_t *)OFFSET_NODE(info, info->candiate); // 注意这里不是shm_list_t 地址
    if (info->candiate)
    {
        list_debug("CK: %p <-- %d bytes\n", cur, cur->size);
        totalSize = EFFTSIZE_TO_POOLSIZE(cur->size);
    }
    else
    {
        list_debug("CK: ( empty ) <--  0 bytes\n");
        totalSize = 0;
    }

    i = 0;
    shm_list_for_each_entry(info, cur, &(info->freeParts), list)
    {
        list_debug("P%d: %p --> %d bytes\n", ++i, cur, cur->size);
        totalSize += EFFTSIZE_TO_POOLSIZE(cur->size);
    }

    if (totalSize != info->chunkSize)
        list_debug("allocator size %d/%d\n", totalSize, info->chunkSize);
    else
        list_debug("full available\n");

    return;
}

void shm_dump(shm_alloc_handle handle)
{
    shm_alloc_t *info = (shm_alloc_t *)handle;
    shm_alloc_dump(info);
}
#endif


