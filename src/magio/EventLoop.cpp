#include "magio/EventLoop.h"

#include <list>
#include "magio/dev/Log.h"
#include "magio/sync/Lock.h"
#include "magio/core/Queue.h"
#include "magio/core/TimingTask.h"

namespace magio {

struct EventLoop::Impl: public ExecutionContext {
    std::atomic_flag                        stop_flag;
    RingQueue<CompletionHandler>            idle_tasks;
    TimingTaskManager                       timed_tasks;
    RingQueue<WaitingCompletionHandler>     waiting_tasks;

    SpinLock                                spin_idle;
    SpinLock                                spin_timed;
    SpinLock                                spin_wait;
    std::atomic_size_t                      count = 0;

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
    impl->stop_flag.clear(std::memory_order_relaxed);
    while (impl->poll());
}

void EventLoop::stop() {
    impl->stop_flag.test_and_set(std::memory_order_relaxed);
}

AnyExecutor EventLoop::get_executor() const {
    return AnyExecutor(impl);
}

void EventLoop::Impl::post(CompletionHandler&& handler) {
    std::lock_guard lk(spin_idle);
    idle_tasks.push(std::move(handler));
    count.fetch_add(1, std::memory_order_relaxed);
}

void EventLoop::Impl::dispatch(CompletionHandler &&handler) {
    post(std::move(handler));
}

TimerID EventLoop::Impl::set_timeout(size_t ms, CompletionHandler&& handler) {
    std::lock_guard lk(spin_timed);
    auto id = timed_tasks.set_timeout(ms, std::move(handler));
    count.fetch_add(1, std::memory_order_relaxed);
    return id;
}

void EventLoop::Impl::clear(TimerID id) {
    std::lock_guard lk(spin_timed);
    timed_tasks.cancel(id);
    count.fetch_sub(1);
}

void EventLoop::Impl::waiting(WaitingCompletionHandler&& handler) {
    std::lock_guard lk(spin_wait);
    waiting_tasks.emplace(std::move(handler));
    count.fetch_add(1, std::memory_order_relaxed);
}


bool EventLoop::Impl::poll() {
    if (stop_flag.test(std::memory_order_relaxed)) {
        return false;
    }

    for (; ;) {
        std::unique_lock lk(spin_idle);
        if (idle_tasks.empty()) {
            break;
        }
        auto task = std::move(idle_tasks.front());
        idle_tasks.pop();
        lk.unlock();

        task();
        count.fetch_sub(1);
    }

    for (; ;) {
        std::unique_lock lk(spin_timed);
        auto res = timed_tasks.get_expired_task();
        lk.unlock();

        if (!res) {
            break;
        }

        res.unwrap()();
        count.fetch_sub(1);
    }

    {
        std::unique_lock lk(spin_wait);
        if (!waiting_tasks.empty()) {
            auto task = std::move(waiting_tasks.front());
            waiting_tasks.pop();    
            lk.unlock();

            if (!task()) {
                lk.lock();
                waiting_tasks.emplace(std::move(task));
            } else {
                count.fetch_sub(1);
            }
        }
    }
    
    return count.load() != 0;
}

}