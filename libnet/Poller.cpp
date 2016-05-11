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

#include "sh_print.h"
#include "ioStream.h"

#define LOG_TAG "Poller"
#include "utility.h"

#define CONTAINER_LIMIT         (50)
#define MAX_EPOLL_EVENT_NUM     (50)

#define IS_VALID_FD(s)          ((s) >= 0 )

using namespace android;

class wakePipe : public ITransport
{
public:
    static wakePipe *create(SocketPoller *target);

    virtual ~wakePipe();

    status_t wake(uint32_t token);
protected:
    virtual void handleClose() {};
    virtual void handleFree() {};
    virtual void handleRead() ;
    virtual void handleWrite() {};

private:
    SocketPoller *mTarget;
    int write_fd;

    status_t init();

    wakePipe(SocketPoller *target);
    wakePipe(const wakePipe &);
    wakePipe &operator=(const wakePipe &);
};


Mutex SocketPoller::cLock;
sp<SocketPoller> SocketPoller::instance;

SocketPoller::SocketPoller()
{
    coreDescriptor = -1;
}

SocketPoller::~SocketPoller()
{
    mWakeOn = NULL;
    close(coreDescriptor);
    coreDescriptor = -1;
}

status_t SocketPoller::init()
{
    status_t err = UNKNOWN_ERROR;
    int corefd = -1;
    do
    {
        //Mutex::Autolock _l(mLock);

        BREAK_IF(coreDescriptor != -1);

        corefd = epoll_create(CONTAINER_LIMIT);
        BREAK_IF(fcntl(corefd, F_SETFD, 1) == -1); // sub-process need not

        SocketHolder *_holder = SocketHolder::Instance();
        BREAK_IF(_holder == NULL);

        mWakeOn = wakePipe::create(this);
        BREAK_IF(mWakeOn == NULL);

        err = _holder->insert(mWakeOn->sock, mWakeOn.get());
        BREAK_IF(err != NO_ERROR);

        struct epoll_event ev;
        ev.data.fd = mWakeOn->sock;
        ev.events = EPOLLIN; // | EPOLLOUT ;
        LOGV("[AddTransport] wake descriptor %d \n", mWakeOn->sock);

        BREAK_IF(-1 == epoll_ctl(corefd, EPOLL_CTL_ADD, \
                           mWakeOn->sock, &ev));

        coreDescriptor = corefd;
        workProgress[0] = 0;

        err = NO_ERROR;
    }while(0);

    if (err != NO_ERROR && corefd != -1) {
        close(corefd);
    }

    return err;
}

bool SocketPoller::threadLoop()
{
    int i, fd_num = 0;
    struct epoll_event events[MAX_EPOLL_EVENT_NUM];

    do
    {
        int time_out;
        int _sec, _usec;

        mTimerQ.timeToNextEvent(_sec, _usec);
        time_out = _sec*1000 + _usec/1000;

        fd_num = epoll_wait(coreDescriptor, events,
                            MAX_EPOLL_EVENT_NUM, time_out);

        if (Thread::exitPending()) {
            return false;
        }
        if (fd_num < 0 && EINTR != errno) {
            LOGE("epoll_wait return -1, %s", strerror(errno));
            return false;
        }

        mTimerQ.handleAlarmEvent();
    }while (fd_num <= 0);

    {
        //LOGV("[epAction] %d event(s)\n", fd_num);
        sp<ITransport> _trans;
        int _sock;
        status_t err;
        for (i=0; i<fd_num; i++)
        {
            _sock = events[i].data.fd;
            //_unit = (weakSocket *)events[i].data.ptr;
            err = SocketHolder::Instance()->fetch(_sock, _trans);
            if (err == NO_ERROR) {
                time_t _current = time(NULL);
                snprintf(workProgress, sizeof(workProgress),
                         "0x%08X on %d @%s", events[i].events,
                         _trans->sock, ctime(&_current));
            } else {
                LOGW("fetch socket %d failed", _sock);
                workProgress[0] = 0;
                continue;
            }

            /// since it had been checked while performing add fucntion
            /// there is no need to check pointer is 'NULL' or not, most
            /// crash is caused by user's fail to keep the non-null
            /// pointer be 'accessable'

            if (events[i].events & (EPOLLHUP|EPOLLERR)) {   // passive close
                if IS_VALID_FD(_trans->sock) {// actually flag
                    close(_trans->sock);
                    SocketHolder::Instance()->remove(_trans->sock);

                    _trans->sock = -1;
                    _trans->handleClose();
                } else {
                    LOGW("epoll close case on socket -1");
                }
            } else if IS_VALID_FD(_trans->sock) {
                if (events[i].events & EPOLLIN) {
                    // fprintf(stderr, "Socket : %d: Addr: %p \n", _trans->sock, _trans);
                    _trans->handleRead();
                }
                if (events[i].events & EPOLLOUT) {
                    _trans->handleWrite();
                }
            } else {
                LOGW("unpredictable event %x happen?", events[i].events);
            }

            workProgress[0] = 0;
        }
    }

    return true;
}

