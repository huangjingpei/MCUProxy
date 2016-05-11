#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "Poller.h"

#include "RTPTransport.h"

#include "sh_print.h"
#include "ioStream.h"

// 5x1.5KBx8bit represent for 6Mbps, 7.5 for 9Mbps, 6 for 7.2Mbps
#define MAX_SENDING_PACKETS     6

#define LOG_TAG "Sender"
#include "utility.h"


using namespace android;

Mutex SocketSender::cLock;
sp<SocketSender> SocketSender::instance;

SocketSender::SocketSender()
{
    mLock = PTHREAD_MUTEX_INITIALIZER;
    mTick = PTHREAD_COND_INITIALIZER;
}

SocketSender::~SocketSender()
{
    pthread_mutex_destroy(&mLock);
    pthread_cond_destroy(&mTick);
    if (mToken != -1) {
        SocketPoller::Instance()->DiscardEvent(mToken);
    }
}

status_t SocketSender::deliver(const sp<MetaBuffer> &buffer, int64_t timeUs,
                     bool video, bool marker)
{
    status_t err = BAD_VALUE;

    do
    {
        BREAK_IF(buffer == NULL);
        BREAK_IF(buffer->data() == NULL);

        int32_t _val;
        BREAK_IF(!buffer->findInt32("fd", &_val))

        buffer->setInt64("ts", timeUs);
        buffer->setInt32("video", video);
        buffer->setInt32("marker", marker);

        pthread_mutex_lock(&mLock);

        mBuffers.push_back(buffer);
        //if (++nDebugCounter % 100 == 0) {
            //LOGV("%p buffering %d", this, mBuffers.size());
        //}
        pthread_mutex_unlock(&mLock);

        err = NO_ERROR;
    }while(0);

    return err;
}

void SocketSender::requestExit()
{
    Thread::requestExit();
}

status_t SocketSender::init()
{
    status_t err = UNKNOWN_ERROR;

    do
    {
        SocketPoller *_poller = SocketPoller::Instance();
        BREAK_IF(_poller == NULL);

        mToken = _poller->AddPeriodSignal(10, &mTick);
        BREAK_IF(mToken == -1);

        err = NO_ERROR;
    }while(0);
#if 0
    if (err != NO_ERROR && mToken != -1) {
        SocketPoller::Instance()->DiscardEvent(mToken);
    }
#endif
    return err;
}

bool SocketSender::threadLoop()
{
    //F_LOG;
    status_t ret;

    pthread_mutex_lock(&mLock);
    do
    {
        ret = pthread_cond_wait(&mTick, &mLock);
    }while(ret != 0);

    if (mPending.empty()) {
        mPending = mBuffers;
        mBuffers.clear();
    } else {
        ///LOGV("%d packets pending", mPending.size());
        while (!mBuffers.empty())
        {
            const sp<MetaBuffer> buffer = *mBuffers.begin();
            mPending.push_back(buffer);
            mBuffers.erase(mBuffers.begin());
        }
    }

    pthread_mutex_unlock(&mLock);


    int i, sendnum;
    sendnum = mPending.size();
    if (sendnum > MAX_SENDING_PACKETS) {
        sendnum = MAX_SENDING_PACKETS;
    }

    status_t err;
    int32_t _fd;
    sp<ITransport> _inst;
    int64_t timeUs;
    bool video, marker;

    i = 0;
    while (!mPending.empty())
    {
        if (i >= sendnum)
            break;

        const sp<MetaBuffer> buffer = *mPending.begin();
        buffer->findInt32("fd", &_fd);
        buffer->findInt32("video", (int32_t *)&video);
        buffer->findInt32("marker", (int32_t *)&marker);
        buffer->findInt64("ts", &timeUs);
        err = SocketHolder::Instance()->fetch(_fd, _inst);
        if (err == NO_ERROR) {
            RTPTransport *_trans = static_cast<RTPTransport *>(_inst.get());
            _trans->doSend(buffer, timeUs, video, marker);
            ++i;
        }

        mPending.erase(mPending.begin());
    }

    return true;
}


void SocketSender::dumpInfo(void *stream)
{
    char _buffer[1024];
    ioStream _logs, *_plogs;
    _logs.buffer = _buffer;
    _logs.size = sizeof(_buffer);

    ioStreamRewind(&_logs);
    _plogs = (stream == NULL) ? &_logs : (ioStream *)stream;

    ioStreamPrint(_plogs, " \\--------------------------------\n");
    ioStreamPrint(_plogs, " \\ buffers: %d/%d\n",
                  mBuffers.size(), mPending.size());
    ioStreamPrint(_plogs, " \\--------------------------------\n");

    if (stream == NULL) {
        ioStreamSeek(_plogs, -1, SEEK_CUR);
        ioStreamPutc('\0', _plogs);
        shell_print("%s", _buffer);
    } /*else {
        ioStreamPutc('\n', _plogs);
    }*/
}

