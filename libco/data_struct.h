#pragma once
#include "co_routine_inner.h"
#include "co_epoll.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>

#include <poll.h>
#include <sys/time.h>
#include <errno.h>

#include <assert.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string>
#include <sstream>

#include <chrono>

using namespace std;

#include <sys/time.h>
#include <sstream>
#include <iomanip>

const long MILLISECONDS_PER_SECOND = 1000;
const long MICROSECONDS_PER_MILLISECOND = 1000;
const long NANOSECONDS_PER_MICROSECOND = 1000;

const long MICROSECONDS_PER_SECOND =
        MICROSECONDS_PER_MILLISECOND * MILLISECONDS_PER_SECOND;
const long NANOSECONDS_PER_MILLISECOND =
        NANOSECONDS_PER_MICROSECOND * MICROSECONDS_PER_MILLISECOND;
const long NANOSECONDS_PER_SECOND =
        NANOSECONDS_PER_MILLISECOND * MILLISECONDS_PER_SECOND;

const int SECONDS_PER_MINUTE = 60;
const int MINUTES_PER_HOUR = 60;
const int HOURS_PER_DAY = 24;
const int SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;

const long MILLISECONDS_PER_MINUTE =
        MILLISECONDS_PER_SECOND * SECONDS_PER_MINUTE;
const long NANOSECONDS_PER_MINUTE = NANOSECONDS_PER_SECOND * SECONDS_PER_MINUTE;
const long NANOSECONDS_PER_HOUR = NANOSECONDS_PER_SECOND * SECONDS_PER_HOUR;
const long NANOSECONDS_PER_DAY = NANOSECONDS_PER_HOUR * HOURS_PER_DAY;

struct stTimeoutItemLink_t;
struct stTimeoutItem_t;
struct stCoEpoll_t;

typedef void (*OnPreparePfn_t)( stTimeoutItem_t *,struct epoll_event &ev, stTimeoutItemLink_t *active );
typedef void (*OnProcessPfn_t)( stTimeoutItem_t *);

typedef int (*poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);

/**
 * dump long time to string with format
 * @param nano nano time in long
 * @param format eg: %Y%m%d-%H:%M:%S
 * @return string-formatted time
 */
inline std::string ToSecondStr(long nano, const char* format="%Y-%m-%d %H:%M:%S") {
    if (nano <= 0)
        return std::string("NULL");
    nano /= NANOSECONDS_PER_SECOND;
    struct tm* dt = {0};
    char buffer[30];
    dt = gmtime(&nano);
    strftime(buffer, sizeof(buffer), format, dt);
    return std::string(buffer);
}

/**
 * dump long time to struct tm
 * @param nano nano time in long
 * @return ctime struct
 */
inline struct tm ToSecondStruct(long nano) {
    time_t sec_num = nano / NANOSECONDS_PER_SECOND;
    return *gmtime(&sec_num);
}

/**
 * util function to utilize NanoTimer
 * @return current nano time in long (unix-timestamp * 1e9 + nano-part)
 */
inline long NanoTime() {
    std::chrono::high_resolution_clock::time_point curtime = std::chrono::high_resolution_clock().now();
    long orin_nanosecs = std::chrono::duration_cast<std::chrono::nanoseconds>(curtime.time_since_epoch()).count();
    return orin_nanosecs;
//    return NanoTimer::getInstance()->getNano();
}

/*

*/
inline std::string NanoTimeStr() {
    long nano_time = NanoTime();
    string time_now = ToSecondStr(nano_time, "%Y-%m-%d %H:%M:%S");
    time_now += "." + std::to_string(nano_time % NANOSECONDS_PER_SECOND);
    return time_now;
}

inline std::string SecTimeStr(std::string time_format="%Y-%m-%d %H:%M:%S") {
    long nano_time = NanoTime();
    string time_now = ToSecondStr(nano_time, time_format.c_str());
    return time_now;
}


