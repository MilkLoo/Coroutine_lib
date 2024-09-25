/*
 - File Name: thread.h
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Wed 25 Sep 2024 04:55:16 PM CST
 */
#ifndef _THREAD_H_
#define _THREAD_H_

#include <functional>
#include <mutex>
#include <condition_variable>

namespace Hourglass
{
class Threadsem
{
private:
    std::mutex m_tex;
    std::condition_variable cv;
    int count;

public:
    explicit Threadsem(int count_ = 0):count(count_){};
    void wait();
    void signal();
};
class Thread
{
private:
    std::string thread_name;
    std::function<void()> thread_func;
    pid_t thread_id = -1;
    pthread_t thread_m = 0;
    Threadsem threadsem;
    static void* run(void* arg);
public:
    Thread(std::function<void()> func,const std::string& name);
    ~Thread();
    pid_t getID() const {return thread_id;}
    const std::string& getName() const {return thread_name;}
    void join();
    static const std::string& GetName();
    static pid_t GetThreadID();
    static Thread* GetThis();
    static void SetName(const std::string& name);
};
}
#endif

