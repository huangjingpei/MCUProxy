
#ifndef NW_TIME_LIST_H__
#define NW_TIME_LIST_H__

#include "list.h"
#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif


//#define THREAD_SAFE
//#define DEBUG_TIMELIST

typedef struct _alarmEventNode alarmEventNode;
typedef struct _timeListType timeListType;

// 定时事件回调函数
typedef int (*alarmEventHandler)(alarmEventNode *thisEvent);

// 定时单元类型
struct _alarmEventNode
{
// public:
	struct timeval      tvRemain;	// 设定的间隔时间

	alarmEventHandler   cbTimeout;	// 回调函数指针
	void*               cbParam;	// 回调参数

// protected:
    unsigned int        reserved;
	unsigned int        idToken;

// private:
	list_t              list;       // timed_list use, don't modify
};


// 定时链表
struct _timeListType
{
// private
    alarmEventNode  eventHead;
	unsigned int    listToken;

    struct timeval  tvLastSync;
#ifdef THREAD_SAFE
	pthread_mutex_t listMutex;
#endif
};


/******************************************************************************************
* 函数名称: initTimeList
* 参数描述:
* @lst: 定时链表
* 返回值: 成功 =0, 失败 <0
* 函数作用: 初始化一个定时链表
******************************************************************************************/
int initTimeList(timeListType *lst);

/******************************************************************************************
* 函数名称: uninitTimeList
* 参数描述:
* @lst: 定时链表
*
* 返回值:
* 函数作用: 销毁一个定时链表
******************************************************************************************/
void uninitTimeList(timeListType *lst);

/******************************************************************************************
* 函数名称: addAlarmEventNode
* 参数描述:
* @lst: 定时链表
* @newEntry: 加入的定时单元
*
* 返回值: 成功返回定时链表中定时单元序号，否则返回-1
* 函数作用: 把数据加入到定时链表中
******************************************************************************************/
int addAlarmEventNode(timeListType *lst, alarmEventNode *newEntry);

/******************************************************************************************
* 函数名称: delAlarmEventNode
* 参数描述:
* @lst: 定时链表
* @token: 定时链表中时间单元的序号
*
* 返回值: 成功返回时间单元的指针，失败返回 NULL
* 函数作用: 把时间单元从定时链表删除
******************************************************************************************/
alarmEventNode * delAlarmEventNode(timeListType *lst, unsigned int token);

/******************************************************************************************
* 函数名称: isTimeListEmpty
* 参数描述:
＊ @lst: 定时链表
*
* 返回值: 0非空，1为空
* 函数作用:判断定时链表有无定时单元
******************************************************************************************/
int isTimeListEmpty(timeListType *lst);

/******************************************************************************************
* 函数名称: dumpTimeList
* 参数描述:
* @lst: 定时链表
*
* 返回值: 返回定时链表中定时单元个数
* 函数作用: 打印整个定时链表
******************************************************************************************/
int dumpTimeList(timeListType *lst);

/******************************************************************************************
* 函数名称: handleAlarmEvent
* 参数描述:
* @lst: 定时链表
*
* 返回值: 无
* 函数作用: 查看定时链表中首个时间单元的时间是否用尽，用尽则进行回调处理
******************************************************************************************/
void handleAlarmEvent(timeListType *lst);

/******************************************************************************************
* 函数名称: timeToNextAlarmEvent
* 参数描述:
* @lst: 定时链表
*
* 返回值: 成功返回下一次事件发生的间隔时间
* 函数作用: 返回定时链表中下一次事件发生的间隔时间
******************************************************************************************/
struct timeval timeToNextAlarmEvent(timeListType *lst);

#ifdef __cplusplus
};
#endif

#endif

