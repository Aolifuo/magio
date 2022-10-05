#pragma once

#include <map>
#include <chrono>
#include "magio/core/MaybeUninit.h"
#include "magio/core/Fwd.h"

namespace magio {

class TimingTaskManager {
public:
    using Clock = std::chrono::steady_clock;
    using Ms = std::chrono::milliseconds;

    TimerID set_timeout(size_t ms, CompletionHandler handler) {
        size_t id = (Clock::now() + Ms(ms)).time_since_epoch().count();
        tasks_.emplace(id, std::move(handler));
        return id;
    }

    void cancel(TimerID id) {
        tasks_.erase(id);
    }

    size_t size() {
        return tasks_.size();
    }

    bool empty() {
        return tasks_.size() == 0;
    }

    MaybeUninit<CompletionHandler> get_expired_task() {
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
    std::map<size_t, CompletionHandler> tasks_;
};

}