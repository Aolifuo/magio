#pragma once

#include "magio/core/Queue.h"
#include "magio/coro/Coro.h"

namespace magio {

class SingleEvent {
public:
    SingleEvent(AnyExecutor exe)
        : exe_(exe)
    { }

    ~SingleEvent() {
        std::lock_guard lk(m_);
        while(!queued_wakers_.empty()) {
            exe_.async_wake(queued_wakers_.front());
            queued_wakers_.pop();
        }
    }

    void notify() {
        std::unique_lock lk(m_);
        if (queued_wakers_.empty()) {
            return;
        }
        auto waker = queued_wakers_.front();
        queued_wakers_.pop();
        lk.unlock();

        exe_.async_wake(waker);
    }

    Coro<> wait() {
        co_await Awaitable{
            [this](AnyExecutor exe, Waker waker, size_t tm) {
                std::lock_guard lk(m_);
                queued_wakers_.emplace(waker);
            }
        };
    }
private:
    AnyExecutor         exe_;
    RingQueue<Waker>    queued_wakers_;
    std::mutex          m_; //
};

}