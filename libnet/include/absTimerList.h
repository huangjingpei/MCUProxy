/******************************************************************************************
* 文件名称: absTimerList.h
* 语言：C++
*
* 描述: absTimer 本生不带线程，只提供一种管理手段，调用 handleAlarmEvent 的线程如果正好
*       遇到超时事件，该线程就会调用相应回调对象 IAbsTimerHandler 调用者。采用这种方式支持
*       多线程，可以使得即便线程数很多调度复杂的情况下，只要有相当数量的线程调用
*       handleAlarmEvent 定时仍可以保持比较准确
******************************************************************************************/
#ifndef ABS_TIMER_LIST_H__
#define ABS_TIMER_LIST_H__


#define  REFLECT_TIME_HANDLER(cls, var)   \
    class TimeHandler : public IAbsTimerHandler { \
    public: \
        TimeHandler(cls *target) \
            : mTarget(target) { \
        } \
        virtual int handleTimeout() { \
            if (mTarget) { \
                return mTarget->handleTimeout(); \
            } else { \
                return -1; \
            } \
        } \
    private: \
        cls *mTarget; \
        TimeHandler(const TimeHandler &); \
        TimeHandler &operator=(const TimeHandler &); \
    }; \
    friend class TimeHandler; \
    TimeHandler var; \
    int handleTimeout();


#if defined(__cplusplus)
#include <list>
#include <sys/time.h>
#include <stdint.h>

using std::list;

// 定时事件回调
class IAbsTimerHandler
{
public:
    IAbsTimerHandler()          {};
    virtual ~IAbsTimerHandler() {};

    virtual int handleTimeout() = 0;
};

class TimedEventQ
{
public:
    enum EventType{
        TIMED_CALLBACK,
        TIMED_RECYCLE,
        PERIOD_CONDITION,
        PERIOD_SEMAPHORE,
    };

    TimedEventQ();
    ~TimedEventQ();

    /******************************************************************************************
    * 函数名称: addTimerCallback
    * 参数描述:
    * @seconds: 定时秒数(相对当前)
    * @useconds: 定时微秒数(相对当前)
    * @callback: 定时回调
    *
    * 函数名称: addTimerCallback
    * 参数描述:
    * @absTime: 定时秒数(绝对数值)
    * @callback: 定时回调
    *
    * 返回值: 成功返回定时链表中定时事件 ID，否则返回-1
    * 函数作用: 把数据加入到定时链表中
    ******************************************************************************************/
    int addLaterEvent(uint32_t mseconds, EventType type, void *handler);
    int addAbsTimedEvent(struct timeval absTime, EventType type, void *handler);
    int addPeriodEvent(uint32_t mseconds, EventType type, void *handler);

    /******************************************************************************************
    * 函数名称: delTimerAlarm
    * 参数描述:
    * @token: addTimerCallback 返回的定时事件 ID
    *
    * 返回值: 成功返回对应回调，失败返回 NULL
    * 函数作用: 把定时事件从定时链表删除
    ******************************************************************************************/
    void *delTimedEvent(unsigned int token);

    /******************************************************************************************
    * 函数名称: isEmpty
    * 参数描述: (无)
    *
    * 返回值: false非空，true为空
    * 函数作用:判断定时链表有无定时事件
    ******************************************************************************************/
    bool isEmpty();

    /******************************************************************************************
    * 函数名称: dump
    * 参数描述: (无)
    *
    * 返回值: 返回定时链表中定时事件个数
    * 函数作用: 导出整个定时链表信息
    ******************************************************************************************/
    void dump(void *stream);

    /******************************************************************************************
    * 函数名称: handleAlarmEvent
    * 参数描述: (无)
    *
    * 返回值: 无
    * 函数作用: 查看定时链表中首个时间单元的时间是否用尽，用尽则进行回调处理
    ******************************************************************************************/
    void handleAlarmEvent();

    /******************************************************************************************
    * 函数名称: timeToNextAlarmEvent
    * 参数描述:
    * @seconds: 下一次定时事件秒数(相对当前)
    * @useconds: 下一次定时事件微秒数(相对当前)
    *
    * 函数名称: timeToNextAlarmEvent
    * 参数描述:
    * @absTime: 下一次定时事件时间(绝对数值)
    *
    * 返回值: 成功返回0，否则返回-1
    * 函数作用: 返回定时链表中下一次事件发生的间隔时间
    ******************************************************************************************/
    int timeToNextEvent(int& seconds, int& useconds);
    int abstimeToNextEvent(struct timeval& absTime);
protected:
    // 定时单元类型
    struct EventUnit
    {
        struct timeval  tvAbsolute; // 设定的间隔时间

        EventType   type;
        void        *handler;

        uint32_t    idToken;
        uint32_t    period;
    };

    void addEventInternal(EventUnit *unit);

    unsigned int    listToken;
    struct timeval  tvLastSync;

    pthread_mutex_t listMutex;
    list<EventUnit*> mList;
};

#endif // defined(__cplusplus)

#endif

