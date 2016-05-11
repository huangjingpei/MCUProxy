/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>

#include "TokenPool.h"



using namespace android;

// static
TokenPool TokenPool::gAtomizer;

// static
const char *TokenPool::atomize(const char *name)
{
    return gAtomizer.atomize_internal(name);
}

void TokenPool::clear()
{
    gAtomizer.clear_internal();
}

void TokenPool::dump()
{
    gAtomizer.dump_internal();
}


TokenPool::TokenPool()
{
    bufferPtr = tokenBuffer;
    for (int i = 0; i < CONST_TOKENPOOL_ROWS; ++i)
    {
        mRows.push(List<char *>());
    }
}

const char *TokenPool::atomize_internal(const char *name)
{
    Mutex::Autolock _l(mLock);

    const size_t n = mRows.size();
    size_t index = TokenPool::Hash(name) % n;
    List<char *> &entry = mRows.editItemAt(index);
    List<char *>::iterator it = entry.begin();
    while (it != entry.end()) {
        if (0 == strcmp((*it), name)) {
            return (*it);
        }
        ++it;
    }

    entry.push_back(alloc_nolock(name));

    return (*--entry.end());
}

void TokenPool::clear_internal()
{
    Mutex::Autolock _l(mLock);
    bufferPtr = tokenBuffer;

    for (int i = 0; i < CONST_TOKENPOOL_ROWS; ++i)
    {
        List<char *> &entry = mRows.editItemAt(i);
        entry.clear();
    }
}

void TokenPool::dump_internal()
{
    Mutex::Autolock _l(mLock);

    for (int i = 0; i < CONST_TOKENPOOL_ROWS; ++i)
    {
        List<char *> &entry = mRows.editItemAt(i);
        List<char *>::iterator it = entry.begin();

        printf("ROW %d", i);
        while (it != entry.end()) {
            printf("[%s]", (*it));
            ++it;
        }
        printf("\n");
    }
}

char * TokenPool::alloc_nolock(const char *name)
{
    char *_item = NULL;

    do
    {
        if (name == NULL)
            break;

        int size = 1 + strlen(name);
        if ((bufferPtr + size) >=
            &tokenBuffer[CONST_TOKENPOOL_SIZE]) {
            break;
        }

        _item = bufferPtr;
        strcpy(_item, name);
        bufferPtr += size;

    }while(0);

    return _item;
}

// static
uint32_t TokenPool::Hash(const char *s)
{
#if 0
    uint32_t sum = 0;
    while (*s != '\0') {
        sum = (sum * 31) + *s;
        ++s;
    }

    return sum;
#else
    uint32_t hash = 5381;
    int count = strlen(s);
    int c;

    if (count < 0 || count >= 7)
        count = 7;      // common hash for 7 char

    while ((c = *(s++)) && count--)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
#endif
}


