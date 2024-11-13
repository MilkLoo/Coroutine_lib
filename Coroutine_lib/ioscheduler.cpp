/*
 - File Name: ioscheduler.cpp
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Fri 25 Oct 2024 05:48:59 PM CST
 */

#include "ioscheduler.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>

namespace Hourglass
{
IOManager::FdContext::EventContext& IOManager::FdContext::getEventContext(Event event)
{
    assert(event == READ || event == WRITE);
    switch(event)
    {
	case READ:
	     return read;
	case WRITE:
	     return write; 
    }
    throw std::invalid_argument("Unspported event type!");
}

void IOManager::FdContext::resetEventContext(EventContext & ectx)
{
    ectx.scheduler = nullptr;
    ectx.coroutine.reset();
    ectx.func = nullptr;
}

void IOManager::FdContext::triggerEvent(Event event)
{
    assert(events & event);
    events = (Event)(events & ~event);
    EventContext& ctx = getEventContext(event);
    if(ctx.func)
    {
	ctx.scheduler->schedulerLock(&ctx.func);
    }
    else
    {
	ctx.scheduler->schedulerLock(&ctx.coroutine);
    }
    resetEventContext(ctx);
    return ;
}

IOManager::IOManager(size_t threads, bool use_caller,const std::string& name):Scheduler(threads,use_caller,name),TimerManager()
{
    m_epfd = epoll_create(5000);
    assert(m_epfd > 0);
    int rt = pipe(m_tickleFds);
    assert(!rt);
    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];
    rt = fcntl(m_tickleFds[0],F_SETFL,O_NONBLOCK);
    assert(!rt);
    rt = epoll_ctl(m_epfd,EPOLL_CTL_ADD,m_tickleFds[0],&event);
    assert(!rt);
    contextResize(32);
    start();
}

void IOManager::contextResize(size_t size)
{
    m_fdcontext.resize(size);
    for(size_t i = 0;i < m_fdcontext.size();++i)
    {
	if(m_fdcontext[i] == nullptr)
	{
	    m_fdcontext[i] = new FdContext();
	    m_fdcontext[i]->fd = i;
	}
    }
}

IOManager::~IOManager()
{
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);
    for(size_t i = 0;i < m_fdcontext.size();++i)
    {
	if(m_fdcontext[i])
	{
	    delete m_fdcontext[i];
	}
    }
}

int IOManager::addEvent(int fd,Event event,std::function<void()> func)
{
    FdContext *fd_ctx = nullptr;
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    if((int)m_fdcontext.size() > fd)
    {
	fd_ctx = m_fdcontext[fd];
	read_lock.unlock();
    }
    else
    {
	read_lock.unlock();
	std::unique_lock<std::shared_mutex> write_lock(m_mutex);
	contextResize(fd * 1.5);
	fd_ctx = m_fdcontext[fd];
    }
    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
    if(fd_ctx->events & event)
    {
	return -1;
    }
    int op = fd_ctx->events? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLIN | fd_ctx->events | event;
    epevent.data.ptr = fd_ctx;
    int rt = epoll_ctl(m_epfd,op,fd,&epevent);
    if(rt)
    {
	std::cerr << "addEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
	return -1;
    }
    ++m_pendingEventCount;
    fd_ctx->events = (Event)(fd_ctx->events | event);
    FdContext::EventContext& event_ctx = fd_ctx->getEventContext(event);
    assert(!event_ctx.scheduler && !event_ctx.coroutine && !event_ctx.func);
    event_ctx.scheduler = Scheduler::GetThis();
    if(func)
    {
	event_ctx.func.swap(func);
    }
    else
    {
	event_ctx.coroutine = Coroutine::getCoroutine();
	assert(event_ctx.coroutine->getState() == Coroutine::RUNNING);
    }
    return 0;
}

bool IOManager::delEvent(int fd,Event event)
{
    FdContext* fd_ctx = nullptr;
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    if((int)m_fdcontext.size() > fd)
    {
	fd_ctx = m_fdcontext[fd];
	read_lock.unlock();
    }
    else
    {
	read_lock.unlock();
	return false;
    }
    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
    if(!(fd_ctx->events & event))
    {
	return false;
    }
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;
    int rt = epoll_ctl(m_epfd,op,fd,&epevent);
    if(rt)
    {
	std::cerr << "delEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
	return -1;
    }
    --m_pendingEventCount;
    fd_ctx->events = new_events;
    FdContext::EventContext& event_ctx = fd_ctx->getEventContext(event);
    fd_ctx->resetEventContext(event_ctx);
    return true;
}

