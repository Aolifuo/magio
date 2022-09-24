#pragma once

#include <memory>
#include <mutex>

namespace magio {

class WaitGroup: public std::enable_shared_from_this<WaitGroup> {
public:
    WaitGroup(size_t task_num)
        : task_num_(task_num)
        , cur_num_(0)
    {

    }

    void done() {
        std::unique_lock lk(m_);
        if (cur_num_ == task_num_) {
            return;
        }

        ++cur_num_;
        if (cur_num_ == task_num_) {
            lk.unlock();
            cv_.notify_all();
        }
    }

    void wait() {
        std::unique_lock lk(m_);
        cv_.wait(lk, [p = shared_from_this()]
            { return p->cur_num_ == p->task_num_; });
    }
private:
    size_t task_num_;
    size_t cur_num_;
    std::mutex m_;
    std::condition_variable cv_;
};

}