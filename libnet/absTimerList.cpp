
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>

#include "absTimerList.h"
#include "ioStream.h"

#define LOG_TAG "AbsTimer"
#include "utility.h"

static const int MILLION = 1000000;
static const int BILLION = 1000000000;
static const int MAX_PERIOD_DELAY = 2146483; // (0x7FFFFFFF - MILLION)/1000
static const struct timeval TV_ZERO = {0, 0};
static const struct timeval TV_INFINITY = {0x7FFFFFFF, MILLION - 1};  // infinity


/******************************************************************************************
* 函数名称: AbsTimerList 构造函数
* 参数描述: (无)
* 返回值: (无)
* 函数作用: 初始化一个定时链表
******************************************************************************************/
TimedEventQ::TimedEventQ()
{
    pthread_mutex_init(&listMutex, NULL);

    /*
    lst->eventHead.tvRemain.tv_sec = 0x7FFFFFFF;
    lst->eventHead.tvRemain.tv_usec = MILLION - 1;
    lst->eventHead.cbTimeout = NULL;
    lst->eventHead.idToken = -1;
    INIT_LIST_HEAD(&lst->eventHead.list);*/

    gettimeofday(&tvLastSync, 0);

    listToken = 1;
}

/******************************************************************************************
* 函数名称: AbsTimerList 析构函数
* 参数描述: (无)
* 返回值: (无)
* 函数作用: 清除一个定时链表
******************************************************************************************/
TimedEventQ::~TimedEventQ()
{
    EventUnit* entry = NULL;

    pthread_mutex_lock(&listMutex);

    while (!mList.empty())
    {
        entry = mList.front();
        mList.pop_front();      // done; remove first element
        delete entry;
    }

    pthread_mutex_unlock(&listMutex);

    pthread_mutex_destroy(&listMutex);
}

// NOTICE: 下面两个函数当  + 负值或 - 负值都有一定程度的 BUG，调用时注意保护
/* res = now + seconds + useconds/1000000 */
static struct timeval time_later(int seconds, int useconds)
{
    struct timeval ret;
    struct timeval timeNow;

    // First, figure out how much time has elapsed since the last sync:
    gettimeofday(&timeNow, 0);

    ret.tv_sec = timeNow.tv_sec + seconds;
    ret.tv_usec = timeNow.tv_usec + useconds;

    while (ret.tv_usec >= MILLION)
    {
        ret.tv_usec -= MILLION;
        ++ret.tv_sec;
    }

    if (ret.tv_sec < 0)
    {
        ret = timeNow;
    }

    return ret;
}

static void time_plus(struct timeval &tv, uint32_t mseconds)
{
    if (mseconds > MAX_PERIOD_DELAY)
        return;

    tv.tv_usec += mseconds*1000;
    while (tv.tv_usec >= MILLION)
    {
        tv.tv_usec -= MILLION;
        ++tv.tv_sec;
    }
}

/* res = tv2 - tv1 */
static struct timeval time_interval(struct timeval tv2, struct timeval tv1)
{
    struct timeval ret;
    ret.tv_sec = tv2.tv_sec - tv1.tv_sec;
    ret.tv_usec = tv2.tv_usec - tv1.tv_usec;

    if (ret.tv_usec < 0)
    {
        ret.tv_usec += MILLION;
        ret.tv_sec--;
    }

    if (ret.tv_sec < 0)
    {
        ret = TV_ZERO;
    }

    return ret;
}

static int time_minus(struct timeval tv2, struct timeval tv1)
{   // CAUTION: may overflow if there is too much gap between tv2 and tv1
    int _sec, _usec;
    _sec = tv2.tv_sec - tv1.tv_sec;
    _usec = tv2.tv_usec - tv1.tv_usec;

    return _sec*1000 + _usec/1000;
}

/* return (tv2 >= tv1)? */
static int time_overthan(struct timeval tv2, struct timeval tv1)
{
    return (tv2.tv_sec > tv1.tv_sec)
         || (tv2.tv_sec == tv1.tv_sec
         && tv2.tv_usec >= tv1.tv_usec);
}