/*
* 线程所管理的协程的运行环境
* 一个线程只有一个这个属性
*/
struct stCoRoutineEnv_t
{
	// 这里实际上维护的是个调用栈
	// 最后一位是当前运行的协程，前一位是当前协程的父协程(即，resume该协程的协程)
	// 可以看出来，libco只能支持128层协程的嵌套调用。这个绝对够了
	stCoRoutine_t * pCallStack[ 128 ]; 

	int             iCallStackSize; // 当前调用栈长度

	stCoEpoll_t *   pEpoll;  //主要是epoll，作为协程的调度器

	//for copy stack log lastco and nextco
	stCoRoutine_t*  pending_co;  // question? 
	stCoRoutine_t*  occupy_co;	// question?

    string str() {
        string result = "";

        result += "iCallStackSize: " + std::to_string(iCallStackSize) + "\n";

        if (pending_co) 
        {
            result += "pending_co.str: " + pending_co->str();
        }
        if (occupy_co)
        {
            result += "occupy_co.str: " + occupy_co->str();
        }
        return result;
    }
};

/*
* 超时链表中的一个项
*/
struct stTimeoutItem_t
{
    stTimeoutItem_t() { id = s_id++;}
	enum
	{
		eMaxTimeout = 40 * 1000 //40s
	};

	stTimeoutItem_t *pPrev;	// 前一个元素
	stTimeoutItem_t *pNext; // 后一个元素

	stTimeoutItemLink_t *pLink; // 该链表项所属的链表

	unsigned long long ullExpireTime;

	OnPreparePfn_t pfnPrepare;  // 预处理函数，在eventloop中会被调用
	OnProcessPfn_t pfnProcess;  // 处理函数 在eventloop中会被调用

	void *pArg; // self routine pArg 是pfnPrepare和pfnProcess的参数

	bool bTimeout; // 是否已经超时

    static int s_id;

    int        id;

    string str() const
    {
        string result;

        result = "id: " + std::to_string(id)
                + ", ullExpireTime: " + std::to_string(ullExpireTime)
                + ", bTimeout: " + std::to_string(int(bTimeout));

        if (pArg) 
        {
            stCoRoutine_t *co = (stCoRoutine_t*)(pArg);
            result += ", co.id: " + std::to_string(co->id);
        }

        return result;
    }
};


/*
* 超时链表
*/
struct stTimeoutItemLink_t
{
	stTimeoutItem_t *head;
	stTimeoutItem_t *tail;

    int size() 
    { 
        int result = 0;
        stTimeoutItem_t *lp = head;
        while( lp )
        {
            result++;
            lp = lp->pNext;
        }	
        return result;        
    }

    string str() {
        string result = "size: " + std::to_string(size()) + "\n";

        stTimeoutItem_t *lp = head;
        while( lp )
        {
            result += lp->str() + "\n";
            lp = lp->pNext;
        }

        return result;
    }
};
/*
* 毫秒级的超时管理器
* 使用时间轮实现
* 但是是有限制的，最长超时时间不可以超过iItemSize毫秒
*/
struct stTimeout_t
{
	/*
	   时间轮
	   超时事件数组，总长度为iItemSize,每一项代表1毫秒，为一个链表，代表这个时间所超时的事件。

	   这个数组在使用的过程中，会使用取模的方式，把它当做一个循环数组来使用，虽然并不是用循环链表来实现的
	*/
	stTimeoutItemLink_t *   pItems;
	int                     iItemSize;   // 默认为60*1000

	unsigned long long      ullStart;   //目前的超时管理器最早的时间
	long long               llStartIdx; //目前最早的时间所对应的pItems上的索引

    string str() {
        string result = "pItems.str: " + pItems->str() + ", iItemSize: " + std::to_string(iItemSize)
                      + ", ullStart: " + std::to_string(ullStart) + ", llStartIdx: " + std::to_string(llStartIdx);
        return result;
    }
};


