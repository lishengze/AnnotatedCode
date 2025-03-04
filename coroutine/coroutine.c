#include "coroutine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#if __APPLE__ && __MACH__
	#include <sys/ucontext.h>
#else 
	#include <ucontext.h>
#endif 

#define STACK_SIZE (1024*1024)
#define DEFAULT_COROUTINE 16

struct coroutine;

/**
* 协程调度器
*/
struct schedule {
	char 			stack[STACK_SIZE];	// 运行时栈

	ucontext_t 		main_co_context; // 主协程的上下文
	int 			alive_co_count;  // 当前存活的协程个数
	int 			max_co_numb;     // 协程管理器的当前最大容量，即可以同时支持多少个协程。如果不够了，则进行扩容
	int 			cur_co_id; 		 // 正在运行的协程ID
	struct coroutine **co; 			 // 一个一维数组，用于存放协程 
};

/*
* 协程
*/
struct coroutine {
	coroutine_func 		func; // 协程所用的函数
	void *				func_params_;  // 协程参数 -- 存储函数参数

	ucontext_t 			ctx; // 协程上下文 ？

	ptrdiff_t 			mem_allocated; 	 // 已经分配的内存大小 ？
	ptrdiff_t 			mem_used; // 当前协程运行时栈，保存起来后的大小 ？
	
	char *				stack; // 当前协程的保存起来的运行时栈 ？

	struct schedule* 	sch; // 该协程所属的调度器
	int 			 	status;	// 协程当前的状态
};

/*
* 新建一个协程
* 主要做的也是分配内存及赋初值
*/
struct coroutine * 
_co_new(struct schedule *S , coroutine_func func, void *func_params_) {
	struct coroutine * co = malloc(sizeof(*co));
	co->func = func;
	co->func_params_ = func_params_;
	co->sch = S;
	co->mem_allocated = 0;
	co->mem_used = 0;
	co->status = COROUTINE_READY; // 默认的最初状态都是COROUTINE_READY
	co->stack = NULL;
	return co;
}

/**
* 删除一个协程
*/
void
_co_delete(struct coroutine *co) {
	free(co->stack);
	free(co);
}

/**
* 创建一个协程调度器 
*/
struct schedule * 
coroutine_open(void) {
	// 这里做的主要就是分配内存，同时赋初值
	struct schedule *S = malloc(sizeof(*S));
	S->alive_co_count = 0;
	S->max_co_numb = DEFAULT_COROUTINE;
	S->cur_co_id = -1;
	S->co = malloc(sizeof(struct coroutine *) * S->max_co_numb);
	memset(S->co, 0, sizeof(struct coroutine *) * S->max_co_numb);
	return S;
}

/**
* 关闭一个协程调度器，同时清理掉其负责管理的
* @param S 将此调度器关闭
*/
void 
coroutine_close(struct schedule *S) {
	int i;
	// 关闭掉每一个协程
	for (i=0;i<S->max_co_numb;i++) {
		struct coroutine * co = S->co[i];

		if (co) {
			_co_delete(co);
		}
	}

	// 释放掉
	free(S->co);
	S->co = NULL;
	free(S);
}

void schedule_expand()
{

}

/**
* 创建一个协程对象
* @param S 该协程所属的调度器
* @param func 该协程函数执行体
* @param func_params_ func的参数
* @return 新建的协程的ID
*/
int 
coroutine_new(struct schedule *S, coroutine_func func, void *func_params_) {
	struct coroutine *co = _co_new(S, func , func_params_);
	if (S->alive_co_count >= S->max_co_numb) {
		// 如果目前协程的数量已经大于调度器的容量，那么进行扩容
		int id = S->max_co_numb;	// 新的协程的id直接为当前容量的大小
		// 扩容的方式为，扩大为当前容量的2倍，这种方式和Hashmap的扩容略像
		S->co = realloc(S->co, S->max_co_numb * 2 * sizeof(struct coroutine *));
		// 初始化内存
		memset(S->co + S->max_co_numb , 0 , sizeof(struct coroutine *) * S->max_co_numb);
		//将协程放入调度器
		S->co[S->max_co_numb] = co;
		// 将容量扩大为两倍
		S->max_co_numb *= 2;
		// 尚未结束运行的协程的个数 
		++S->alive_co_count; 
		return id;
	} else {
		// 如果目前协程的数量小于调度器的容量，则取一个为NULL的位置，放入新的协程
		int i;
		for (i=0;i<S->max_co_numb;i++) {
			/* 
			 * 为什么不 i%S->max_co_numb,而是要从nco+i开始呢 
			 * 这其实也算是一种优化策略吧，因为前nco有很大概率都非NULL的，直接跳过去更好
			*/
			int id = (i+S->alive_co_count) % S->max_co_numb;
			if (S->co[id] == NULL) {
				S->co[id] = co;
				++S->alive_co_count;
				return id;
			}
		}
	}
	assert(0);
	return -1;
}

