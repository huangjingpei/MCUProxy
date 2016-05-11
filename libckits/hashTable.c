#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashTable.h"

#define CONST_MAX_HASHSIZE  (4096)

typedef struct _hashToken hashToken;
struct _hashToken
{
    hashToken *next;      // for hash table linkage
    const char *name;
    void *value;
};

typedef struct _tableInfo tableInfo;
struct _tableInfo
{
    uint32_t hashSize;
    hashToken *_table[0];
};


static allocFunc _malloc = malloc;
static freeFunc _free = free;
static allocFunc _tkmalloc = malloc;
static freeFunc _tkfree = free;

static inline char * _hashStrdup(const char *s)
{
    size_t len = strlen(s);
    char *mem = NULL;

    if (s)
    {
        if (len >= 0)
            mem = _malloc(len + 1);

        if (mem)
            snprintf(mem, len + 1, "%s", s);
    }

    return mem;
}

static uint32_t djb2_hash(const char *str, int count)
{
    uint32_t hash = 5381;
    int c;

    if (count < 0 || count >= 7)
        count = 7;      // common hash for 7 char

    while ((c = *(str++)) && count--)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static inline uint32_t realHashSize(uint32_t hashSize)
{
    uint32_t testbit = CONST_MAX_HASHSIZE;
    uint32_t size = 0;

    if (hashSize > testbit)
        return testbit;

    do
    {
        if (hashSize & testbit)
            break;

        testbit >>= 1;
    }while (testbit);

    return testbit;
}


int inline setHtStringAlloc(allocFunc func1, freeFunc func2)
{
    if (!func1 || !func2)
        return -1;

    _malloc = func1;
    _free = func2;
    return 0;
}

int inline setHtTokenAlloc(allocFunc func1, freeFunc func2)
{
    if (!func1 || !func2)
        return -1;

    _tkmalloc = func1;
    _tkfree = func2;
    return 0;
}

int initHashTable(void **pHandle, uint32_t hashSize)
{
    int ret = -1;
    size_t _size;
    tableInfo *handle;

    hashSize = realHashSize(hashSize);
    printf("real hashSize = %d\n", hashSize);
    do
    {
        if (!pHandle || !hashSize)
            break;

        _size = sizeof(tableInfo) + sizeof(hashToken)*hashSize;
        handle = (tableInfo *)_malloc(_size);
        if (!handle)
            break;

        memset(handle, 0, _size);
        handle->hashSize = hashSize;

        *pHandle = handle;
        ret = 0;
    }while(0);

    return ret;
}

static hashToken * findToken(tableInfo *tbl, const char *name, uint32_t delFlag)
{
    uint32_t hashValue;
    hashToken *hashPlace = NULL;
    hashToken *prevPlace = NULL;

    hashValue = djb2_hash(name, -1) & (tbl->hashSize - 1);
    hashPlace = tbl->_table[hashValue];

    if (!tbl || !hashPlace)
        return NULL;

    //ASSERT(hashPlace->name);
    if (!strcmp(name, hashPlace->name))
    {
        if (delFlag)
        {
            tbl->_table[hashValue] = hashPlace->next;
        }
    }
    else
    {
        prevPlace = hashPlace;
        hashPlace = hashPlace->next;
        while (hashPlace)
        {
            //ASSERT(hashPlace->name);
            if (!strcmp(name, hashPlace->name))
                break;
            prevPlace = hashPlace;
            hashPlace = hashPlace->next;
        }

        if (hashPlace && delFlag)
            prevPlace->next = hashPlace->next;
    }

    return hashPlace;
}

/*
加入表时，name 做一份 copy, value 只保存指针
*/
static inline hashToken *createToken(const char *name, void *value)
{
    hashToken *newItem;
    char *_name;

    newItem = _tkmalloc(sizeof(hashToken));
    if (NULL != newItem)
    {
        _name = _hashStrdup(name);
        if (_name)
        {
            newItem->name = _name;
            newItem->value = value;
            newItem->next = NULL;
        }
        else
        {
            _tkfree(newItem);
            newItem = NULL;
        }
    }

    return newItem;
}

static inline int deleteToken(hashToken *item, void **retval)
{
    if (!item)
        return -1;

    if (retval)
        *retval = item->value;

    _free((void *)item->name);
    _tkfree((void *)item);

    return 0;
}

int setHashValue(void *handle, const char *name, void *value, void **retval)
{
    int ret = -1;
    uint32_t hashValue;
    hashToken *hashPlace = NULL;
    hashToken *newToken = NULL;
    tableInfo *tbl = (tableInfo *)handle;

    if (retval)
        *retval = NULL;

    do
    {   // add '!value' case, 2008-7-3 gaokp
        if (!handle || !name)
            break;

        newToken = findToken(handle, name, 0);
        if (!newToken)
        {
            newToken = createToken(name, value);
        }
        else
        {
            if (retval)
            {
                *retval = newToken->value;
            }
            newToken->value = value;
            ret = 0;
            break;
        }

        if (NULL == newToken)
            break;

        hashValue = djb2_hash(name, -1) & (tbl->hashSize - 1);
        hashPlace = tbl->_table[hashValue];

        while (hashPlace)
        {
            if (hashPlace->next)
                hashPlace = hashPlace->next;
            else
                break;
        }

        if (!hashPlace)
            tbl->_table[hashValue] = newToken;
        else
            hashPlace->next = newToken;

        ret = 0;
    }while(0);

    return ret;
}

int getHashValue(void *handle, const char *name, void **retval)
{
    int ret = -1;
    hashToken *newToken = NULL;
    tableInfo *tbl = (tableInfo *)handle;

    do
    {
        if (!handle || !name || !retval)
            break;

        newToken = findToken(handle, name, 0);
        if (!newToken)
            break;

        *retval = newToken->value;
        ret = 0;
    }while(0);

    return ret;
}

int delHashValue(void *handle, const char *name, void **retval)
{
    int ret = -1;
    hashToken *newToken = NULL;
    tableInfo *tbl = (tableInfo *)handle;

    do
    {
        if (!handle || !name)
            break;

        newToken = findToken(handle, name, 1);
        if (!newToken)
            break;

        ret = deleteToken(newToken, retval);
    }while(0);

    return ret;
}

int iterHashTable(void *handle, iterFunc callback, void *param)
{
    int i;
    int level;
    hashToken *node = NULL;
    iterCtrl ctrl = ITERCTRL_CONTINUE;
    tableInfo *tbl = (tableInfo *)handle;

    if (!handle || !callback)
        return -1;

    for (i=0; i<tbl->hashSize; i++)
    {
        level = 0;
        node = tbl->_table[i];

        while (node)
        {
            ctrl = callback(node->name, node->value, param);

            if (ITERCTRL_BREAK == ctrl)
                goto end;

            if (node->next)
            {
                level++;
                node = node->next;
            }
            else
                break;
        }
    }

end:
    return (ITERCTRL_BREAK == ctrl)? -1 : 0;
}

void dumpHashTable(void *handle)
{
    int i;
    int level;
    hashToken *node = NULL;
    tableInfo *tbl = (tableInfo *)handle;

    if (!handle)
        return;

    for (i=0; i<tbl->hashSize; i++)
    {
        level = 0;
        node = tbl->_table[i];

        while (node)
        {
            printf("%s(H:%d,L:%d):\n", node->name, i, level);
            printf("%p\n", node->value);

            if (node->next)
            {
                level++;
                node = node->next;
            }
            else
                break;
        }

    }
    return;
}