// 自己管理的epoll结构体
struct stCoEpoll_t
{
	int iEpollFd;	// epoll的id

	static const int _EPOLL_SIZE = 1024 * 10;  // 1024*10

	struct stTimeout_t *pTimeout;  // 超时管理器

	struct stTimeoutItemLink_t *pstTimeoutList; // 目前已超时的事件，仅仅作为中转使用，最后会合并到active上

	struct stTimeoutItemLink_t *pstActiveList; // 正在处理的事件

	co_epoll_res *result; 

	std::string str() { 
		std::string result= "iEpollFd: " + std::to_string(iEpollFd) 
                            + "\npstTimeoutList: " + pstTimeoutList->str() 
                            + "pstActiveList: " + pstActiveList->str();

		return result;
	}
};

//int poll(struct pollfd fds[], nfds_t nfds, int timeout);
// { fd,events,revents }
struct stPollItem_t ;

struct stPoll_t : public stTimeoutItem_t 
{
    stPoll_t(const nfds_t& nfds_v, const int& epollfd, OnProcessPfn_t process_func , stCoRoutine_t* co) 
    { 
        memset(this, 0, sizeof(stPoll_t));

        iEpollFd = epollfd;        
        nfds     = nfds_v;       
        fds      = (pollfd*)calloc(nfds, sizeof(pollfd));

        pfnProcess  = process_func;
        pArg        = co;
    }

    void init_poll_items(struct pollfd fds[], nfds_t nfds, stCoRoutine_t* self);

    int add_poll_items(struct pollfd fds[], const nfds_t nfds, const int epfd, const int timeout,  
                        OnPreparePfn_t prepare_func, poll_pfn_t pollfunc);

    // int add_timeout_items( stCoEpoll_t *ctx, struct pollfd fds[], const nfds_t nfds, const int epfd, const int timeout,  
    //                         OnPreparePfn_t prepare_func, poll_pfn_t pollfunc);


	struct pollfd*  fds;
	nfds_t          nfds; // typedef unsigned long int nfds_t;

	stPollItem_t *  pPollItems;  // 其中的 pPollItems
	int iAllEventDetach;         // 标识是否已经处理过了这个对象了
	int iEpollFd;                // epoll instance
	int iRaiseCnt;  // poll的active的事件个数
};

struct stPollItem_t : public stTimeoutItem_t
{
	struct pollfd *     pSelf;
	stPoll_t *          pPoll;

	struct epoll_event  stEvent;
};

/*
* 条件变量
*/
struct stCoCond_t;
struct stCoCondItem_t 
{
	stCoCondItem_t  *pPrev;
	stCoCondItem_t  *pNext;
	stCoCond_t      *pLink;

	stTimeoutItem_t timeout;

    string str()
    {
        string result;

        result = "stCoCondItem_t-timeout: " + timeout.str();

        return result;
    }
};

struct stCoCond_t
{
	stCoCondItem_t *head;
	stCoCondItem_t *tail;

    int size() 
    { 
        int result = 0;
        stCoCondItem_t *lp = head;
        while( lp )
        {
            result++;
            lp = lp->pNext;
        }	
        return result;        
    }

    string str() 
    {
        string result = "";

        stCoCondItem_t *lp = head;
        result +=  "size: " + std::to_string(size()) + "\n";

        while( lp )
        {
            result += lp->str();
            lp = lp->pNext;
        }	
        return result;
    }
};

inline int list_size(stTimeoutItemLink_t* list) 
{
	int result = 0;
	stTimeoutItem_t *lp = list->head;
	while( lp )
	{
		result++;
		lp = lp->pNext;
	}	
	return result;
}

inline int list_size(stCoCond_t* list) 
{
	int result = 0;
	stCoCondItem_t *lp = list->head;
	while( lp )
	{
		result++;
		lp = lp->pNext;
	}	
	return result;
}
