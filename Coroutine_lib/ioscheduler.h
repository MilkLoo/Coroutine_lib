/*
 - File Name: ioscheduler.h
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Fri 25 Oct 2024 10:57:37 AM CST
 */

#ifndef _IOSCHEDULER_H_
#define _IOSCHEDULER_H_
#include "scheduler.h"
#include "timer.h"

namespace Hourglass
{
class IOManager : public Scheduler , public TimerManager
{
public:
    // 事件定义
    enum Event
    {
	NONE = 0x0,
	READ = 0x1,
	WRITE = 0x4
    };

    IOManager(size_t threads = 1, bool use_caller = true,const std::string& name = "IOManager");
    ~IOManager();
    int addEvent(int fd,Event event,std::function<void()> func = nullptr);
    bool delEvent(int fd,Event event);
    bool cancelEvent(int fd,Event event);  
    bool cancelAll(int fd);
    static IOManager* GetIOManager();

protected:
    void tickle() override;
    bool stopping() override;
    void idle() override;
    void onTimerInsertAtFront() override;
    void contextResize(size_t size);

private: 
    struct FdContext
    {
	struct EventContext
	{
	    Scheduler *scheduler = nullptr;
	    std::shared_ptr<Coroutine> coroutine;
	    std::function<void()> func;
	};
	EventContext read;
	EventContext write;
	int fd = 0;
	Event events = NONE;
	std::mutex mutex;
	EventContext& getEventContext(Event event);
	void resetEventContext(EventContext & ectx);
	void triggerEvent(Event event);
    };
       
    int m_epfd = 0;
    int m_tickleFds[2];
    std::atomic<size_t> m_pendingEventCount = {0};
    std::shared_mutex m_mutex;
    std::vector<FdContext*> m_fdcontext;
};
}
#endif
