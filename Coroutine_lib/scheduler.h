/*
 - File Name: scheduler.h
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Thu 10 Oct 2024 10:27:28 AM CST
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "coroutine.h"
#include "thread.h"
#include <vector>
#include <mutex>
#include <string>

namespace Hourglass
{
class Scheduler
{
private:
    //调度器的名字
    std::string s_name;
    //互斥锁
    std::mutex s_mutex;
    //线程池
    std::vector<std::shared_ptr<Thread>> s_threads;
    
    //调度任务
    struct SchedulerTask
    {
	std::shared_ptr<Coroutine> coroutine;
	std::function<void()> func;
	int thread;// 指定任务需要运行的线程id
	
	// 初始化构造函数 无参构造
	SchedulerTask()
	{
	    coroutine = nullptr;
	    func = nullptr;
	    thread = -1;
	}

	// 有参构造 函数重载
	SchedulerTask(std::shared_ptr<Coroutine> cp, int thr)
	{
	    coroutine = cp;
	    thread = thr;
	}

	SchedulerTask(std::shared_ptr<Coroutine>* cp, int thr)
	{
	    coroutine.swap(*cp);
	    thread = thr;
	}

	SchedulerTask(std::function<void()> function, int thr)
	{
	    func = function;
	    thread = thr;
	}

	SchedulerTask(std::function<void()> *function, int thr)
	{
	    func.swap(*function);
	    thread = thr;
	}

	void reset()
	{
	    coroutine = nullptr;
	    func = nullptr;
	    thread = -1;
	}
    };

    //任务队列
    std::vector<SchedulerTask> s_tasks;
    //存储工作线程的线程id
    std::vector<int> s_threadIDs;
    //需要额外创建的线程数
    size_t s_threadCount = 0;
    //活跃的线程数
    std::atomic<size_t> s_activateThreadCount = {0};
    //空闲的线程数
    std::atomic<size_t> s_idleThreadCount = {0};
    
    //主线程是否用作工作线程
    bool s_useCaller;
    //如果是，需要创建额外的调度协程
    std::shared_ptr<Coroutine> s_schedulerCoroutine;
    //如果是，记录主线程的id
    int s_rootThread = -1;
    //是否正在关闭
    bool s_stopping = false;

protected:
    // 设置正在运行的调度器
    void SetThis();
    // 线程函数
    virtual void run();
    // 空闲协程函数
    virtual void idle();
    // 是否可以关闭
    virtual bool stopping();

    virtual void tickle();

    bool hasIdleThreads(){return s_idleThreadCount > 0;};
    
public:
    // 构造函数
    Scheduler(size_t threads = -1, bool use_caller = true, const std::string& name="Scheduler");
    virtual ~Scheduler();
    //获取调度器的名称
    const std::string& getName() const {return s_name;};
    //获取正在运行的调度器
    static Scheduler* GetThis();

    template <class CoroutineOrFunc>
    void schedulerLock(CoroutineOrFunc cf, int thread=-1)
    {
	bool need_tickle;
	{
	    std::lock_guard<std::mutex> lock(s_mutex);
	    need_tickle = s_tasks.empty();
	    SchedulerTask task(cf,thread);
	    if(task.coroutine || task.func)
	    {
		s_tasks.push_back(task);
	    }
	}

	if(need_tickle){tickle();}
    }
    

    virtual void start();
    virtual void stop();
};
}
#endif