status_t SocketPoller::AddTransport(ITransport *transport)
{
    //F_LOG;
    status_t ret = UNKNOWN_ERROR;

    struct epoll_event ev;

    do
    {
        BREAK_IF(transport == NULL);
        BREAK_IF(transport->sock == -1);

        SocketHolder *_holder = SocketHolder::Instance();
        BREAK_IF(_holder == NULL);
        _holder->insert(transport->sock, transport);

        ev.data.fd = transport->sock;
        ev.events = EPOLLIN; // | EPOLLOUT ;

        /// it is epoll's duty to keep socket valid, this ADD method
        /// cohere an user pointer with the socket, so the user should keep
        /// the pointer be valid in socket lifetime
        LOGV("[AddTransport] descriptor %d \n", transport->sock);

        BREAK_IF(-1 == epoll_ctl(coreDescriptor, EPOLL_CTL_ADD, \
                           transport->sock, &ev));

        ret = NO_ERROR;
    }while(0);

    return ret;
}

status_t SocketPoller::CloseTransport(ITransport *transport)
{
    //F_LOG;
    status_t ret = UNKNOWN_ERROR;
    do
    {
        BREAK_IF(transport == NULL);
        DOING_IF(transport->sock != -1); // which means already closed

        int _sock = transport->sock;
        transport->sock = -1;

        LOGV("[DelTransport] descriptor %d \n", _sock);

        SocketHolder *_holder = SocketHolder::Instance();
        BREAK_IF(_holder == NULL);
        _holder->remove(_sock);

        struct epoll_event ev;
        BREAK_IF(-1 == epoll_ctl(coreDescriptor, EPOLL_CTL_DEL, \
                           _sock, &ev));

        close(_sock);
        _sock = -1;
        transport->handleClose();

        ret = NO_ERROR;
    }while(0);

    return ret;
}

int SocketPoller::AddTimedCallback(uint32_t mseconds, IAbsTimerHandler *cbfunc)
{
    int token = -1;

    if (!cbfunc || !mseconds) {
        return BAD_VALUE;
    }

    token = mTimerQ.addLaterEvent(mseconds, TimedEventQ::TIMED_CALLBACK, cbfunc);
    if (token != -1) {
        mWakeOn->wake('func');
    }
    return token;
}

int SocketPoller::AddPeriodSignal(uint32_t mseconds, pthread_cond_t *cond)
{
    int token = -1;

    if (!cond || !mseconds) {
        return BAD_VALUE;
    }

    token = mTimerQ.addPeriodEvent(mseconds, TimedEventQ::PERIOD_CONDITION, cond);
    if (token != -1) {
        mWakeOn->wake('cond');
    }
    return token;
}

void *SocketPoller::DiscardEvent(int _id)
{
    return mTimerQ.delTimedEvent(_id);
}

void SocketPoller::dumpInfo(void *stream)
{
    char _buffer[1024];
    ioStream _logs, *_plogs;
    _logs.buffer = _buffer;
    _logs.size = sizeof(_buffer);

    ioStreamRewind(&_logs);
    _plogs = (stream == NULL) ? &_logs : (ioStream *)stream;

    ioStreamPrint(_plogs, " \\--------------------------------\n");
    ioStreamPrint(_plogs, " \\ event: %s\n", workProgress);
    SocketHolder::Instance()->dumpInfo(_plogs);
    ioStreamPrint(_plogs, " \\ timer: ");
    mTimerQ.dump(_plogs);
    ioStreamPrint(_plogs, " \\--------------------------------\n");

    if (stream == NULL) {
        ioStreamSeek(_plogs, -1, SEEK_CUR);
        ioStreamPutc('\0', _plogs);
        shell_print("%s", _buffer);
    } /*else {
        ioStreamPutc('\n', _plogs);
    }*/
}

wakePipe *wakePipe::create(SocketPoller *target)
{
    //F_LOG;
    wakePipe* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(target == NULL);

        self = new wakePipe(target);
        BREAK_IF(self == NULL);

        err = self->init();
    }while(0);

    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }

    return self;
}

wakePipe::wakePipe(SocketPoller *target)
    : mTarget(target), write_fd(-1)
{
}

wakePipe::~wakePipe()
{   // we should close ITransport::sock here, not in ~ITransport()
    if (ITransport::sock != -1) {
        close(ITransport::sock);
        ITransport::sock = -1;
    }

    if (write_fd != -1) {
        close(write_fd);
        write_fd = -1;
    }
}

status_t wakePipe::init()
{
    status_t err = UNKNOWN_ERROR;
    int pipefd[2];

    do
    {
        err = pipe(pipefd);
        BREAK_IF(err != NO_ERROR);

        // set read fd non-block
        int flags = fcntl(pipefd[0], F_GETFL, 0);
        BREAK_IF(flags == -1);

        flags |= O_NONBLOCK;
        err = fcntl(pipefd[0], F_SETFL, flags);
        BREAK_IF(err != NO_ERROR);

        flags = fcntl(pipefd[1], F_GETFL, 0);
        BREAK_IF(flags == -1);

        flags |= O_NONBLOCK;
        err = fcntl(pipefd[1], F_SETFL, flags);

    }while(0);

    if (err != NO_ERROR) {
        if (pipefd[0] != -1) {
            close(pipefd[0]);
            pipefd[0] = -1;
        }

        if (pipefd[1] != -1) {
            close(pipefd[1]);
            pipefd[1] = -1;
        }
    } else {
        ITransport::sock = pipefd[0];
        write_fd = pipefd[1];
    }

    return err;
}

status_t wakePipe::wake(uint32_t token)
{
    write(write_fd, &token, sizeof(token));
    return 0;
}

void wakePipe::handleRead()
{
#if 0 // currently, it is useless
    if (mTarget) {
        mTarget->handleRead();
    }
#endif
    // just read it out
    int buffer[128];
    read(ITransport::sock, buffer, sizeof(buffer));
}

