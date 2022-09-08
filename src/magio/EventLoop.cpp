#include "magio/EventLoop.h"

#include "magio/core/Queue.h"
#include "magio/core/TimingTask.h"

namespace magio {

struct EventLoop::Impl: public ExecutionContext {
    bool stop_flag = false;
    RingQueue<CompletionHandler> idle_tasks;
    TimingTaskManager pending_tasks_;

    void post(CompletionHandler handler) override;
    TimerID set_timeout(size_t ms, CompletionHandler handler) override;
    void clear(TimerID id) override;
    bool poll() override;
};

CLASS_PIMPL_IMPLEMENT(EventLoop)

EventLoop::EventLoop() {
    impl = new Impl;
}

void EventLoop::post(CompletionHandler handler) {
    impl->post(std::move(handler));
}

TimerID EventLoop::set_timeout(size_t ms, CompletionHandler handler) {
    return impl->set_timeout(ms, std::move(handler));
}

void EventLoop::clear(TimerID id) {
    impl->clear(id);
}

bool EventLoop::poll() {
    return impl->poll();
}

void EventLoop::run() {
    while (impl->poll());
}

void EventLoop::stop() {
    impl->stop_flag = true;
}

void EventLoop::restart() {
    impl->stop_flag = false;
}

AnyExecutor EventLoop::get_executor() {
    return AnyExecutor(impl);
}

void EventLoop::Impl::post(CompletionHandler handler) {
    idle_tasks.push(std::move(handler));
}

TimerID EventLoop::Impl::set_timeout(size_t ms, CompletionHandler handler) {
    return pending_tasks_.set_timeout(ms, std::move(handler));
}

void EventLoop::Impl::clear(TimerID id) {
    pending_tasks_.cancel(id);
}

bool EventLoop::Impl::poll() {
    if (stop_flag) {
        return false;
    }

    for (; !idle_tasks.empty();) {
        idle_tasks.front()();
        idle_tasks.pop();
    }

    for (; ;) {
        if (auto res = pending_tasks_.get_expired_task(); res) {
            idle_tasks.push(res.unwrap());
        } else {
            break;
        }
    }

    return !(idle_tasks.empty() && pending_tasks_.empty());
}

}