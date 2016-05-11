#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "epCore.h"
#include "timeList.h"
#include "myAlloc.h"

#define CONTAINER_LIMIT         20
#define MAX_EPOLL_EVENT_NUM      (20)

#define CONCRET_LIMIT         20

//#define TEST_EPOLL_PROC

#if defined(TEST_EPOLL_PROC) && defined(NWDEBUG)
#define epoll_debug(str...) printf(str)
#else
#define epoll_debug(str...)
#endif


#if defined(TEST_EPOLL_PROC) && defined(NWDEBUG)
#define epf_debug(str...) printf(str)
#else
#define epf_debug(str...)
#endif

#define trace_func(x)  do{epoll_debug("%s\n", __FUNCTION__);}while(0)

#define CHECK_SINGLETON \
do{ \
    epoll_debug("%s\n", __FUNCTION__); \
    if (!singletonFlag) { \
    	initTimeList(&epCoreStruct.waitQueue); \
        epCoreStruct.descriptor = epoll_create(CONTAINER_LIMIT); \
        if (fcntl(epCoreStruct.descriptor, F_SETFD, 1) == -1) \
          epoll_debug("error on epoll F_SETFD of %d\n", epCoreStruct.descriptor); \
        epCoreStruct.header = NULL; \
        singletonFlag = 1; \
    } \
}while(0)

#define TIMEEVENT_FACTORY_SINGLETON \
do{ \
    epf_debug("%s\n", __FUNCTION__); \
    if (!timeEventFactoryFlag) { \
        init_slot_allocator(&epTimeEventAlloc); \
        timeEventFactoryFlag = 1; \
    } \
}while(0)



typedef int (*epTimeEventHandler)(void *theEvent);


alarmEventNode * epFactoryCreateTimeEvent();
int epFactoryRecyleTimeEvent(alarmEventNode * thisEventNode);


static struct
{
    int descriptor;
    epTransport *header;

    timeListType waitQueue;
}epCoreStruct = { -1, NULL };

static int singletonFlag = 0;

static int timeEventFactoryFlag = 0;
static alarmEventNode theEventNodes[2*CONCRET_LIMIT];

static slot_alloc_t epTimeEventAlloc = \
  { .chunkMem = theEventNodes, .chunkSize = sizeof(theEventNodes), \
    .slotSize = sizeof(alarmEventNode) };


epTransport * epGetTransport(int theDesc)
{
    epTransport *prev;
    epTransport *iterator;

    prev = epCoreStruct.header;
    if (!prev || theDesc == prev->descriptor)
        return prev;
#if 1
    iterator = prev->next;
    while (iterator)
    {
        if (theDesc == iterator->descriptor)
            break;

        prev = iterator;
        iterator = prev->next;
    }
#else
    for (iterator=prev->next; iterator; \
      prev=iterator, iterator=prev->next;)
        if (theDesc == iterator->descriptor)
            break;
#endif
    if (iterator)
        prev->next = iterator->next;

    return iterator;
}

static int insertTransport(epTransport *transport)
{
    if (!transport)
        return -1;

    transport->next = epCoreStruct.header;
    epCoreStruct.header = transport;

    return 0;
}

int epAction()
{
    CHECK_SINGLETON;

    if (-1 == epCoreStruct.descriptor)
        return -1;

    do
    {
        int i, fd_num;
        int time_out;

        epTransport *transport;
        epVirtualTable *vptr;

        struct timeval tvRet;

        struct epoll_event events[MAX_EPOLL_EVENT_NUM];

        tvRet = timeToNextAlarmEvent(&epCoreStruct.waitQueue);
        time_out = tvRet.tv_sec*1000 + tvRet.tv_usec/1000;

        epoll_debug("[epAction] on wait\n");
        fd_num = epoll_wait(epCoreStruct.descriptor, \
          events, MAX_EPOLL_EVENT_NUM, time_out);

		if (fd_num < 0)
		{
            if (EINTR == errno)
            {
                handleAlarmEvent(&epCoreStruct.waitQueue);
                continue;
            }

			syslog(LOG_ERR, "epoll_wait return -1, %s", strerror(errno));
			break;
		}

        handleAlarmEvent(&epCoreStruct.waitQueue);

        if (fd_num) epoll_debug("[epAction] %d event(s)\n", fd_num);
        for (i=0; i<fd_num; i++)
        {
            transport = (epTransport *)events[i].data.ptr;
            vptr = transport->vptr;

            if (events[i].events & (EPOLLHUP|EPOLLERR))
            {   // 被动关闭
#if 1
                if (epCloseTransport(transport))
                {
                    syslog(LOG_ERR, "epoll close case on socket -1?\n");
                }
#else
                if (transport->descriptor != -1)
                {
                    close(transport->descriptor);
                    transport->descriptor = -1;
                    if (vptr->handleClose)
                        vptr->handleClose(transport);
                    else
                        epoll_debug("unhandle descriptor %d close\n", \
                         transport->descriptor);
                }
                else
                {
                    syslog(LOG_ERR, "epoll close case on socket -1");
                }
#endif
            }
            else
            {
                if (events[i].events & EPOLLIN)
            {
                if (vptr->handleRead)
                    (*vptr->handleRead)(transport);
                else
                    epoll_debug("unhandle descriptor %d read\n", \
                      transport->descriptor);
            }
            if (events[i].events & EPOLLOUT)
            {
                if (vptr->handleWrite)
                    (*vptr->handleWrite)(transport);
                else
                    epoll_debug("unhandle descriptor %d write\n", \
                      transport->descriptor);
            }
            }
#if 0
            /*
             * closing a descriptor will cause it be automatically
             * removed from epoll sets. (see man epoll faq6)
             * 注: 对方先 close，本地调用 close 后会消失
             *     对方异常, 本地调用 close 后会进下面的 case?
             */
            if (events[i].events & (~(EPOLLIN | EPOLLOUT)))
            {
                // remove it from list
//                transport = epGetTransport(transport->descriptor);
                // TODO: add return value to epCore imply who should take response for deallocate
                if (vptr->handleClose)
                    vptr->handleClose(transport);
                else
                    epoll_debug("unhandle descriptor %d close\n", \
                     transport->descriptor);
            }
#endif
        }
    }while(1);

    return 0;
}


