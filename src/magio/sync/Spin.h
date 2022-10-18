#pragma once

#include <mutex>

namespace magio {

class SpinLock {
public:
    SpinLock() = default;
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire));
    }

    bool try_lock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }
private:
    std::atomic_flag flag_;
};

template<typename Lock>
class UseLock {
public:
    explicit UseLock(Lock& lock, bool use_lock)
        : lock_(lock)
        , use_lock_(use_lock) 
    { 
        if (use_lock_) {
            lock_.lock();
        }
    }

    ~UseLock() {
        if (use_lock_) {
            lock_.unlock();
        }
    }
private:
    Lock& lock_;
    bool use_lock_;
};

template<typename L>
UseLock(L&, bool) -> UseLock<L>;

}