/******************************************************************************************
* 函数名称: addTimerCallback
* 参数描述:
* @seconds: 定时秒数(相对当前)
* @useconds: 定时微秒数(相对当前)
* @callback: 定时回调
*
* 返回值: 成功返回定时链表中定时事件 ID，否则返回-1
* 函数作用: 把数据加入到定时链表中
******************************************************************************************/
int TimedEventQ::addLaterEvent(uint32_t mseconds, EventType type, void *handler)
{
    int ret = -1;
    EventUnit* newEntry = NULL;

    do
    {
        if (!handler || (mseconds > MAX_PERIOD_DELAY) ||
            (type != TIMED_CALLBACK && type != TIMED_RECYCLE)) {
            break;
        }

        newEntry = new EventUnit;
        if (!newEntry)
            break;

        newEntry->type = type;
        newEntry->handler = handler;

        gettimeofday(&newEntry->tvAbsolute, 0);
        time_plus(newEntry->tvAbsolute, mseconds);
        addEventInternal(newEntry);

        pthread_mutex_lock(&listMutex);
        newEntry->idToken = listToken;
        listToken = (listToken + 1) & 0x0FFFFFFF;
        pthread_mutex_unlock(&listMutex);

        ret = newEntry->idToken;
    }while(0);

    if (ret == -1)
        delete newEntry;

    return ret;
}

/******************************************************************************************
* 函数名称: addTimerCallback
* 参数描述:
* @absTime: 定时秒数(绝对数值)
* @callback: 定时回调
*
* 返回值: 成功返回定时链表中定时事件 ID，否则返回-1
* 函数作用: 把数据加入到定时链表中
******************************************************************************************/
int TimedEventQ::addAbsTimedEvent(struct timeval absTime,
                                  EventType type, void *handler)
{
    int ret = -1;
    EventUnit* newEntry = NULL;
    list<EventUnit *>::iterator iter;

    do
    {
        if (!handler) // ought to be something
            break;

        newEntry = new EventUnit;
        if (!newEntry)
            break;

        newEntry->tvAbsolute = absTime;
        newEntry->type = type; // TODO: check validation
        newEntry->handler = handler;

        pthread_mutex_lock(&listMutex);

        for (iter=mList.begin(); iter!=mList.end(); ++iter)
        {
            if (!time_overthan(absTime, (*iter)->tvAbsolute))
                break;
        }

        newEntry->idToken = listToken;
        listToken = (listToken + 1) & 0x0FFFFFFF;

        mList.insert(iter, newEntry);

        pthread_mutex_unlock(&listMutex);

        ret = newEntry->idToken;
    }while(0);

    if (ret == -1)
        delete newEntry;

    return ret;
}


int TimedEventQ::addPeriodEvent(uint32_t mseconds, EventType type, void *handler)
{
    int ret = -1;
    EventUnit* newEntry = NULL;

    do
    {
        if (!handler || (mseconds > MAX_PERIOD_DELAY) ||
            (type != PERIOD_CONDITION && type != PERIOD_SEMAPHORE)) {
            break;
        }

        newEntry = new EventUnit;
        if (!newEntry)
            break;

        newEntry->type = type;
        newEntry->handler = handler;
        newEntry->period = mseconds;

        gettimeofday(&newEntry->tvAbsolute, 0);
        time_plus(newEntry->tvAbsolute, mseconds);
        addEventInternal(newEntry);

        pthread_mutex_lock(&listMutex);
        newEntry->idToken = listToken;
        listToken = (listToken + 1) & 0x0FFFFFFF;
        pthread_mutex_unlock(&listMutex);

        ret = newEntry->idToken;
    }while(0);

    if (ret == -1)
        delete newEntry;

    return ret;
}


void TimedEventQ::addEventInternal(EventUnit *unit)
{
    list<EventUnit *>::iterator iter;
    //ASSERT(unit);
    pthread_mutex_lock(&listMutex);

    for (iter=mList.begin(); iter!=mList.end(); ++iter)
    {
        if (!time_overthan(unit->tvAbsolute, (*iter)->tvAbsolute))
            break;
    }

    mList.insert(iter, unit);

    pthread_mutex_unlock(&listMutex);
}

