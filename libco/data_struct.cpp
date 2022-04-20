#include "data_struct.h"

int stTimeoutItem_t::s_id = 0;

void stPoll_t::init_poll_items(struct pollfd fds[], nfds_t nfds, stCoRoutine_t* self)
{
    // Init pPollItems: PollItems 与 是否是共享栈有什么关系 why?
    printf("%s.%d: nfds: %u,  self->cIsShareStack: %d \n", 
            __func__, __LINE__, nfds, int(self->cIsShareStack));

    int default_poll_count = 2;

    if( nfds < default_poll_count && !self->cIsShareStack)
    {
        printf("%s.%d not share stack, pPollItems set to arr \n", __func__, __LINE__);
        // 如果监听的描述符只有1个或者0个， 并且目前的不是共享栈模型0
        this->pPollItems = (stPollItem_t*)malloc(default_poll_count * sizeof( stPollItem_t ) );
        memset(this->pPollItems, 0, default_poll_count * sizeof(stPollItem_t) );
    }	
    else
    {
        printf("%s.%d use share stack, or monitor more than 2 fids, malloc new space %d for pPollItems \n", 
                __func__, __LINE__, nfds * sizeof( stPollItem_t ));
        // 如果监听的描述符在2个以上，或者协程本身采用共享栈, 
        this->pPollItems = (stPollItem_t*)malloc(nfds * sizeof( stPollItem_t ) );
        memset(this->pPollItems, 0, nfds * sizeof(stPollItem_t) );
    }
    
    
}

/*
 *   EPOLLPRI 		POLLPRI    // There is urgent data to read.  
 *   EPOLLMSG 		POLLMSG
 *
 *   				POLLREMOVE
 *   				POLLRDHUP
 *   				POLLNVAL
 *
 * */
static uint32_t PollEvent2Epoll( short events )
{
	uint32_t e = 0;	
	if( events & POLLIN ) 	e |= EPOLLIN;
	if( events & POLLOUT )  e |= EPOLLOUT;
	if( events & POLLHUP ) 	e |= EPOLLHUP;
	if( events & POLLERR )	e |= EPOLLERR;
	if( events & POLLRDNORM ) e |= EPOLLRDNORM;
	if( events & POLLWRNORM ) e |= EPOLLWRNORM;
	return e;
}


int stPoll_t::add_poll_items(struct pollfd fds[], const nfds_t nfds, const int epfd, const int timeout,  OnPreparePfn_t prepare_func, poll_pfn_t pollfunc)
{
	//2. add epoll
	for(nfds_t i=0;i<nfds;i++)
	{
		// 将事件添加到epoll中
		this->pPollItems[i].pSelf = this->fds + i;
		this->pPollItems[i].pPoll = this;

		// 设置一个预处理的callback
		// 这个函数会在事件active的时候首先触发
		this->pPollItems[i].pfnPrepare = prepare_func; 

		struct epoll_event &ev = this->pPollItems[i].stEvent;

		// 如果大于-1，说明要监听fd的相关事件了
		// 否则就是个timeout事件
		if( fds[i].fd > -1 )
		{
			// 这个相当于是个userdata, 当事件触发的时候，可以根据这个指针找到之前的数据
			ev.data.ptr = this->pPollItems + i;

			// 将poll的事件类型转化为epoll
			ev.events = PollEvent2Epoll( fds[i].events );

			// 将fd添加入epoll中
			int ret = co_epoll_ctl( epfd, EPOLL_CTL_ADD, fds[i].fd, &ev );

			if (ret < 0 && errno == EPERM && nfds == 1 && pollfunc != NULL)
			{
                printf("************** %s.%d [Error] ret %d \n", ret, __func__, __LINE__);
                free( this->pPollItems );
                this->pPollItems = NULL;               
				free(this->fds);
				
                pollfunc(fds, nfds, timeout); // 使用最原生的poll函数
				
				return 0;
			}
		}
		//if fail,the timeout would work
	}
    return 1;
}

// int stPoll_t::add_timeout_items( stCoEpoll_t *ctx, struct pollfd fds[], const nfds_t nfds, const int epfd, const int timeout,  
//                         OnPreparePfn_t prepare_func, poll_pfn_t pollfunc)
// {
// 	// 获取当前时间
// 	unsigned long long now = GetTickMS();

// 	this->ullExpireTime = now + timeout;	
	
// 	// 将其添加到超时链表中
// 	int ret = AddTimeout(ctx->pTimeout, this, now );

// 	printf("%s.%d add args to timeout list, ctx->pTimeout: %s \n", 
// 			__func__, __LINE__, ctx->pTimeout->str().c_str());

// 	// 如果出错了
// 	if( ret != 0 )
// 	{
// 		co_log_err("CO_ERR: AddTimeout ret %d now %lld timeout %d this->ullExpireTime %lld",
// 				ret,now,timeout,this->ullExpireTime);
// 		errno = EINVAL;

// 		free( this->pPollItems );
// 		this->pPollItems = NULL;		
// 		free(this->fds);
// 		return 0;
// 	}
// }                        