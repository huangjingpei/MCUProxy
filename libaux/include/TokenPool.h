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

#ifndef TOKEN_POOL_H
#define TOKEN_POOL_H

#include <stdint.h>

#include <utils/List.h>
#include <utils/Vector.h>
#include <utils/threads.h>

#define CONST_TOKENPOOL_SIZE   (65536l)
#define CONST_TOKENPOOL_ROWS   (128) // should be n.th power of 2

using namespace android;

struct TokenPool
{
    static const char *atomize(const char *name);
    static void clear();
    static void dump();

private:
    static TokenPool gAtomizer;

    Mutex mLock;
    Vector<List<char *> > mRows;

    char tokenBuffer[CONST_TOKENPOOL_SIZE];
    char *bufferPtr;

    const char *atomize_internal(const char *name);
    void clear_internal();
    void dump_internal();

    char *alloc_nolock(const char *name);

    static uint32_t Hash(const char *s);
private:
    TokenPool();
    TokenPool(const TokenPool &);
    TokenPool &operator=(const TokenPool &);
};



#endif  // TOKEN_POOL_H
