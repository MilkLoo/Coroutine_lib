/*
 - File Name: scheduler.cpp
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Thu 10 Oct 2024 03:40:34 PM CST
 */

#include "scheduler.h"
namespace Hourglass
{
static thread_local Scheduler* t_scheduler = nullptr;

Scheduler* Scheduler::GetThis()
{
    return t_scheduler;
}

void Scheduler::SetThis()
{
    t_scheduler = this;
}

Scheduler::Scheduler(size_t threads,bool use_caller,const std::string& name):s_useCaller(use_caller),s_name(name)
{
    assert(threads > 0 && Scheduler::GetThis() == nullptr);
    SetThis();
    Thread::SetName(name);
    if(use_caller)
    {
	threads--;
	Coroutine::getCoroutine();
	s_schedulerCoroutine.reset(new Coroutine(std::bind(&Scheduler::run,this),0,this));
	Coroutine::setSchedulerCortinue(s_schedulerCoroutine.get());
	s_rootThread = Thread::GetThreadID();
	s_threadIDs.push_back(s_rootThread);
    }
    s_threadCount = threads;
}

Scheduler::~Scheduler()
{
    assert(stopping()==true);
    if(GetThis() == this)
    {
	t_scheduler = nullptr;
    }
}

void Scheduler::start()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if(s_stopping)
    {
	std::cerr << "Scheduler is stopped!" << std::endl;
	return;
    }
    assert(s_threads.empty());
    s_threads.resize(s_threadCount);
    for(size_t i = 0;i < s_threadCount;i++)
    {
	s_threads[i].reset(new Thread(std::bind(&Scheduler::run,this),s_name + "_" + std::to_string(i)));
	s_threadIDs.push_back(s_threads[i]->getID());
    }
}

void Scheduler::run()
{
    int thread_id = Thread::GetThreadID();
    SetThis();
    if(thread_id != s_rootThread)
    {
	Coroutine::getCoroutine();
    }
    std::shared_ptr<Coroutine> idle_Coroutine = std::make_shared<Coroutine>(std::bind(&Scheduler::idle,this));
    SchedulerTask task;
    while(true)
    {
	task.reset();
	bool tickle_me = false;
	{
	    std::lock_guard<std::mutex> lock(s_mutex);
	    auto it = s_tasks.begin();
	    while(it != s_tasks.end())
	    {
		if(it->thread != -1 && it->thread != thread_id)
		{
		    it++;
		    tickle_me = true;
		    continue;
		}
		assert(it->coroutine || it->func);
		task = *it;
		s_tasks.erase(it);
		s_activateThreadCount++;
		break;
	    }
	    tickle_me = tickle_me || (it != s_tasks.end());
	}
	if(tickle_me)
	{
	    tickle();
	}

	if(task.coroutine)
	{
	    {
		std::lock_guard<std::mutex> lock(task.coroutine->c_mutex);
		if(task.coroutine->getState() != Coroutine::TERM)
		{
		    task.coroutine->resume();
		}
	    }
	    s_activateThreadCount--;
	    task.reset();
	}
	else if(task.func)
	{
	    std::shared_ptr<Coroutine> func_cor = std::make_shared<Coroutine>(task.func);
	    {
		std::lock_guard<std::mutex> lock(func_cor->c_mutex);
		func_cor->resume();
	    }
	    s_activateThreadCount--;
	    task.reset();
	}
	else
	{
	    if(idle_Coroutine->getState() == Coroutine::TERM)
	    {
		break;
	    }
	    s_idleThreadCount++;
	    idle_Coroutine->resume();
	    s_idleThreadCount--;
	}
    }
}

void Scheduler::stop()
{
    if(stopping())
    {
	return;
    }
    s_stopping = true;
    if(s_useCaller){assert(GetThis() == this);}
    else{assert(GetThis() != this);}
    for(size_t i = 0;i < s_threadCount;i++)
    {
	tickle();
    }
    if(s_schedulerCoroutine){tickle();}
    if(s_schedulerCoroutine){s_schedulerCoroutine->resume();}
    std::vector<std::shared_ptr<Thread>> thrs;
    {
	std::lock_guard<std::mutex> lock(s_mutex);
	thrs.swap(s_threads);
    }
    for(auto &i:thrs)
    {
	i->join();
    }
}

void Scheduler::tickle(){}

void Scheduler::idle()
{
    while(!stopping())
    {
	sleep(1);
	Coroutine::getCoroutine()->yield();
    }
}

bool Scheduler::stopping()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_stopping && s_tasks.empty() && s_activateThreadCount == 0;
}
}
