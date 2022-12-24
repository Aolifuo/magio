#include "magio-v3/core/thread_pool.h"

#include "magio-v3/core/coro.h"
#include "magio-v3/core/logger.h"

namespace magio {

ThreadPool::ThreadPool(size_t thread_num)
    : threads_(thread_num) 
{
    if (thread_num < 1) {
        M_FATAL("{}", "worker threads cannot less than 1");
    }

    if (!LocalContext) {
        M_FATAL("{}", "This thread is missing a context");
    }
}

ThreadPool::~ThreadPool() {
    wait();
    {
        std::lock_guard lk(mutex_);
        state_ = PendingDestroy;
    }
    cv_.notify_all();
    
    for (auto& th : threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
}

void ThreadPool::start() {
    std::call_once(once_flag_, [&] {
        for (auto& th : threads_) {
            th = std::thread(&ThreadPool::run_in_background, this);
        }
    });

    {
        std::lock_guard lk(mutex_);
        state_ = Running;
    }
    cv_.notify_all();
}

void ThreadPool::stop() {
    {
        std::lock_guard lk(mutex_);
        state_ = Stopping;
    }
}

void ThreadPool::wait() {
    std::unique_lock lk(mutex_);
    if (state_ != Running) {
        return;
    }
    wait_flag_ = true;
    wait_cv_.wait(lk, [this] {
        return tasks_.empty();
    });
}

void ThreadPool::execute(Task &&task) {
    {
        std::lock_guard lk(mutex_);
        tasks_.push_back(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::run_in_background() {
    Task task;

    for (; ;) {
        {
            std::unique_lock lk(mutex_);

            cv_.wait(lk, [this] {
                if (tasks_.empty()) {
                    if (wait_flag_) {
                        wait_flag_ = false;
                        wait_cv_.notify_all();
                    }
                    return state_ == PendingDestroy;
                }
                return state_ == Running;
            });

            if (state_ == PendingDestroy) {
                M_TRACE("{}", "One thread exits");
                return;
            }

            task.swap(tasks_.front());
            tasks_.pop_front();
        }
        
        try {
            task();
        } catch(...) {
            M_FATAL("{}", "One task throw exception when thread function is running");
        }
    }
}

}