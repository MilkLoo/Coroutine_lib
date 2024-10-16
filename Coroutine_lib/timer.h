/*
 - File Name: timer.h
 - Author: YXC
 - Mail: 2395611610@qq.com 
 - Created Time: Tue 15 Oct 2024 11:52:12 AM CST
 */

#include <memory>
#include <vector>
#include <set>
#include <shared_mutex>
#include <assert.h>
#include <functional>
#include <mutex>

namespace Hourglass
{
class TimerManager;

class Timer:public std::enable_shared_from_this<Timer>
{
    friend class TimerManager;

private:
    Timer(uint64_t ms,std::function<void()> func,bool recurring,TimerManager* manager);
    bool m_recurring = false;
    uint64_t m_ms = 0;
    std::chrono::time_point<std::chrono::system_clock> m_next;
    std::function<void()> m_func;
    TimerManager* m_manager = nullptr;
    struct Comparator
    {
	bool operator()(const std::shared_ptr<Timer>& lhs,const std::shared_ptr<Timer>& rhs) const;
    };

public:
    bool cancel();
    bool refresh();
    bool reset(uint64_t ms,bool from_now);
};

class TimerManager
{
    friend class Timer;
private:
    bool detecClockRollover();
    std::shared_mutex m_mutex;
    std::set<std::shared_ptr<Timer>,Timer::Comparator> m_timers;
    bool m_tickled = false;
    std::chrono::time_point<std::chrono::system_clock> m_previousTime;

protected:
    virtual void onTimerInsertAtFront() {};
    void addTimer(std::shared_ptr<Timer> timer);
public:
    TimerManager();
    virtual ~TimerManager();
    std::shared_ptr<Timer> addTimer(uint64_t ms, std::function<void()> func, bool recurring = false);
    std::shared_ptr<Timer> addConditionTimer(uint64_t ms,std::function<void()> func,std::weak_ptr<void> weak_cond,bool recurring=false);
    uint64_t getNextTimer();
    void listExpiredFunc(std::vector<std::function<void()>>& funcs);
    bool hasTimer();
};
}
