#include "magio/EventLoop.h"

#include "magio/core/Queue.h"
#include "magio/core/TimingTask.h"

namespace magio {

struct EventLoop::Impl: public ExecutionContext {
    bool                                    stop_flag = false;
    RingQueue<CompletionHandler>            idle_tasks;
    TimingTaskManager                       pending_tasks;
    std::list<WaitingCompletionHandler>     waiting_tasks;

    void post(CompletionHandler&& handler) override;
    void dispatch(CompletionHandler&& handler) override;
    void waiting(WaitingCompletionHandler&& handler) override;
    TimerID set_timeout(size_t ms, CompletionHandler&& handler) override;
    void clear(TimerID id) override;
    bool poll();
};

CLASS_PIMPL_IMPLEMENT(EventLoop)

EventLoop::EventLoop() {
    impl = new Impl;
}

void EventLoop::post(CompletionHandler&& handler) {
    impl->post(std::move(handler));
}

void EventLoop::dispatch(CompletionHandler&& handler) {
    impl->dispatch(std::move(handler));
}

void EventLoop::waiting(WaitingCompletionHandler&& handler) {
    impl->waiting(std::move(handler));
}

TimerID EventLoop::set_timeout(size_t ms, CompletionHandler&& handler) {
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

void EventLoop::Impl::post(CompletionHandler&& handler) {
    idle_tasks.push(std::move(handler));
}

void EventLoop::Impl::dispatch(CompletionHandler &&handler) {
    post(std::move(handler));
}

void EventLoop::Impl::waiting(WaitingCompletionHandler&& handler) {
    waiting_tasks.push_back(std::move(handler));
}

TimerID EventLoop::Impl::set_timeout(size_t ms, CompletionHandler&& handler) {
    return pending_tasks.set_timeout(ms, std::move(handler));
}

void EventLoop::Impl::clear(TimerID id) {
    pending_tasks.cancel(id);
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
        if (auto res = pending_tasks.get_expired_task(); res) {
            res.unwrap()();
        } else {
            break;
        }
    }

    
    for (auto it = waiting_tasks.begin(); it != waiting_tasks.end(); ++it) {
        if ((*it)()) {
            it = waiting_tasks.erase(it);
        }
    }

    return !(idle_tasks.empty() && pending_tasks.empty() && waiting_tasks.empty());
}

}