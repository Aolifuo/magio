#pragma once

#include <thread>
#include <functional>
#include "magio/Configs.h"
#include "magio/core/Queue.h"

namespace magio {

namespace plat {

class Runtime {

    Runtime(size_t worker_threads) {
        for (size_t i = 0; i < worker_threads; ++i) {
            work_threads_.emplace_back([this] {
                worker();
            });
        }
    }
public:
    Runtime(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;

    ~Runtime() {
        {
            std::unique_lock lk(m_);
            stop_flag_ = true;
        }
        cv_.notify_all();
        for (auto& th : work_threads_) {
            if (th.joinable()) {
                th.join();
            }
        }
    }

    static Runtime& ins() {
        static Runtime runtime(global_config.worker_threads);
        return runtime;
    }

    void post(std::function<void()>&& task) {
        {
            std::lock_guard lk(m_);
            tasks_.emplace(std::move(task));
        }
        cv_.notify_one();
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

    bool                                stop_flag_ = false;
    std::mutex                          m_;
    std::condition_variable             cv_;

    RingQueue<std::function<void()>>    tasks_{128};
    std::vector<std::thread>            work_threads_;
};

}

}
