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
#include <errno.h>
#include <vector>
#include <set>
#include <unistd.h>
#include <thread>
#include <chrono>

#include "data_struct.h"

#ifdef __FreeBSD__
#include <cstring>
#endif

using namespace std;

struct task_t
{
	stCoRoutine_t *		co;
	int 				fd;
	struct sockaddr_in 	addr;

	string              ip;
	int                 port;
};

static int SetNonBlock(int iSock)
{
    int iFlags;

    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;
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

static int CreateTcpSocket(const unsigned short shPort  = 0 ,const char *pszIP  = "*" ,bool bReuse  = false )
{
	printf("%s.%d create tcp socket %s:%u \n", __func__, __LINE__, pszIP, shPort);

	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

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

				printf("%s.%d:[Error] create tcp socket %s:%u Failed! Close Fd: %d \n", __func__, __LINE__, pszIP, shPort, fd);
				close(fd);
				return -1;
			}
		}
	}

	printf("%s.%d create tcp socket %s:%u , fd: %d, over!\n", __func__, __LINE__, pszIP, shPort, fd);

	return fd;
}


static int CreateTcpSocket(struct sockaddr_in& addr ,bool bReuse  = false )
{
	printf("%s.%d create tcp socket\n", __func__, __LINE__);

	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if( fd >= 0 )
	{
		if(bReuse)
		{
			int nReuseAddr = 1;
			setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&nReuseAddr,sizeof(nReuseAddr));
		}

		int ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
		if( ret != 0)
		{

			printf("%s.%d:[Error] create tcp socket Failed! Close Fd: %d \n", __func__, __LINE__, fd);
			close(fd);
			return -1;
		}		
	}

	printf("%s.%d create tcp socket fd: %d over!\n", __func__, __LINE__, fd);

	return fd;
}



void init_task(vector<task_t> & task_vec) 
{
	printf("\n%s.%d: init_task \n", __func__, __LINE__);
	// 创建 socket;
	for(size_t i=0;i<task_vec.size();i++)
	{
		// int fd = CreateTcpSocket(task_vec[i].addr);

		int fd = CreateTcpSocket(task_vec[i].port, task_vec[i].ip.c_str());

		// printf("1\n");

		if (fd <= 0) 
		{
			printf("%s.%d:[Error] CreateTcpSocket failed!\n", __func__, __LINE__);
			continue;
		}

		SetNonBlock( fd );  // ?

		// printf("2\n");
		task_vec[i].fd = fd;

		struct sockaddr* address = (struct sockaddr*)&task_vec[i].addr;

		// printf("3\n");

		int ret = connect(fd, address, sizeof(task_vec[i].addr)); //? 为什么现在就连接； 

		printf("%s.%d: connnect  co.info: %p, ret: %d \n", __func__, __LINE__, co_self(), ret);

		// printf("co %s connect i %ld,  ret %d errno %d (%s)\n",
		// 		co_self()->str().c_str(), i,  ret, errno, strerror(errno));
	}

	printf("%s.%d: init_task over!\n", __func__, __LINE__);
}

struct pollfd * init_pollfd(vector<task_t> & task_vec)
{
	struct pollfd *pf = (struct pollfd*)calloc(1, sizeof(struct pollfd) * task_vec.size());
	for(size_t i=0;i<task_vec.size();i++)
	{
		pf[i].fd = task_vec[i].fd;
		pf[i].events = (POLLOUT | POLLERR | POLLHUP);
	}
	return pf;
}

string get_event_str(short int event) 
{
	if (event == POLLOUT) return "POLLOUT";
	if (event == POLLERR) return "POLLERR";
	if (event == POLLHUP) return "POLLHUP";
	return "unknown";
}

