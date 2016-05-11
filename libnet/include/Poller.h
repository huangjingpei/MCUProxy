#ifndef POLLER_H
#define POLLER_H

#include <unistd.h>
#include <utils/threads.h>
#include <utils/List.h>

#include "MetaBuffer.h"
#include "absTimerList.h"
#include "myAlloc.h"

#define CONTAINER_LIMIT         (50)

using namespace android;

struct ITransport;
class wakePipe;

class SocketHolder : public RefBase
{
public:
    struct socketItem {
        int reserved;
        socketItem *prev, *next;
        int socket_fd;

        ITransport *socket_inst;
        RefBase::weakref_type *ref;
    };

    static inline SocketHolder* Instance()
    {
        if (instance == NULL) {
            Mutex::Autolock _l(cLock);
            do {
                if (instance != NULL)
                    break;

                sp<SocketHolder> ptr = new SocketHolder();
                if (ptr == NULL)
                    break;

                instance = ptr;
            }while(0);
        }
        return instance.get();
    };

    status_t insert(int _sock, ITransport *_inst);
    status_t fetch(int _sock, sp<ITransport> &_inst);
    status_t remove(int _sock);

    void dumpInfo(void *stream);
private:
    Mutex mLock;
    socketItem mQueue;

    inline void removeItem(socketItem *_item);

    slot_alloc_t mAlloc;
    socketItem mArray[CONTAINER_LIMIT];

protected:
    SocketHolder();
    ~SocketHolder();

private:
    static Mutex cLock;
    static sp<SocketHolder> instance;
};

class SocketPoller : public Thread
{
public:
    static inline SocketPoller* Instance()
    {
        if (instance == NULL) {
            Mutex::Autolock _l(cLock);
            do {
                if (instance != NULL)
                    break;

                sp<SocketPoller> ptr = new SocketPoller();
                if (ptr == NULL)
                    break;

                if (NO_ERROR != ptr->init())
                    break;

                instance = ptr;
            }while(0);
        }
        return instance.get();
    };

    status_t AddTransport(ITransport *transport);
    status_t CloseTransport(ITransport *transport);

    int AddTimedCallback(uint32_t mseconds, IAbsTimerHandler *cbfunc);
    int AddPeriodSignal(uint32_t mseconds, pthread_cond_t *cond);
    void *DiscardEvent(int eventId);

    void dumpInfo(void *stream);
protected:
    int coreDescriptor;

    friend class wakePipe;
    sp<wakePipe> mWakeOn;

    TimedEventQ mTimerQ;

    // debug
    char workProgress[256];

protected:
    status_t init();
    virtual bool        threadLoop();

    SocketPoller();
    virtual ~SocketPoller();

private:
    static Mutex cLock;
    static sp<SocketPoller> instance;
};

struct ITransport : public RefBase
{
    int sock;
    virtual void handleRead() = 0;  /* called when socket got read event */
    virtual void handleWrite() = 0; /* called when socket got write event */
    virtual void handleClose() = 0; /* called when socket should close */
    virtual void handleFree() = 0;  /* called when socket was closed by peer when set */

    ITransport() { sock = -1; };
    virtual ~ITransport() {
        SocketPoller::Instance()->CloseTransport(this);
    };
};

class SocketSender : public Thread
{
public:
    static inline SocketSender* Instance()
    {
        if (instance == NULL) {
            Mutex::Autolock _l(cLock);
            do {
                if (instance != NULL)
                    break;

                sp<SocketSender> ptr = new SocketSender();
                if (ptr == NULL)
                    break;

                if (NO_ERROR != ptr->init())
                    break;

                instance = ptr;
            }while(0);
        }
        return instance.get();
    };

    status_t deliver(const sp<MetaBuffer> &buffer, int64_t timeUs,
                     bool video=true, bool marker=false);

    virtual void requestExit();
    void dumpInfo(void *stream);

protected:
    pthread_mutex_t mLock;
    pthread_cond_t mTick;

    List<sp<MetaBuffer> > mBuffers;
    List<sp<MetaBuffer> > mPending;
    int mToken;

    status_t init();

protected:
    virtual bool        threadLoop();

    SocketSender();
    virtual ~SocketSender();

private:
    static Mutex cLock;
    static sp<SocketSender> instance;
};

#endif /* POLLER_H */

