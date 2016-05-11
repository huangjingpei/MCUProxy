#include <stdarg.h>
#include <memory.h>
#include <assert.h>  // define NDEBUG to disable it

#include "ioStream.h"

#if defined(NWDEBUG)
#define ASSERT(x)   assert(x)
#else
#define ASSERT(x)   ((void)0)
#endif


inline int ioStreamOpen(ioStream *thisBuf)
{
    int ret = ioStreamRewind(thisBuf);

    if (ret) return ret;

    memset(thisBuf->buffer, 0, thisBuf->size);
    return 0;
}

inline int ioStreamRewind(ioStream *thisBuf)
{
    ASSERT(thisBuf);
    ASSERT(thisBuf->buffer);
    ASSERT(thisBuf->size > 0);

    thisBuf->cursor = thisBuf->buffer;
    thisBuf->length = thisBuf->size;

    return 0;
}

inline int ioStreamTell(ioStream *thisBuf)
{
    ASSERT(thisBuf);
    ASSERT(thisBuf->buffer);
    ASSERT(thisBuf->size > 0);

    return thisBuf->size - thisBuf->length;
}

inline int ioStreamSeek(ioStream *thisBuf, off_t offset, int whence)
{
    int ret = -1;
    off_t move;

    ASSERT(thisBuf);
    ASSERT(thisBuf->buffer);
    ASSERT(thisBuf->size > 0);

    do
    {
        if (SEEK_SET == whence)
            move = offset;
        else if (SEEK_END == whence)
            move = thisBuf->size + offset;
        else if (SEEK_CUR == whence)
            move = thisBuf->size - thisBuf->length + offset;
        else
            break;

        if (move < 0)
            move = 0;
        else if (move > thisBuf->size)
            move = thisBuf->size;

        thisBuf->cursor = thisBuf->buffer + move;
        thisBuf->length = thisBuf->size - move;

        ret = thisBuf->size - thisBuf->length;
    }while(0);

    return ret;
}

int ioStreamPrint(ioStream *thisBuf, const char* format, ...)
{
    int ret = -1;
    va_list vlist;

    ASSERT(thisBuf);
    ASSERT(thisBuf->buffer);
    ASSERT(thisBuf->size > 0);

    if ((thisBuf->cursor < thisBuf->buffer) ||
        (thisBuf->buffer + thisBuf->size) <= thisBuf->cursor)
        return ret;

    va_start(vlist, format);

    ret = vsnprintf(thisBuf->cursor, thisBuf->length, format, vlist);

    va_end(vlist);

    if (ret >= 0)
    {
        thisBuf->cursor += ret;
        thisBuf->length -= ret;
    }

    return ret;
}

int ioStreamWrite(ioStream *thisBuf, const void *buf, size_t count)
{
    int ret = -1;

    ASSERT(thisBuf);
    ASSERT(thisBuf->buffer);
    ASSERT(thisBuf->size > 0);

    if ((thisBuf->cursor < thisBuf->buffer) ||
        (thisBuf->buffer + thisBuf->size) <= thisBuf->cursor)
        return ret;

    ret = (count < thisBuf->length)? count : thisBuf->length;

    if (ret > 0)
    {
        memcpy(thisBuf->cursor, buf, ret);

        thisBuf->cursor += ret;
        thisBuf->length -= ret;
    }

    return ret;
}

int ioStreamPutc(int c, ioStream *thisBuf) // to make it compatible with putc
{
    int ret = -1;

    ASSERT(thisBuf);
    ASSERT(thisBuf->buffer);
    ASSERT(thisBuf->size > 0);

    if ((thisBuf->cursor < thisBuf->buffer) ||
        (thisBuf->buffer + thisBuf->size) <= thisBuf->cursor)
        return ret;

    if (thisBuf->length > 1)
    {
        *(thisBuf->cursor) = (char)c;
        ret = *(thisBuf->cursor);

        thisBuf->cursor++;
        thisBuf->length--;
    }

    return ret;
}

inline int ioStreamIsEnd(ioStream *thisBuf)
{
    int ret = 0; // false

    ASSERT(thisBuf);
    ASSERT(thisBuf->buffer);
    ASSERT(thisBuf->size > 0);

    if ((thisBuf->cursor < thisBuf->buffer) ||
        (thisBuf->buffer + thisBuf->size) <= thisBuf->cursor)
        ret = 1;

    return ret;
}