void poll_task(vector<task_t> &task_vec, struct pollfd *pf, size_t& iWaitCnt, set<int>& setRaiseFds)
{
	printf("\n%s.%d poll_task iWaitCnt:%u, task_vec.size: %u \n", __func__, __LINE__, iWaitCnt, task_vec.size());
	for(;;)
	{
		int ret = poll(pf, iWaitCnt, 1500);
		printf("%s.%d: co.info: %p poll wait %ld ret %d\n", __func__, __LINE__, co_self(), iWaitCnt, ret);

		for(int i=0;i<ret;i++)
		{
			printf("%s.%d: co %p fire fd %d revents %s \n",__func__, __LINE__, 
					co_self(), pf[i].fd, get_event_str(pf[i].revents).c_str());

			setRaiseFds.insert( pf[i].fd );
		}

		if( setRaiseFds.size() == task_vec.size())
		{
			printf("%s.%d: setRaiseFds.size() == task_vec.size() all events over \n", __func__, __LINE__);
			break;
		}
		if( ret <= 0 )
		{
			printf("%s.%d: [over] ret <= 0 \n", __func__, __LINE__);
			break;
		}

		// collect left events;
		iWaitCnt = 0;
		for(size_t i=0;i<task_vec.size();i++)
		{
			if( setRaiseFds.find( task_vec[i].fd ) == setRaiseFds.end() )
			{
				pf[ iWaitCnt ].fd = task_vec[i].fd;
				pf[ iWaitCnt ].events = ( POLLOUT | POLLERR | POLLHUP );
				++iWaitCnt;
			}
		}


		printf("%s.%d new iWaitCnt: %d\n", __func__, __LINE__, iWaitCnt);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	printf("%s.%d poll_task over! \n", __func__, __LINE__);
}

void close_task(vector<task_t> &task_vec)
{
	printf("\n%s.%d close task! \n", __func__, __LINE__);

	for(size_t i=0;i<task_vec.size();i++)
	{
		close( task_vec[i].fd );
		task_vec[i].fd = -1;
	}
}

static void *poll_routine( void *arg )
{
	if (co_self())
	{
		printf("\n---------------- %s.%d poll_routine, co.info: %s start! --------------\n", __func__, __LINE__, co_self()->str().c_str());
	}
	else
	{
		printf("\n---------------- %s.%d poll_routine, main init start! --------------\n", __func__, __LINE__);
	}
	

	co_enable_hook_sys();

	vector<task_t> &task_vec = *(vector<task_t>*)arg;

	init_task(task_vec);

	// 为每个 task 创建pollfd
	struct pollfd *pf = init_pollfd(task_vec);

	set<int> setRaiseFds;
	size_t iWaitCnt = task_vec.size();

	poll_task(task_vec, pf, iWaitCnt, setRaiseFds);

	close_task(task_vec);

	printf("%s.%d co %p task cnt %ld fire %ld\n",__func__, __LINE__, 
			co_self(), task_vec.size(), setRaiseFds.size()) ;

	return 0;
}

int main(int argc,char *argv[])
{
	vector<string> address = {"127.0.0.1", "127.0.0.1"};
	vector<int> port = {8112, 8113};
	int ip_count = 2;

	vector<task_t> task_vec;

	for(int i=0; i<ip_count; i++)
	{
		task_t task = { 0 };
		task.ip = address[i];
		task.port = port[i];

		printf("%s.%d: %s:%d \n", __func__, __LINE__, address[i].c_str(), port[i]);
		SetAddr(address[i].c_str(), port[i], task.addr);
		task_vec.push_back( task );
	}

//------------------------------------------------------------------------------------
	printf("\n --------------------- start main -------------------\n");
	vector<task_t> v2 = task_vec;
	poll_routine( &v2 );

	printf("\n--------------------- start routine -------------------\n");
	int work_routine_count = 2;
	for(int i=0; i<work_routine_count; i++)
	{
		printf("\n\n************** routine %d start *************\n",i);

		stCoRoutine_t *co = 0;
		vector<task_t> *work_task = new vector<task_t>();
		*work_task = task_vec;
		co_create(&co, NULL, poll_routine, work_task);
		
		co_resume(co);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	co_eventloop(co_get_epoll_ct(), 0, 0);

	return 0;
}
//./example_poll 127.0.0.1 12365 127.0.0.1 12222 192.168.1.1 1000 192.168.1.2 1111

