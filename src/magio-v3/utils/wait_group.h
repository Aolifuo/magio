#ifndef MAGIO_CORE_WAIT_GROUP_H_
#define MAGIO_CORE_WAIT_GROUP_H_

#include <mutex>
#include <condition_variable>

namespace magio {

class WaitGroup {
public:
    WaitGroup(size_t task_num)
        : wait_num_(task_num)
    { }

    void done() {
        std::lock_guard lk(m_);
        if (wait_num_ == 0) {
            return;
        }

        --wait_num_;
        if (wait_num_ == 0) {
            cv_.notify_all();
        }
    }

    void wait() {
        std::unique_lock lk(m_);
        cv_.wait(lk, [this] {
            return wait_num_ == 0;
        });
    }
private:
    std::mutex m_;
    size_t wait_num_;
    std::condition_variable cv_;
};

}

#endif