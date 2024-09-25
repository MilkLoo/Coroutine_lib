/*
 - File Name: thread.cpp
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Wed 25 Sep 2024 05:15:06 PM CST
 */

#include "thread.h"
#include <sys/syscall.h>
#include <iostream>
#include <unistd.h>

namespace Hourglass
{
static thread_local Thread * t_thread = nullptr;
static thread_local std::string t_thread_name = "NULL";

void Threadsem::wait()
{
    std::unique_lock<std::mutex> lock(m_tex);
    while(count == 0)
    {
	cv.wait(lock);
    }
    count--;
}

void Threadsem::signal()
{
    std::unique_lock<std::mutex> lock(m_tex);
    count++;
    cv.notify_one();
}

Thread::Thread(std::function<void()> func, const std::string& name):thread_func(func),thread_name(name)
{
    int thread_t = pthread_create(&thread_m,nullptr,&Thread::run,this);
    if(thread_t)
    {
	std::cerr << "Thread create failed!\n";
	throw std::logic_error("pthread_create error");
    }
    threadsem.wait();
}

Thread::~Thread()
{
    if(thread_m)
    {
	pthread_detach(thread_m);
	thread_m = 0;
    }
}

void Thread::join()
{
    if(thread_m)
    {
	int thread_t = pthread_join(thread_m,nullptr);
	if(thread_t)
	{
	    std::cerr << "pthread join failed, thrad_t=" << thread_t << "name = " << thread_name << std::endl;
	    throw std::logic_error("pthread_join error!");
	}
	thread_m = 0;
    }
}

void* Thread::run(void* arg)
{
    Thread* thread = (Thread*)arg;
    t_thread = thread;
    t_thread_name = thread->thread_name;
    thread->thread_id = GetThreadID();
    pthread_setname_np(pthread_self(),thread->thread_name.substr(0,15).c_str());
    std::function<void()> func;
    func.swap(thread->thread_func);
    thread->threadsem.signal();
    func();
    return 0;
}

pid_t Thread::GetThreadID()
{
    return syscall(SYS_gettid);
}

Thread* Thread::GetThis()
{
    return t_thread;
}

const std::string& Thread::GetName()
{
    return t_thread_name;
}

void Thread::SetName(const std::string& name)
{
    if(t_thread)
    {
	t_thread->thread_name = name;
    }
    t_thread_name = name;
}
}

