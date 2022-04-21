/*
* Tencent is pleased to support the open source community by making Libco available.

* Copyright (C) 2014 THL A29 Limited, a Tencent company. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License"); 
* you may not use this file except in compliance with the License. 
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, 
* software distributed under the License is distributed on an "AS IS" BASIS, 
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
* See the License for the specific language governing permissions and 
* limitations under the License.
*/



#include "co_routine.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <stack>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include "data_struct.h"

#ifdef __FreeBSD__
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#endif

using namespace std;
struct task_t
{
	stCoRoutine_t *	co;
	int 			fd;

	string str()
	{
		string result = "co.info: ";
		result += co ? co->str() : "null";
		result += ", fd: " + std::to_string(fd);

		return result;
	}
};

static stack<task_t*> g_readwrite;
static int g_listen_fd = -1;
static int SetNonBlock(int iSock)
{
    int iFlags;

    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;
}

static void *readwrite_routine( void *arg )
{
	printf("\n------------ %s.%d: readwrite_routine ----------\n", __func__, __LINE__);
	co_enable_hook_sys();

	task_t *co = (task_t*)arg;

	string co_str = "null";
	if (co->co)
	{
		co_str = co->co->str();
	}

	printf("%s.%d: task.fd: %d, task.co: %s \n", __func__, __LINE__, co->fd, co_str.c_str());

	char buf[ 1024 * 16 ];
	for(;;)
	{
		if( -1 == co->fd )
		{
			printf("%s.%d: \n", __func__, __LINE__);
			// push进去
			g_readwrite.push( co );
			// 切出
			co_yield_ct();
			continue;
		}

		int fd = co->fd;
		co->fd = -1;

		for(;;)
		{
			// 将该fd的可读事件，注册到epoll中
			// co_accept里面libco并没有将其设置为 O_NONBLOCK
			// 是用户主动设置的 O_NONBLOCK
			// 所以read函数不走hook逻辑，需要自行进行poll切出
			struct pollfd pf = { 0 };
			pf.fd = fd;
			pf.events = (POLLIN|POLLERR|POLLHUP);

			
			printf("%s.%d: add fd %d in epoll, add timeout: %d in timelist\n", __func__, __LINE__, fd, 1000);

			co_poll(co_get_epoll_ct(), &pf, 1, 1000);

			// 当超时或者可读事件到达时，进行read。所以read不一定成功，有可能是超时造成的
			int ret = read( fd,buf,sizeof(buf) );

			printf("%s.%d: read fd: %d, buf: %s, ret: %d \n", __func__, __LINE__, fd, buf, ret);

			// 读多少就写多少
			if( ret > 0 )
			{
				ret = write( fd, buf, ret );

				printf("%s.%d: write fd: %d, buf: %s, ret: %d \n", __func__, __LINE__, fd, buf, ret);
			}

			if( ret <= 0 )
			{
				// accept_routine->SetNonBlock(fd) cause EAGAIN, we should continue
				if (errno == EAGAIN)
					continue;
				close( fd );
				break;
			}
		}

	}
	return 0;
}
int co_accept(int fd, struct sockaddr *addr, socklen_t *len );
static void *accept_routine( void * )
{
	co_enable_hook_sys(); 
	string co_str = "null";
	if (co_self())
	{
		co_str = co_self()->str();
	}

	printf("\n---------------%s.%d: accept_routine .co.information: %s ------------\n", __func__, __LINE__, co_str.c_str());

	fflush(stdout);
	for(;;)
	{
		//printf("pid %ld g_readwrite.size %ld\n",getpid(),g_readwrite.size());
		if( g_readwrite.empty() )
		{
			int sleep_millsecs = 1000;
			printf("%s.%d: g_readwrite empty, wait: %d millsecs\n", __func__, __LINE__, sleep_millsecs); //sleep
			struct pollfd pf = { 0 };
			pf.fd = -1;

			// sleep 1秒，等待有空余的协程
			poll(&pf, 1, sleep_millsecs);

			continue;
		}

		struct sockaddr_in addr; //maybe sockaddr_un;
		memset( &addr,0,sizeof(addr) );
		socklen_t len = sizeof(addr);

		// accept
		int fd = co_accept(g_listen_fd, (struct sockaddr *)&addr, &len);

		printf("%s.%d: accept fd: %d\n", __func__, __LINE__, fd);

		printf("[Error] aceept always failed!");
		if( fd < 0 )
		{
			// 意思是，如果accept失败了，没办法，暂时切出去
			struct pollfd pf = { 0 };
			pf.fd = g_listen_fd;
			pf.events = (POLLIN|POLLERR|POLLHUP);

			printf("%s.%d: accept failed , have to swap out! ", __func__, __LINE__);
			co_poll(co_get_epoll_ct(), &pf, 1, 1000);

			continue;
		}

		if( g_readwrite.empty())
		{
			close( fd );
			continue;
		}

		// 这里需要手动将其变成非阻塞的
		SetNonBlock( fd );

		task_t *co = g_readwrite.top();

		printf("%s.%d: top task.info: %s\n", __func__, __LINE__, co->str().c_str());
		co->fd = fd;
		g_readwrite.pop();
		co_resume(co->co);
	}
	return 0;
}

