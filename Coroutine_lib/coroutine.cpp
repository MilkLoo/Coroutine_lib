/*
 - File Name: coroutine.cpp
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Tue 24 Sep 2024 09:55:04 PM CST
 */

#include "coroutine.h"

namespace Hourglass
{

static thread_local Coroutine* t_coroutine = nullptr;
static thread_local std::shared_ptr<Coroutine> t_thread_coroutine = nullptr;
static thread_local Coroutine* t_scheduler_cor = nullptr;
static std::atomic<uint64_t> t_coroutine_id{0};
static std::atomic<uint64_t> t_coroutine_count{0};

uint64_t Coroutine::getCorID()
{
    if(t_coroutine)
    {
	return t_coroutine->getID();
    }
    return (uint64_t) - 1;
}

void Coroutine::setCoroutine(Coroutine* cor)
{
    t_coroutine = cor;
}

void Coroutine::setSchedulerCortinue(Coroutine* cor)
{
    t_scheduler_cor = cor;
}

Coroutine::Coroutine()
{
    setCoroutine(this);
    coroutineState = RUNNING;
    if(getcontext(&coroutineCT))
    {
	std::cerr << "Coroutine() Failed!\n";
	pthread_exit(NULL);
    }
    coroutineID = t_coroutine_id++;
    t_coroutine_count++;
}

Coroutine::Coroutine(std::function<void()> func, size_t stack_size,bool runinscheduler):coroutineFunc(func),runInSchedulerCor(runinscheduler)
{
    setCoroutine(this);
    coroutineState = READY;
    coroutineStackSize = stack_size ? stack_size : 128000;
    coroutineStack = malloc(coroutineStackSize);
    if(getcontext(&coroutineCT))
    {
	std::cerr << "Coroutine(func,stack_size) Failed!\n";
	pthread_exit(NULL);
    }
    coroutineCT.uc_link = nullptr;
    coroutineCT.uc_stack.ss_sp = coroutineStack;
    coroutineCT.uc_stack.ss_size = coroutineStackSize;
    makecontext(&coroutineCT,&Coroutine::mainFunc,0);
    coroutineID = t_coroutine_id++;
    t_coroutine_count++;
}

Coroutine::~Coroutine()
{
    t_coroutine_count--;
    if(coroutineStack)
    {
	free(coroutineStack);
    }
}

void Coroutine::resume()
{
    assert(coroutineState == READY);
    coroutineState = RUNNING;
    if(runInSchedulerCor)
    {
	setCoroutine(this);
	if(swapcontext(&(t_scheduler_cor->coroutineCT),&coroutineCT))
	{
	    std::cerr << "resume() to t_scheduler_coroutine failed!\n";
	    pthread_exit(NULL);
	}
    }
    else 
    {
	setCoroutine(this);
	if(swapcontext(&(t_thread_coroutine->coroutineCT),&coroutineCT))
	{
	    std::cerr << "resume() failed!\n";
	    pthread_exit(NULL);
	}
    }
}

void Coroutine::yield()
{
    assert(coroutineState == RUNNING || coroutineState == TERM);
    if(coroutineState != TERM)
    {
	coroutineState = READY;
    }
    if(runInSchedulerCor)
    {
	setCoroutine(this);
	if(swapcontext(&coroutineCT,&(t_scheduler_cor->coroutineCT)))
	{
	    std::cerr << "yield() t_scheduler_cor failed!\n";
	    pthread_exit(NULL);
	}
    }
    else
    {
	setCoroutine(this);
	if(swapcontext(&coroutineCT,&(t_thread_coroutine->coroutineCT)))
	{
	    std::cerr << "yield() falied!\n";
	    pthread_exit(NULL);
	}
    }
}

void Coroutine::reset(std::function<void()> func)
{
    assert(coroutineStack != nullptr && coroutineState == TERM);
    coroutineState = READY;
    coroutineFunc = func;
    if(getcontext(&coroutineCT))
    {
	std::cerr << "reset() failed!\n";
	pthread_exit(NULL);
    }
    coroutineCT.uc_link = nullptr;
    coroutineCT.uc_stack.ss_sp = coroutineStack;
    coroutineCT.uc_stack.ss_size = coroutineStackSize;
    makecontext(&coroutineCT, &Coroutine::mainFunc, 0);
}

std::shared_ptr<Coroutine> Coroutine::getCoroutine()
{
    if(t_coroutine != nullptr)
    {
	return t_coroutine->shared_from_this();
    }
    std::shared_ptr<Coroutine> main_cor(new Coroutine());
    t_thread_coroutine = main_cor;
    t_scheduler_cor = main_cor.get();//除非主动设置，主协程默认是调度协程
    // assert 条件为假抛出错误！
    assert(t_coroutine == main_cor.get());
    return t_coroutine->shared_from_this();
}

void Coroutine::mainFunc()
{
    std::shared_ptr<Coroutine> cur = getCoroutine();
    assert(cur != nullptr);
    // shared_ptr 重载了-> 运算符的
    cur->coroutineFunc();
    cur->coroutineFunc = nullptr;
    cur->coroutineState = TERM;
    auto ori_ptr = cur.get();
    cur.reset();// shared_ptr引用计数减１
    ori_ptr->yield();
}
}