/******************************************************************************************
* 函数名称: timeToNextAlarmEvent
* 参数描述:
* @seconds: 下一次定时事件秒数(相对当前)
* @useconds: 下一次定时事件微秒数(相对当前)
*
* 返回值: 成功返回0，否则返回-1
* 函数作用: 返回定时链表中下一次事件发生的间隔时间
******************************************************************************************/
int TimedEventQ::timeToNextEvent(int& seconds, int& useconds)
{
    //F_LOG;
    int ret = -1;
    struct timeval tvRet;
    struct timeval timeNow;
    do
    {
        ret = abstimeToNextEvent(tvRet);
        gettimeofday(&timeNow, 0);
        tvRet = time_interval(tvRet, timeNow);

        seconds = tvRet.tv_sec;
        useconds = tvRet.tv_usec;

    }while(0);

    return ret;
}

/******************************************************************************************
* 函数名称: timeToNextAlarmEvent
* 参数描述:
* @absTime: 下一次定时事件时间(绝对数值)
*
* 返回值: 成功返回0，否则返回-1
* 函数作用: 返回定时链表中下一次事件发生的间隔时间
******************************************************************************************/
int TimedEventQ::abstimeToNextEvent(struct timeval& absTime)
{
    int ret = -1;
    list<EventUnit *>::iterator iter;
    absTime = TV_INFINITY; //(struct timeval){0x7FFFFFFF, MILLION - 1};  // infinity


    pthread_mutex_lock(&listMutex);

    do
    {
        if (mList.empty())
            break;

        iter = mList.begin();
        absTime = (*iter)->tvAbsolute;
        ret = 0;
    }while(0);

    pthread_mutex_unlock(&listMutex);

    return ret;
}

/******************************************************************************************
* 函数名称: handleAlarmEvent
* 参数描述: (无)
*
* 返回值: 无
* 函数作用: 查看定时链表中首个时间单元的时间是否用尽，用尽则进行回调处理
******************************************************************************************/
void TimedEventQ::handleAlarmEvent()
{
    //F_LOG;

    list<EventUnit*> _list;
    list<EventUnit *>::iterator iter;

    struct timeval timeNow;
    EventUnit* entry;

    // TODO: 反复调用直至没有 timeout 事件？
    pthread_mutex_lock(&listMutex);

    gettimeofday(&timeNow, 0);

    for (iter=mList.begin(); iter!=mList.end(); ++iter)
    {
        if (!time_overthan(timeNow, (*iter)->tvAbsolute))
            break;
    }

    _list.splice(_list.begin(), mList, mList.begin(), iter);

    pthread_mutex_unlock(&listMutex);

    bool period;
    while (!_list.empty())
    {
        entry = _list.front();
        _list.pop_front();
        //ASSERT(entry->handler);

        period = false;
        switch(entry->type) {
        case TIMED_CALLBACK:
            {
                IAbsTimerHandler *_callback = \
                    static_cast<IAbsTimerHandler *>(entry->handler);
                _callback->handleTimeout();
            }break;

        case TIMED_RECYCLE:
            {
                delete entry->handler;
                entry->handler = NULL;
            }break;

        case PERIOD_CONDITION:
            {
                period = true;
                pthread_cond_t *_condition = \
                    static_cast<pthread_cond_t *>(entry->handler);
                pthread_cond_signal(_condition);
            }break;

        case PERIOD_SEMAPHORE:
            {
                period = true;
                sem_t *_semaphore = \
                    static_cast<sem_t *>(entry->handler);
                sem_post(_semaphore);
            }break;

        default:
            break;
        }

        if (period) {
            //LOGV("period event %d re-arranged", entry->idToken);
            gettimeofday(&entry->tvAbsolute, 0);
            time_plus(entry->tvAbsolute, entry->period);
            addEventInternal(entry);
        } else {
            delete entry;
        }
    }

    return;
}