int epAddTransport(epTransport *transport)
{
    int ret = -1;
    epTransport *theSlot;

    struct epoll_event ev;

    CHECK_SINGLETON;

    do
    {
        if (NULL == transport ||
            NULL == transport->vptr ||
            -1 == transport->descriptor ||
            -1 == epCoreStruct.descriptor)
            break;

        /*
         * although epoll allow same descriptor in one epoll set
         * and be a harmless condition (see man epoll faq.1), we
         * should make sure that there is only one callback.
         */
#if 0
        theSlot = epGetTransport(transport->descriptor);
        if (theSlot)
        {
            printf("epoll dup!!\n");
            epoll_ctl(epCoreStruct.descriptor, EPOLL_CTL_DEL, \
              transport->descriptor, &ev);
            if (theSlot->vptr->dealloc)
                theSlot->vptr->dealloc(theSlot);
        }
#endif
        ev.data.ptr = (void *)transport;
        ev.events = EPOLLIN; // | EPOLLOUT ;

        epoll_debug("[epAddTransport] descriptor %d \n", transport->descriptor);

        if (-1 == epoll_ctl(epCoreStruct.descriptor, EPOLL_CTL_ADD, \
          transport->descriptor, &ev))
            break;

        ret = 0;//insertTransport(transport);
    }while(0);

    return ret;
}
// 用于主动关闭
int epCloseTransport(epTransport *transport)
{
    int ret = 1;

    CHECK_SINGLETON;

    do
    {
        if (!transport || -1 == transport->descriptor)
            break;

        close(transport->descriptor);
        transport->descriptor = -1;

        if (transport->vptr->handleClose)
            transport->vptr->handleClose(transport);

        ret = 0;
    }while(0);

    return ret;
}

inline static int epTimeOutHandler(alarmEventNode *theEvent)
{
    int ret = (*(epTimeoutHandler)theEvent->reserved)(theEvent->cbParam);
    epFactoryRecyleTimeEvent(theEvent);

    return ret;
}

int epCallLater(int seconds, int useconds, epTimeoutHandler handler, void *param)
{
    int ret = -1;

    CHECK_SINGLETON;

    do
    {
        alarmEventNode *newAlarmNode;

        if (NULL == handler)
            break;

        newAlarmNode = epFactoryCreateTimeEvent();
        if (NULL == newAlarmNode)
        {
            epoll_debug("out of time event node");
            break;
        }
        newAlarmNode->tvRemain.tv_sec = seconds;
        newAlarmNode->tvRemain.tv_usec = useconds;

        newAlarmNode->cbTimeout = epTimeOutHandler;
        newAlarmNode->cbParam = param;

        newAlarmNode->reserved = (unsigned int)handler;
        ret = addAlarmEventNode(&epCoreStruct.waitQueue, newAlarmNode);
    }while(0);

    return ret;
}

int epCancelCall(int token)
{
    alarmEventNode *theEvent;
    theEvent = delAlarmEventNode(&epCoreStruct.waitQueue, token);

    if (theEvent)
        return epFactoryRecyleTimeEvent(theEvent);

    return -1;
}

alarmEventNode* epFactoryCreateTimeEvent()
{
    TIMEEVENT_FACTORY_SINGLETON;

    return slot_malloc(&epTimeEventAlloc, sizeof(alarmEventNode));
}


int epFactoryRecyleTimeEvent(alarmEventNode * thisEvent)
{
    TIMEEVENT_FACTORY_SINGLETON;

    if (!timeEventFactoryFlag)
        return -1;

    slot_free(&epTimeEventAlloc, thisEvent);
    return 0;
}


