#include "magio-v3/core/coro_context.h"

#include "magio-v3/core/logger.h"
#ifdef _WIN32
#include "magio-v3/net/iocp.h"
#define IOSERVICE(X) std::make_unique<::magio::net::IoCompletionPort>()
#elif defined (__linux__)
#include "magio-v3/net/io_uring.h"
#define IOSERVICE(X) std::make_unique<::magio::net::IoUring>(X)
#endif

namespace magio {

CoroContext::CoroContext(size_t entries)
    : thread_id_(CurrentThread::get_id()) 
{
    if (LocalContext != nullptr) {
        M_FATAL("{}", "This thread already has a context");
    }

    if (entries == 0) {
        M_FATAL("{}", "Entries cannot be zero");
    }

    p_io_service_ = IOSERVICE(entries);
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
    decltype(pending_handles_) handles;
    std::vector<TimerTask> timer_tasks;

    for (; state_ != Stopping;) {
        {
            std::lock_guard lk(mutex_);
            handles.swap(pending_handles_);
        }

        for (auto& func : handles) {
            func();
        }
        // TODO shrink
        handles.clear();

        timer_queue_.get_expired(timer_tasks);
        for (auto& task : timer_tasks) {
            task(true);
        }
        // TODO shrink
        timer_tasks.clear();

        int64_t next_duration = timer_queue_.next_duration();
        {
            std::lock_guard lk(mutex_);
            if (!pending_handles_.empty() || state_ == Stopping) {
                next_duration = 0;
            }
        }
        handle_io_poller(next_duration);
    }
}

void CoroContext::stop() {
    if (!assert_in_context_thread()) {
        M_FATAL("{}", "You can't stop the context on another thread");
    }

    state_ = Stopping;
}

void CoroContext::execute(Task &&task) {
    {
        std::lock_guard lk(mutex_);
        pending_handles_.push_back(std::move(task));
    }

    if (!assert_in_context_thread()) {
        wake_up();
    }
}

void CoroContext::dispatch(Task &&task) {
    if (assert_in_context_thread()) {
        task();
    } else {
        execute(std::move(task));
    }
}

void CoroContext::handle_io_poller(int64_t wait_time) {
    std::error_code ec;
    int status = p_io_service_->poll(wait_time < 0 ? 0 : wait_time, ec);
    if (-1 == status) {
        M_SYS_ERROR("Io service error: {}, then the context will be stopped", ec.message());
        stop();
    }
}

#ifdef MAGIO_USE_CORO
void CoroContext::wake_in_context(std::coroutine_handle<> h) {
    if (assert_in_context_thread()) {
        h.resume();
    } else {
        queue_in_context(h);
    }
}

void CoroContext::queue_in_context(std::coroutine_handle<> h) {
    execute([h]() mutable { h.resume(); });
}
#endif

void CoroContext::wake_up() {
    p_io_service_->wake_up();
}

IoService& CoroContext::get_service() const {
    return *p_io_service_;
}

bool CoroContext::assert_in_context_thread() {
    return thread_id_ == CurrentThread::get_id();
}

}