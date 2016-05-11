#ifndef __IOSTREAM_H__
#define __IOSTREAM_H__

#include <stdio.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C"
{
#endif


typedef struct _ioStream ioStream;

struct _ioStream
{
    char    *buffer;
    size_t  size;

    char    *cursor;
    size_t  length;
};

int ioStreamSeek(ioStream *thisBuf, off_t offset, int whence);
int ioStreamRewind(ioStream *thisBuf);
int ioStreamTell(ioStream *thisBuf);
int ioStreamIsEnd(ioStream *thisBuf);

int ioStreamPrint(ioStream *thisBuf, const char* format, ...);
int ioStreamWrite(ioStream * thisBuf, const void * buf, size_t count);
int ioStreamPutc(int c, ioStream *thisBuf);

#ifdef __cplusplus
};
#endif

#endif
