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
    std::vector<std::coroutine_handle<>> handles;
    std::vector<TimerTask> timer_tasks;

    for (; state_ != Stopping;) {
        {
            std::lock_guard lk(mutex_);
            handles.swap(pending_handles_);
        }

        for (auto& h : handles) {
            h.resume();
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
    auto coro = [](Task task) mutable -> Coro<> {
        co_return task();
    }(std::move(task));
    wake_in_context(coro.handle());
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

void CoroContext::wake_in_context(std::coroutine_handle<> h) {
    {
        std::lock_guard lk(mutex_);
        pending_handles_.push_back(h);
    }
    if (!assert_in_context_thread()) {
        wake_up();
    }
}

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