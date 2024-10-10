/*
 - File Name: coroutine.h
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Tue 24 Sep 2024 08:41:17 PM CST
 */
#ifndef _COROUTINE_H_
#define _COROUTINE_H_

#include <memory>
#include <functional>
#include <cassert>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <ucontext.h>
#include <iostream>

namespace Hourglass{
    // enable_shared_from 允许一个类（通常是shared_ptr管理的类）安全的生成指向自身（this）的std::shared_ptr的实例．
    // 意味着当使用shared_ptr来管理对象的生命周期时，想要在对象的成员函数中获取对象的shared_ptr的实例，直接用this来代替． 
    // 造一个share_ptr会出错．多个shared_ptr管理同一个对象可能会造成重析构的问题．
    // 也就是与已有的共享一个引用计数．
class Coroutine: public std::enable_shared_from_this<Coroutine>
{
public:
    enum State{RUNNING,READY,TERM};
protected:
    // 基本 Data
    // 协程ＩＤ
    uint64_t coroutineID = 0;
    // 协程状态　简化
    State coroutineState = READY;
    // 上下文结构
    ucontext_t coroutineCT;
    // 栈地址
    void* coroutineStack = nullptr;
    // 栈大小
    uint32_t coroutineStackSize = 0;
    // 协程入口函数
    std::function<void()> coroutineFunc;
    // 无参构造
    Coroutine();
    //是否会参加调度协程的调度
    bool runInSchedulerCor;
 
public:
    /* 获取属性相关的成员 attributes */
    // 获取协程ID
    uint64_t getID() const {return coroutineID;}
    // 获取协程状态
    State getState() const {return coroutineState;}
    // 获取当前运行的协程ID
    static uint64_t getCorID();

    /* 协程行为相关的成员　behavior */
    // 无参构造　由于不想直接通过类进行创建实例，通过方法直接进行构造，转化为私有化．
    // 有参构造
    Coroutine(std::function<void()> func, size_t stack_size=0,bool runinscheduler=true);
    // 析构
    ~Coroutine();
    // 利用类对getCoroutine方法进行调用无参构造，提供一个用户接口
    static std::shared_ptr<Coroutine> getCoroutine();
    // 想将创建的协程对象在线程中使用，想在每个线程创建时使得协程对象都有各自的实例，线程之间的协程实例不受影响．
    // 并且线程结束时，任何协程都会被清理，生命周期同线程，线程启动时分配，线程结束时自动释放．
    // 利用setCoroutine将产生的对象赋给被thread_local限定的协程对象．
    static void setCoroutine(Coroutine* cor);
    // 恢复执行
    void resume();
    // 让出执行
    void yield();
    // 重用一个协程
    void reset(std::function<void()> func);
    // 想将协程操作再封装成一个成员，当协程进行操作时，直接绑定函数就可以了
    static void mainFunc();
    // 设置调度协程，默认主协程
    static void setSchedulerCortinue(Coroutine* cor);
    std::mutex c_mutex;
};
}
#endif
