#include "magio-v3/core/coro_context.h"

#include "magio-v3/core/logger.h"
#ifdef _WIN32
#include "magio-v3/net/iocp.h"
#define IOSERVICE std::make_unique<::magio::net::IoCompletionPort>()
#elif defined (__linux__)
#include "magio-v3/net/io_uring.h"
#define IOSERVICE std::make_unique<::magio::net::IoUring>(100)
#endif

namespace magio {

CoroContext::CoroContext(bool enable_io)
    : thread_id_(CurrentThread::get_id()) 
{
    if (LocalContext != nullptr) {
        M_FATAL("{}", "This thread already has a context");
    }

    if (enable_io) {
        p_io_service_ = IOSERVICE;
    }

    LocalContext = this;
}

void CoroContext::start() {
    if (!assert_in_context_thread()) {
        M_FATAL("{}", "You can't start the context on another thread");
    }

    if (state_ != Stopping) {
        M_FATAL("{}", "You can't start the context twice");
    }

    state_ = Running;
    std::vector<std::coroutine_handle<>> handles;
    std::vector<TimerTask> timer_tasks;

    for (; state_ != Stopping;) {
        // wake handle
        {
            std::lock_guard lk(mutex_);
            handles.swap(pending_handles_);
        }
        for (auto& h : handles) {
            h.resume();
        }
        // TODO shrink
        handles.clear();

        // invoke timer
        timer_queue_.get_expired(timer_tasks);
        for (auto& task : timer_tasks) {
            task(true);
        }
        // TODO shrink
        timer_tasks.clear();
        
        // IO
        handle_io_poller();
    }
}

void CoroContext::stop() {
    if (!assert_in_context_thread()) {
        M_FATAL("{}", "You can't stop the context on another thread");
    }

    state_ = Stopping;
}

void CoroContext::execute(Task &&task) {
    auto coro = [task = std::move(task)]() -> Coro<> {
        co_return task();
    }();
    wake_in_context(coro.handle());
}

void CoroContext::handle_io_poller() {
    if (!p_io_service_) {
        return;
    }

    bool block = false;
    bool stop_flag = false;
    for (; !stop_flag && state_ != Stopping;) {
        std::error_code ec;
        int status = p_io_service_->poll(block, ec);
        block = false;
        if (status == -1) {
            // error
            M_SYS_ERROR("Io service error: {}, then the context will be stopped", ec.value());
            stop();
            stop_flag = true;
        } else if (status == 0) {
            // wait timeout
            bool is_pending_empty = false;
            {
                std::lock_guard lk(mutex_);
                is_pending_empty = pending_handles_.empty();
            }
            if (!is_pending_empty || !timer_queue_.empty()) {
                stop_flag = true;
            } else {
                // try to sleep
                block = true;
            }
        } else if (status == 1) {
            // io completion
            // continue;
        } else {
            // be waked up
            stop_flag = true;
        }
    }
}

void CoroContext::wake_up() {
    p_io_service_->wake_up();
}

void CoroContext::wake_in_context(std::coroutine_handle<> h) {
    std::lock_guard lk(mutex_);
    pending_handles_.push_back(h);
}

IoService& CoroContext::get_service() const {
    if (!p_io_service_) {
        M_FATAL("{}", "There is no io service");
    }
    return *p_io_service_;
}

bool CoroContext::assert_in_context_thread() {
    return thread_id_ == CurrentThread::get_id();
}

}