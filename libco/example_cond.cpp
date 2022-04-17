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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include "co_routine.h"

#include "co_routine_inner.h"

using namespace std;

/**
* 本实例是对条件变量的展示，其作用类似于pthread_cond_wait
*/
struct stTask_t
{
	int id;
};
struct stEnv_t
{
	stCoCond_t* cond;
	queue<stTask_t*> task_queue;
};
void* Producer(void* args)
{
	co_enable_hook_sys();
	stEnv_t* env=  (stEnv_t*)args;
	int id = 0;
	while (true)
	{
		stTask_t* task = (stTask_t*)calloc(1, sizeof(stTask_t));
		task->id = id++;
		env->task_queue.push(task);
		printf("\n******* %s.%d: produce task %d\n", __func__, __LINE__, task->id);
		co_cond_signal(env->cond); // 似乎重复放入了一个timeout 事件；
		poll(NULL, 0, 5000);
	}
	return NULL;
}
void* Consumer(void* args)
{
	co_enable_hook_sys();
	stEnv_t* env = (stEnv_t*)args;
	// consumer会一直消费，直到为空后，调用co_cond_timedwait切出协程，等待再次不为空
	while (true)
	{
		if (env->task_queue.empty())
		{
			co_cond_timedwait(env->cond, -1);
			continue;
		}
		stTask_t* task = env->task_queue.front();
		env->task_queue.pop();
		printf("\n======= %s.%d:consume task %d\n", __func__, __LINE__, task->id);
		free(task);
	}
	return NULL;
}


// void check_env(stCoCond_t* cond)
// {
// 	// 队列从中取出一个等待项，进行唤醒

// 	if (cond->head)
// 	stCoCondItem_t * sp = co_cond_pop( cond );
// 	if( !sp ) 
// 	{
// 		printf("%s.%d: sp is null \n", __func__, __LINE__);
// 	}
// 	else
// 	{
// 		printf("%s.%d sp is not null", __func__, __LINE__);
// 	}
// }

int main()
{
	stEnv_t* env = new stEnv_t;
	env->cond = co_cond_alloc();

	stCoRoutine_t* consumer_routine;
	co_create(&consumer_routine, NULL, Consumer, env);
	co_resume(consumer_routine);

	stCoRoutine_t* producer_routine;
	co_create(&producer_routine, NULL, Producer, env);
	co_resume(producer_routine);
	
	co_eventloop(co_get_epoll_ct(), NULL, NULL);
	return 0;
}