/******************************************************************************************
* 函数名称: delTimerAlarm
* 参数描述:
* @token: addTimerCallback 返回的定时事件 ID
*
* 返回值: 成功返回对应回调，失败返回 NULL
* 函数作用: 把定时事件从定时链表删除
******************************************************************************************/
void *TimedEventQ::delTimedEvent(unsigned int token)
{
    void *retHandler = NULL;
    list<EventUnit *>::iterator iter;

    pthread_mutex_lock(&listMutex);

    for (iter=mList.begin(); iter!=mList.end(); ++iter)
    {
        if ((*iter)->idToken == token)
            break;
    }

    if (iter != mList.end())
    {
#if 0   // period event may need signal for end
        if ((*iter)->type == PERIOD_CONDITION) {
            pthread_cond_t *_condition = \
                static_cast<pthread_cond_t *>(entry->handler);
            pthread_cond_signal(_condition);
        } else if ((*iter)->type == PERIOD_SEMAPHORE) {
            sem_t *_semaphore = \
                static_cast<sem_t *>(entry->handler);
            sem_post(_semaphore);
        }
#endif
        delete (*iter);
        retHandler = (*iter)->handler;

        mList.erase(iter);
    }

    pthread_mutex_unlock(&listMutex);

    return retHandler;
}

/******************************************************************************************
* 函数名称: isEmpty
* 参数描述: (无)
*
* 返回值: false非空，true为空
* 函数作用:判断定时链表有无定时事件
******************************************************************************************/
bool TimedEventQ::isEmpty()
{
    bool retVal = false;

    pthread_mutex_lock(&listMutex);
    retVal = mList.empty();
    pthread_mutex_unlock(&listMutex);

    return retVal;
}

/******************************************************************************************
* 函数名称: dump
* 参数描述: (无)
*
* 返回值: 返回定时链表中定时事件个数
* 函数作用: 导出整个定时链表信息
******************************************************************************************/
void TimedEventQ::dump(void *stream)
{
    ioStream *_plogs = (ioStream *)stream;

    if (_plogs == NULL) {
        return;
    }

    list<EventUnit *>::iterator iter;
    struct timeval timeNow;
    struct timeval tvRet;

    pthread_mutex_lock(&listMutex);

    if (!mList.empty()) {
        gettimeofday(&timeNow, 0);
        for (iter=mList.begin(); iter!=mList.end(); ++iter)
        {
            ioStreamPrint(_plogs, "[%06d/%d] -> ", (*iter)->idToken,
                    time_minus((*iter)->tvAbsolute, timeNow));
        }

        ioStreamSeek(_plogs, -3, SEEK_CUR);
    }

    pthread_mutex_unlock(&listMutex);

    /*if (stream == NULL) {
        ioStreamPutc('\0', _plogs);
        shell_print("%s", _buffer);
    } else */ {
        ioStreamPutc('\n', _plogs);
    }
}

#if  0
class my_timeout : public IAbsTimerHandler
{
    virtual int handleAbsTimeOut()
    {
        struct timeval timeNow;

        gettimeofday(&timeNow, 0);
        printf("slot %d timeout ~~ %d/%d\n", token,
            (int)timeNow.tv_sec, (int)timeNow.tv_usec);
    }
public:
    int token;
};

int main()
{
    int i;
    int ret;
    IAbsTimerHandler *ph;
    my_timeout events[10];

    AbsTimerList thisList;

    thisList.dump();

    for (i=0; i<5; i++)
    {
        ret = thisList.addTimerCallback((i+1)*5, 0, &events[i]);
        events[i].token = ret;
    }

    thisList.dump();
    ph = thisList.delTimerAlarm(3);
    printf("ph = %p\n", ph);
    sleep(1);

    thisList.dump();
    ph = thisList.delTimerAlarm(1);
    printf("ph = %p\n", ph);
    sleep(1);

    while(!thisList.isEmpty())
    {
        thisList.dump();
        thisList.handleAlarmEvent();
        sleep(1);
    }

    return 0;
}
#endif

