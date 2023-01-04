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

        for (auto& h : handles) {
#ifdef MAGIO_USE_CORO
            h.resume();
#else
            h();
#endif
        }
        // TODO shrink
        handles.clear();

        timer_queue_.get_expired(timer_tasks);
        for (auto& task : timer_tasks) {
            task(true);
        }
        // TODO shrink
        timer_tasks.clear();

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
#ifdef MAGIO_USE_CORO
    auto coro = [](Task task) mutable -> Coro<> {
        co_return task();
    }(std::move(task));
    queue_in_context(coro.handle());
#else
    {
        std::lock_guard lk(mutex_);
        pending_handles_.push_back(std::move(task));
    }
    if (!assert_in_context_thread()) {
        wake_up();
    }
#endif
}

void CoroContext::dispatch(Task &&task) {
    if (assert_in_context_thread()) {
        task();
    } else {
        execute(std::move(task));
    }
}

void CoroContext::handle_io_poller() {
    std::error_code ec;
    bool block = true;
    bool is_pending_empty = false;
    {
        std::lock_guard lk(mutex_);
        is_pending_empty = pending_handles_.empty();
    }
    if (!is_pending_empty || !timer_queue_.empty() || state_ == Stopping) {
        block = false;
    }

    int status = p_io_service_->poll(block, ec);
    if (-1 == status) {
        M_SYS_ERROR("Io service error: {}, then the context will be stopped", ec.value());
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
    {
        std::lock_guard lk(mutex_);
        pending_handles_.push_back(h);
    }
    if (!assert_in_context_thread()) {
        wake_up();
    }
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