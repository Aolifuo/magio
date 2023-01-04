#ifndef MAGIO_CORE_MUTEX_H_
#define MAGIO_CORE_MUTEX_H_

#include <atomic>

#include "magio-v3/core/coro_context.h"

namespace magio {

#ifdef MAGIO_USE_CORO
template<typename>
class Coro;

class CoroContext;

class LockGuard;

class Mutex: Noncopyable {
    friend class LockGuard;
    friend class Condition;

public:
    class LockAwaitable: Noncopyable {
    public:
        LockAwaitable(Mutex& m)
            : co_mutex_(m) 
        { }

        bool await_ready() { 
            return false; 
        }

        void await_suspend(std::coroutine_handle<> prev_h);

        void await_resume();

    private:
        Mutex& co_mutex_;
    };

    class GuardAwaitable: Noncopyable {
    public:
        GuardAwaitable(Mutex& m)
            : co_mutex_(m) 
        { }

        bool await_ready() { 
            return false; 
        }

        void await_suspend(std::coroutine_handle<> prev_h);

        LockGuard await_resume();

    private:
        Mutex& co_mutex_;
    };

    Mutex();
    
    [[nodiscard]]
    GuardAwaitable lock_guard();

private:
    struct Entry {
        CoroContext* ctx;
        std::coroutine_handle<> h;
    };

    [[nodiscard]]
    LockAwaitable lock();
    
    void unlock();

    std::atomic_flag flag_;
    std::mutex mutex_;
    std::deque<Entry> queue_;
};

class LockGuard: Noncopyable {
    friend class Condition;

public:
    LockGuard(Mutex& mutex)
        : mutex_(mutex)
    { }
    
    ~LockGuard() {
        mutex_.unlock();
    }

private:
    Mutex& mutex_;
};

class Condition: Noncopyable {
public:
    Condition() = default;

    template<typename Pred>
    [[nodiscard]]
    Coro<void> wait(LockGuard& guard, Pred pred) {
        while (!pred()) {
            co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
                {
                    std::lock_guard lk(m_);
                    queue_.push_back({LocalContext, h});
                }
                guard.mutex_.unlock();
            });
            co_await guard.mutex_.lock();
        }
        co_return;
    }

    void notify_one();

    void notify_all();

private:
    struct Entry {
        CoroContext* ctx;
        std::coroutine_handle<> h;
    };

    std::mutex m_;
    std::deque<Entry> queue_;
};
#endif

}

#endif