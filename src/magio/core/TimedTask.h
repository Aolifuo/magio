#pragma once

#include <map>
#include <chrono>
#include "magio/core/Fwd.h"
#include "magio/core/MaybeUninit.h"

namespace magio {

class TimedTaskManager {
public:
    using Clock = std::chrono::steady_clock;
    using Ms = std::chrono::milliseconds;

    ~TimedTaskManager() {
        cancel_all();
    }

    TimerID set_timeout(size_t ms, TimerHandler handler) {
        size_t id = (Clock::now() + Ms(ms)).time_since_epoch().count();
        tasks_.emplace(id, std::move(handler));
        return id;
    }

    bool cancel(TimerID id) {
        auto it = tasks_.find(id);
        if (it != tasks_.end()) {
            it->second(true);
            tasks_.erase(it);
            return true;
        } else {
            return false;
        }
    }

    void cancel_all() {
        for (auto&& it : tasks_) {
            it.second(true);
        }
    }

    size_t size() {
        return tasks_.size();
    }

    bool empty() {
        return tasks_.size() == 0;
    }

    MaybeUninit<TimerHandler> get_expired_task() {
        if (tasks_.empty()) {
            return {};
        }
        size_t now_time = Clock::now().time_since_epoch().count();
        
        if (auto it = tasks_.begin(); it->first <= now_time) {
            auto res = std::move(it->second);
            tasks_.erase(it);
            return std::move(res);
        }
        return {};
    }
private:
    std::map<size_t, TimerHandler> tasks_;
};

}