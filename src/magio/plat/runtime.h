#pragma once

#include <thread>
#include <functional>
#include "magio/core/Queue.h"
#include "magio/execution/Execution.h"

namespace magio {

namespace plat {

class Runtime {
public:
    Runtime(size_t worker_threads) {
        for (size_t i = 0; i < worker_threads; ++i) {
            work_threads_.emplace_back([this] {
                worker();
            });
        }
    }

    ~Runtime() {
        {
            std::lock_guard lk(m_);
            stop_flag_ = true;
        }

        cv_.notify_all();
        for (auto& th : work_threads_) {
            if (th.joinable()) {
                th.join();
            }
        }
    }

    void post(std::function<void()>&& task) {
        {
            std::lock_guard lk(m_);
            tasks_.emplace(std::move(task));
        }
        cv_.notify_one();
    }

    size_t workers() {
        return work_threads_.size();
    }
private:
    void worker() {
        for (; ;) {
            std::unique_lock lk(m_);
            cv_.wait(lk, [this]{ return !tasks_.empty() || stop_flag_; });
            if (stop_flag_) {
                return;
            }

            auto task = std::move(tasks_.front());
            tasks_.pop();
            lk.unlock();
            
            task();
        }
    }

    bool                                                    stop_flag_ = false;
    std::mutex                                              m_; // protect tasks
    std::condition_variable                                 cv_;

    RingQueue<std::function<void()>>                        tasks_{128};
    std::vector<std::thread>                                work_threads_;
};


}

}
