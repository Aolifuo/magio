#include "magio-v3/core/mutex.h"

namespace magio {

#ifdef MAGIO_USE_CORO
void Mutex::LockAwaitable::await_suspend(std::coroutine_handle<> prev_h) {
    if (co_mutex_.flag_.test_and_set(std::memory_order_acquire)) {
        std::lock_guard lk(co_mutex_.mutex_);
        co_mutex_.queue_.push_back({LocalContext, prev_h});
    } else {
        prev_h.resume();
    }
}

void Mutex::LockAwaitable::await_resume() {

}

void Mutex::GuardAwaitable::await_suspend(std::coroutine_handle<> prev_h) {
    if (co_mutex_.flag_.test_and_set(std::memory_order_acquire)) {
        std::lock_guard lk(co_mutex_.mutex_);
        co_mutex_.queue_.push_back({LocalContext, prev_h});
    } else {
        prev_h.resume();
    }
}

LockGuard Mutex::GuardAwaitable::await_resume() {
     return {co_mutex_};
}

Mutex::Mutex()
    : flag_()
{ }

Mutex::GuardAwaitable Mutex::lock_guard() {
    return {*this};
}

Mutex::LockAwaitable Mutex::lock() {
    return {*this};
}

void Mutex::unlock() {
    Entry entry;
    {
        std::lock_guard lk(mutex_);
        if (!queue_.empty()) {
            entry = queue_.front();
            queue_.pop_front();
        } else {
            flag_.clear(std::memory_order_release);
        }
    }
    if (entry.h) {
        entry.ctx->queue_in_context(entry.h);
    }
}

void Condition::notify_one() {
    Entry entry;
    {
        std::lock_guard lk(m_);
        if (!queue_.empty()) {
            entry = queue_.front();
            queue_.pop_front();
        }
    }
    if (entry.h) {
        entry.ctx->queue_in_context(entry.h);
    }
}

void Condition::notify_all() {
    std::deque<Entry> tmp;
    {
        std::lock_guard lk(m_);
        tmp.swap(queue_);
    }
    for (auto entry : tmp) {
        entry.ctx->queue_in_context(entry.h);
    }
}
#endif

}