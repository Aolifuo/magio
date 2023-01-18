#ifndef MAGIO_CORE_TIMER_QUEUE_H
#define MAGIO_CORE_TIMER_QUEUE_H

#include <queue>
#include <memory>
#include <chrono>
#include <functional>

#include "magio-v3/core/noncopyable.h"

namespace magio {

using TimerClock = std::chrono::steady_clock;
using TimerTask = std::function<void(bool)>;

struct TimerData {
    TimerData(TimerClock::time_point tp, TimerTask&& f)
        : dead_line(tp), task(std::move(f)) 
    { }

    bool flag = true;
    TimerClock::time_point dead_line;
    TimerTask task;
};

class TimerHandle {
public:
    TimerHandle(const std::shared_ptr<TimerData>& p)
        : pdata_(p) { }

    bool cancel() {
        auto p = pdata_.lock();
        if (p) {
            p->flag = false;
            p->task(false);
            return true;
        }
        return false;
    }

private:
    std::weak_ptr<TimerData> pdata_;
};

struct TimerCompare {
    bool operator()(
        const std::shared_ptr<TimerData>& left, 
        const std::shared_ptr<TimerData>& right
    ) const {
        return left->dead_line > right->dead_line;
    }
};

class TimerQueue: Noncopyable {
public:
    using QueueType = std::priority_queue<
        std::shared_ptr<TimerData>,
        std::vector<std::shared_ptr<TimerData>>,
        TimerCompare
    >;

    ~TimerQueue() {
        while (!timers_.empty()) {
            timers_.top()->task(false);
            timers_.pop();
        }
    }

    void get_expired(std::vector<TimerTask>& result) {
        auto current_tp = TimerClock::now();
        for (; !timers_.empty() && current_tp >= timers_.top()->dead_line;) {
            if (timers_.top()->flag == true) {
                result.push_back(std::move(timers_.top()->task));
            }
            timers_.pop();
        }
    }

    TimerHandle push(const TimerClock::time_point& tp, TimerTask&& task) {
        auto ptimer = std::make_shared<TimerData>(tp, std::move(task));
        TimerHandle handle(ptimer);
        timers_.emplace(std::move(ptimer));
        return handle;
    }

    size_t empty() {
        return timers_.empty();
    }

    size_t next_duration() {
        if (timers_.empty()) {
            return 0xFFFFFFFF;
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            timers_.top()->dead_line - TimerClock::now()
        ).count();
        return duration < 0 ? 0 : duration;
    }

private:
    QueueType timers_;
};

}

#endif