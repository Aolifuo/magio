#pragma once

#include <mutex>

namespace magio {

class SpinLock {
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire));
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }
private:
    std::atomic_flag flag_;
};


}