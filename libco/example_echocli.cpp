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

#include <errno.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stack>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <thread>

#include "data_struct.h"

using namespace std;
struct stEndPoint
{
	char *ip;
	unsigned short int port;
};

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

static int iSuccCnt = 0;
static int iFailCnt = 0;
static int iTime = 0;

void AddSuccCnt()
{
	int now = time(NULL);
	if (now >iTime)
	{
		printf("time %d Succ Cnt %d Fail Cnt %d\n", iTime, iSuccCnt, iFailCnt);
		iTime = now;
		iSuccCnt = 0;
		iFailCnt = 0;
	}
	else
	{
		iSuccCnt++;
	}
}
void AddFailCnt()
{
	int now = time(NULL);
	if (now >iTime)
	{
		printf("time %d Succ Cnt %d Fail Cnt %d\n", iTime, iSuccCnt, iFailCnt);
		iTime = now;
		iSuccCnt = 0;
		iFailCnt = 0;
	}
	else
	{
		iFailCnt++;
	}
}


void init_socket(stEndPoint* endpoint, 	int& fd, int& ret)
{
	
	fd = socket(PF_INET, SOCK_STREAM, 0);

	printf("%s.%d init fd: %d \n", __func__, __LINE__, fd);

	struct sockaddr_in addr;
	SetAddr(endpoint->ip, endpoint->port, addr);

	printf("1\n");

	ret = connect(fd,(struct sockaddr*)&addr,sizeof(addr));	// connect 啥？ why?

	printf("2\n");

	printf("%s.%d connect fd: %d, ret: %d \n",  __func__, __LINE__, fd, ret);
}


void write_data(int fd, string& str, int& ret, char* buf) 
{
	printf("%s.%d: write data \n", __func__, __LINE__);
	ret = write( fd, str.c_str(), 8);

	printf("%s.%d:[Write] co %p ret %d errno %d (%s)\n", 
			__func__, __LINE__, co_self(), ret, errno, strerror(errno));		

	if ( ret > 0 )
	{
		ret = read( fd, buf, sizeof(buf) );

		printf("%s.%d:[Read] co %p ret %d errno %d (%s)\n", __func__, __LINE__, 
				co_self(), ret,errno,strerror(errno));	

		if ( ret <= 0 )
		{
			//printf("co %p read ret %d errno %d (%s)\n",
			//		co_self(), ret,errno,strerror(errno));
			close(fd);
			fd = -1;
			AddFailCnt();
		}
		else
		{
			//printf("echo %s fd %d\n", buf,fd);
			AddSuccCnt();
		}
	}
	else
	{
		printf("%s.%d co %p write ret %d errno %d (%s)\n", __func__, __LINE__, 
				co_self(), ret,errno,strerror(errno));
		close(fd);
		fd = -1;
		AddFailCnt();
	}
}

void poll_event(int& fd, int& ret)
{
	printf("%s.%d init socket fd: %d, ret: %d, errno: %s \n",
				__func__, __LINE__, fd, ret, strerror(errno));

	string co_str = co_self() ? co_self()->str() : "null";

	printf("%s.%d: poll event start, co.info: %s \n", __func__, __LINE__, co_str.c_str());

	struct pollfd pf = { 0 };
	pf.fd = fd;
	pf.events = (POLLOUT|POLLERR|POLLHUP);

	// add fd event and timeout event;
	co_poll(co_get_epoll_ct(), &pf, 1, 2000);

	printf("%s.%d: poll event over, co.info: %s \n", __func__, __LINE__, co_str.c_str());
}

int check_fd(int& fd)
{
	printf("%s.%d: check_fd! \n", __func__, __LINE__);

	int ret = 0;
	int error = 0;
	uint32_t socklen = sizeof(error);
	errno = 0;

	ret = getsockopt(fd, SOL_SOCKET, SO_ERROR,(void *)&error,  &socklen);

	printf("%s.%d getsockopt fd: %d, ret: %d, errno: %s \n",
				__func__, __LINE__, fd, ret, strerror(errno));

	if ( ret == -1 ) 
	{       
		printf("getsockopt ERROR ret %d %d:%s\n", ret, errno, strerror(errno));
		close(fd);
		fd = -1;
		AddFailCnt();
		return 0;
	}       
	if ( error ) 
	{       
		errno = error;
		printf("connect ERROR ret %d %d:%s\n", error, errno, strerror(errno));
		close(fd);
		fd = -1;
		AddFailCnt();
		return 0;
	}    

	return 1;
}

static void *readwrite_routine( void *arg )
{
	string co_str = "null";
	if (co_self()) co_str = co_self()->str();

	printf("------------ readwrite_routine start --------------- \n");
	printf("%s.%d: co.info: %s\n", __func__, __LINE__, co_str.c_str());
	co_enable_hook_sys();

	stEndPoint *endpoint = (stEndPoint *)arg;
	string str = NanoTimeStr();
	char buf[ 1024 * 16 ];
	int fd = -1;
	int ret = 0;
	for(;;)
	{
		if ( fd < 0 )
		{
			init_socket(endpoint, fd, ret);
						
			if ( errno == EALREADY || errno == EINPROGRESS )
			{       
				poll_event(fd, ret);

				if (!check_fd(fd)) continue;
			} 	  			
		}
		write_data(fd, str, ret, buf);
	}
	return 0;
}

int main(int argc,char *argv[])
{
	stEndPoint endpoint;
	// endpoint.ip = argv[1];
	// endpoint.port = atoi(argv[2]);
	// int cnt = atoi( argv[3] );
	// int process_count = atoi( argv[4] );

	endpoint.ip = "127.0.0.1";
	endpoint.port = 8911;
	int co_count = 1;
	int process_count = 1;	
	
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction( SIGPIPE, &sa, NULL );
	
	for(int k=0;k<process_count;k++)
	{

		// pid_t pid = fork();
		// if( pid > 0 )
		// {
		// 	printf("%s.%d fork pid: %d\n", __func__, __LINE__, pid);
		// 	continue;
		// }
		// else if( pid < 0 )
		// {
		// 	printf("%s.%d fork pid: %d  Failed!\n", __func__, __LINE__, pid);
		// 	break;
		// }

		for(int i=0;i<co_count;i++)
		{
			stCoRoutine_t *co = 0;
			co_create(&co, NULL, readwrite_routine, &endpoint);
			printf("%s.%d: create co: %s \n", __func__, __LINE__, co->str().c_str());
			co_resume( co );
		}
		co_eventloop(co_get_epoll_ct(), 0, 0);

		std::this_thread::sleep_for(std::chrono::milliseconds(4000));

		// exit(1);
	}
	return 0;
}
/*./example_echosvr 127.0.0.1 10000 100 50*/