/*
 * 通过low32和hi32 拼出了struct schedule的指针，这里为什么要用这种方式，而不是直接传struct schedule*呢？
 * 因为makecontext的函数指针的参数是int可变列表，在64位下，一个int没法承载一个指针
*/
static void
mainfunc(uint32_t low32, uint32_t hi32) {
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
	struct schedule *S = (struct schedule *)ptr;

	int id = S->cur_co_id;
	struct coroutine *C = S->co[id];
	C->func(S,C->func_params_);	// 中间有可能会有不断的yield
	_co_delete(C);
	S->co[id] = NULL;
	--S->alive_co_count;
	S->cur_co_id = -1;
}

/**
* 切换到对应协程中执行
* 
* @param S 协程调度器
* @param id 协程ID
*/
void 
coroutine_resume(struct schedule * S, int id) {
	assert(S->cur_co_id == -1);
	assert(id >=0 && id < S->max_co_numb);

    // 取出协程
	struct coroutine *C = S->co[id];
	if (C == NULL)
		return;

	int status = C->status;
	switch(status) {
	case COROUTINE_READY:
	    //初始化ucontext_t结构体,将当前的上下文放到C->ctx里面
		getcontext(&C->ctx);
		// 将当前协程的运行时栈的栈顶设置为S->stack
		// 每个协程都这么设置，这就是所谓的共享栈。（注意，这里是栈顶）
		C->ctx.uc_stack.ss_sp = S->stack; // 每个协程真正上下文环境指向了调度器中的 stack;
		C->ctx.uc_stack.ss_size = STACK_SIZE;
		C->ctx.uc_link = &S->main_co_context; // 如果协程执行完，将切换到主协程中执行
		S->cur_co_id = id;
		C->status = COROUTINE_RUNNING;

		// 设置执行C->ctx函数, 并将S作为参数传进去
		uintptr_t ptr = (uintptr_t)S;
		makecontext(&C->ctx, (void (*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr>>32));

		// 将当前的上下文放入S->main中，并将C->ctx的上下文替换到当前上下文
		swapcontext(&S->main_co_context, &C->ctx);
		break;
	case COROUTINE_SUSPEND:
	    // 将协程所保存的栈的内容，拷贝到当前运行时栈中
		// 其中C->size在yield时有保存
		memcpy(S->stack + STACK_SIZE - C->mem_used, C->stack, C->mem_used);
		S->cur_co_id = id;
		C->status = COROUTINE_RUNNING;
		swapcontext(&S->main_co_context, &C->ctx);
		break;
	default:
		assert(0);
	}
}

/*
* 将本协程的栈内容保存起来
* @top 栈顶 
* 
*/
static void
_save_stack(struct coroutine *C, char *stack_top) {
	// 这个dummy很关键，是求取整个栈的关键
	// 这个非常经典，涉及到linux的内存分布，栈是从高地址向低地址扩展，因此
	// S->stack + STACK_SIZE就是运行时栈的栈底
	// dummy，此时在栈中，肯定是位于最底的位置的，即栈顶
	// top - &dummy 即整个栈的容量
	char dummy = 0; // 在栈上最新使用一块内存，这就是当前的栈顶了；
	char* stack_bottom = &dummy;

	assert(stack_top - stack_bottom <= STACK_SIZE);
	if (C->mem_allocated < stack_top - stack_bottom) {
		free(C->stack);
		C->mem_allocated = stack_top-stack_bottom;
		C->stack = malloc(C->mem_allocated);
	}
	C->mem_used = stack_top - stack_bottom;
	memcpy(C->stack, stack_bottom, C->mem_used);
}

/**
* 将当前正在运行的协程让出，切换到主协程上
* @param S 协程调度器
*/
void
coroutine_yield(struct schedule * S) {
	// 取出当前正在运行的协程
	int id = S->cur_co_id;
	assert(id >= 0);

	struct coroutine * C = S->co[id];
	assert((char *)&C > S->stack);

	// 将当前运行的协程的栈内容保存起来
	char* stack_top = S->stack + STACK_SIZE;
	_save_stack(C, stack_top);
	
	// 将当前栈的状态改为 挂起
	C->status = COROUTINE_SUSPEND;
	S->cur_co_id = -1;

	// 所以这里可以看到，只能从协程切换到主协程中, 这样就能挂起当前协程？
	swapcontext(&C->ctx , &S->main_co_context);   
}

int 
coroutine_status(struct schedule * S, int id) {
	assert(id>=0 && id < S->max_co_numb);
	if (S->co[id] == NULL) {
		return COROUTINE_DEAD;
	}
	return S->co[id]->status;
}

/**
* 获取正在运行的协程的ID
* 
* @param S 协程调度器
* @return 协程ID 
*/
int 
coroutine_running(struct schedule * S) {
	return S->cur_co_id;
}