bool IOManager::cancelEvent(int fd, Event event)
{
    FdContext* fd_ctx = nullptr;
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    if((int)m_fdcontext.size() > fd)
    {
	fd_ctx = m_fdcontext[fd];
	read_lock.unlock();
    }
    else
    {
	read_lock.unlock();
	return false;
    }
    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
    if(!(fd_ctx->events & event))
    {
	return false;
    }
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;
    int rt = epoll_ctl(m_epfd,op,fd,&epevent);
    if(rt)
    {
	std::cerr << "cacelEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
	return -1;
    }
    --m_pendingEventCount;
    fd_ctx->triggerEvent(event);
    return true;
}

bool IOManager::cancelAll(int fd)
{
    FdContext *fd_ctx = nullptr;
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    if((int)m_fdcontext.size() > fd)
    {
	fd_ctx = m_fdcontext[fd];
	read_lock.unlock();
    }
    else
    {
	read_lock.unlock();
	return false;
    }
    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
    if(!fd_ctx->events)
    {
	return false;
    }
    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;
    int rt = epoll_ctl(m_epfd,op,fd,&epevent);
    if(rt)
    {
	std::cerr << "IOManager::epoll_ctl failed: " << strerror(errno) << std::endl;
	return -1;
    }
    if(fd_ctx->events & READ)
    {
	fd_ctx->triggerEvent(READ);
	--m_pendingEventCount;
    }
    if(fd_ctx->events & WRITE)
    {
	fd_ctx->triggerEvent(WRITE);
	--m_pendingEventCount;
    }
    assert(fd_ctx->events == 0);
    return true;
}

IOManager* IOManager::GetIOManager()
{
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle()
{
    if(!hasIdleThreads())
    {
	return ;
    }
    int rt = write(m_tickleFds[1],"T",1);
    assert(rt == 1);
}

bool IOManager::stopping()
{
    uint64_t timeout = getNextTimer();
    return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
}

void IOManager::idle() 
{
    static const uint64_t MAX_EVENTS = 256;
    std::unique_ptr<epoll_event[]> events(new epoll_event[MAX_EVENTS]);
    while(true)
    {
	if(stopping())
	{
	    break;
	}
	int rt = 0;
	while(true)
	{
	    static const uint64_t MAX_TIMEOUT = 5000;
	    uint64_t next_timeout = getNextTimer();
	    next_timeout = std::min(next_timeout,MAX_TIMEOUT);
	    rt = epoll_wait(m_epfd,events.get(),MAX_EVENTS,(int)next_timeout);
	    if(rt < 0 && errno == EINTR)
	    {
		continue;
	    }
	    else
	    {
		break;
	    }
	}
	std::vector<std::function<void()>> funcs;
	listExpiredFunc(funcs);
	if(!funcs.empty())
	{
	    for(const auto& func : funcs)
	    {
		schedulerLock(func);
	    }
	    funcs.clear();
	}
	for(int i = 0;i < rt;++i)
	{
	    epoll_event& event = events[i];
	    if(event.data.fd == m_tickleFds[0])
	    {
		uint8_t dummy[256];
		while(read(m_tickleFds[0],dummy,sizeof(dummy)) > 0);
		continue;
	    }
	    FdContext *fd_ctx = (FdContext*)event.data.ptr;
	    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
	    if(event.events & (EPOLLERR | EPOLLHUP))
	    {
		event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
	    }
	    int real_events = NONE;
	    if(event.events & EPOLLIN)
	    {
		real_events |= READ;
	    }
	    if(event.events & EPOLLOUT)
	    {
		real_events |= WRITE;
	    }
	    if((fd_ctx->events & real_events) == NONE)
	    {
		continue;
	    }
	    int left_events = (fd_ctx->events & ~real_events);
	    int op = left_events? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
	    event.events = EPOLLET | left_events;
	    int rt2 = epoll_ctl(m_epfd,op,fd_ctx->fd,&event);
	    if(rt2)
	    {
		std::cerr << "idle::epoll_ctl failed: " << strerror(errno) <<std::endl;
		continue;
	    }
	    if(real_events & READ)
	    {
		fd_ctx->triggerEvent(READ);
		--m_pendingEventCount;
	    }
	    if(real_events & WRITE)
	    {
		fd_ctx->triggerEvent(WRITE);
		--m_pendingEventCount;
	    }
	}
	Coroutine::getCoroutine()->yield();
    }
}

void IOManager::onTimerInsertAtFront()
{
    tickle();
}
}
