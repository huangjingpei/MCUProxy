#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/epoll.h>
#endif
#include "Poller.h"

#include "sh_print.h"
#include "ioStream.h"

#define LOG_TAG "Holder"
#include "utility.h"

Mutex SocketHolder::cLock;
sp<SocketHolder> SocketHolder::instance;

SocketHolder::SocketHolder()
{
    //Mutex::Autolock _l(mLock);
    status_t err;

    mAlloc.chunkMem = &mArray[0];
    mAlloc.chunkSize = sizeof(mArray);
    mAlloc.slotSize = sizeof(mArray[0]);
    err = init_slot_allocator(&mAlloc);

    mQueue.prev = mQueue.next = &mQueue;
    ///BREAK_IF(err != NO_ERROR);
}

SocketHolder::~SocketHolder()
{
    Mutex::Autolock _l(mLock);

    socketItem *node, *cursor;
    for (cursor = mQueue.next; cursor != &mQueue;)
    {
        node = cursor;
        cursor = cursor->next;

        node->ref->decWeak(node);
        slot_free(&mAlloc, node);
    }

    mQueue.prev = mQueue.next = &mQueue;
}

status_t SocketHolder::insert(int _sock, ITransport *_inst)
{
    status_t err = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(_inst == NULL);

        Mutex::Autolock _l(mLock);
        socketItem *node, *cursor, *_unit;

        _unit = (socketItem *)slot_malloc(&mAlloc, sizeof(socketItem));
        BREAK_IF(_unit == NULL);

        _unit->next = NULL;
        _unit->socket_fd = _sock;
        _unit->socket_inst = _inst;
        _unit->ref = _inst->createWeak(_unit);

        for (cursor = mQueue.next; cursor != &mQueue; cursor = cursor->next)
        {
            if (cursor->socket_fd > _sock) {
                break;
            } else if (cursor->socket_fd == _sock) {
                if (_unit != cursor) {  // delete old
                    node = cursor->next;
                    removeItem(cursor);
                    cursor = node;  // cursor for insertion
                } else {
                    LOGE("BUG: a case not in suppose");
                    return err;
                }

                break;
            }
        }

        err = NO_ERROR;
        _unit->next = cursor;
        _unit->prev = cursor->prev;
        cursor->prev->next = _unit;
        cursor->prev = _unit;

    }while(0);

    return err;
}

status_t SocketHolder::fetch(int _sock, sp<ITransport> &_inst)
{
    status_t err = UNKNOWN_ERROR;
    bool del = false;

    do
    {
        Mutex::Autolock _l(mLock);

        socketItem *node, *cursor;

        bool found = false;
        err = NAME_NOT_FOUND;
        for (cursor = mQueue.next; cursor != &mQueue; cursor = cursor->next)
        {
            if (cursor->socket_fd > _sock) {
                break;
            } else if (cursor->socket_fd == _sock) {
                found = true;
                break;
            }
        }

        DOING_IF(found);
        err = NO_ERROR;

        // accesss to sock is a dangerous step
        if (!cursor->ref->attemptIncStrong(cursor)) {
            err = DEAD_OBJECT;
            LOGW("increase strong failed, auto dealloc");
            del = true;
        } else if (cursor->socket_inst->sock != _sock) {
            err = DEAD_OBJECT;
            LOGW("socket invalid %d-%d, auto dealloc",
                     cursor->socket_inst->sock, _sock);
            cursor->socket_inst->decStrong(cursor);
            del = true;
        } else {
            _inst = cursor->socket_inst;
            cursor->socket_inst->decStrong(cursor);
            //*_inst = cursor->socket_inst;
        }

        if (del) {
            removeItem(cursor);
        }
    }while(0);

    return err;
}


status_t SocketHolder::remove(int _sock)
{
    status_t err;

    do
    {
        Mutex::Autolock _l(mLock);

        socketItem *node, *cursor;

        bool found = false;
        err = NAME_NOT_FOUND;
        for (cursor = mQueue.next; cursor != &mQueue; cursor = cursor->next)
        {
            if (cursor->socket_fd > _sock) {
                break;
            } else if (cursor->socket_fd == _sock) {
                found = true;
                break;
            }
        }

        DOING_IF(found);
        err = NO_ERROR;

        // delete node
        removeItem(cursor);
    }while(0);

    return err;
}

void SocketHolder::removeItem(socketItem *cursor)
{
    cursor->next->prev = cursor->prev;
    cursor->prev->next = cursor->next;

    cursor->ref->decWeak(cursor);
    slot_free(&mAlloc, cursor);
}

void SocketHolder::dumpInfo(void *stream)
{
    ioStream *_plogs = (ioStream *)stream;

    if (_plogs == NULL) {
        return;
    }

    int i = 0;
    struct free_slot_t
    {
        list_t          list;
        uint8_t         unitMem[0];
    }*cur;

    Mutex::Autolock _l(mLock);

    list_for_each_entry(cur, &(mAlloc.freeSlots), list) {
        ++i;
    }
    ioStreamPrint(_plogs, " \\ fd [%d/%d]: (", i, CONTAINER_LIMIT);

    socketItem *cursor;
    for (cursor = mQueue.next; cursor != &mQueue; cursor = cursor->next)
    {
        //ioStreamPrint(_plogs, "[0x%08X/%d] -> ", cursor, cursor->socket_fd);
        ioStreamPrint(_plogs, "%d, ", cursor->socket_fd);
    }

    if (mQueue.next != &mQueue) {
        ioStreamSeek(_plogs, -2, SEEK_CUR);
    }

    /*if (stream == NULL) {
        ioStreamPutc('\0', _plogs);
        shell_print("%s", _buffer);
    } else */ {
        ioStreamPrint(_plogs, ")\n");
    }

}