static void SetAddr(const char *pszIP,const unsigned short shPort,struct sockaddr_in &addr)
{
	bzero(&addr,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(shPort);
	int nIP = 0;
	if( !pszIP || '\0' == *pszIP   
	    || 0 == strcmp(pszIP,"0") || 0 == strcmp(pszIP,"0.0.0.0") 
		|| 0 == strcmp(pszIP,"*") 
	  )
	{
		nIP = htonl(INADDR_ANY);
	}
	else
	{
		nIP = inet_addr(pszIP);
	}
	addr.sin_addr.s_addr = nIP;

}

static int CreateTcpSocket(const unsigned short shPort /* = 0 */,const char *pszIP /* = "*" */,bool bReuse /* = false */)
{
	int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
	if( fd >= 0 )
	{
		if(shPort != 0)
		{
			if(bReuse)
			{
				int nReuseAddr = 1;
				setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&nReuseAddr,sizeof(nReuseAddr));
			}
			struct sockaddr_in addr ;
			SetAddr(pszIP,shPort,addr);
			int ret = bind(fd,(struct sockaddr*)&addr,sizeof(addr));
			if( ret != 0)
			{
				close(fd);
				return -1;
			}
		}
	}
	return fd;
}


int main(int argc,char *argv[])
{
	// if(argc<5){
	// 	printf("Usage:\n"
    //            "example_echosvr [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT]\n"
    //            "example_echosvr [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT] -d   # daemonize mode\n");
	// 	return -1;
	// }
	// const char *ip = argv[1];
	// int port = atoi( argv[2] );
	// int co_count = atoi( argv[3] ); // task_count 协程数
	// int process_count = atoi( argv[4] ); // 进程数
	// bool deamonize = argc >= 6 && strcmp(argv[5], "-d") == 0;

	const char *ip = "127.0.0.1";
	int port = 8911;
	int co_count = 1; // task_count 协程数
	int process_count = 1; // 进程数
	bool deamonize = true;	

	g_listen_fd = CreateTcpSocket( port,ip,true );
	int ret = listen(g_listen_fd, 1024);

	if(ret==-1)
	{
		printf("Port %d is in use\n", port);
		return -1;
	}
	printf("listen %d %s:%d\n",g_listen_fd,ip,port);

	SetNonBlock( g_listen_fd );

	for(int k=0;k<process_count;k++)
	{

		pid_t pid = fork();

		if( pid > 0 )
		{
			continue;
		}
		else if( pid < 0 )
		{
			break;
		}

		for(int i=0;i<co_count;i++)
		{
			task_t * task = (task_t*)calloc( 1,sizeof(task_t) );
			// task->fd = -1;

			task->fd = g_listen_fd;

			// 创建一个协程
			co_create( &(task->co),NULL,readwrite_routine,task );

			// 启动协程
			co_resume( task->co );
		}

		printf("\n---------------------%s.%d: start accept_routine -----------\n", __func__, __LINE__);
		// 启动listen协程
		stCoRoutine_t *accept_co = NULL;
		co_create(&accept_co, NULL, accept_routine, 0);
		// 启动协程
		co_resume( accept_co );

		printf("\n");

		// 启动事件循环
		co_eventloop( co_get_epoll_ct(),0,0 );

		exit(0);
	}

	if(!deamonize) wait(NULL);

	return 0;
}

