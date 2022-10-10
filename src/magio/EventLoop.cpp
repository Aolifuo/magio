#include "magio/EventLoop.h"

#include <cassert>
#include "magio/core/Queue.h"
#include "magio/core/TimingTask.h"
#include "magio/plat/runtime.h"
#include "magio/plat/loop.h"
#include "magio/sync/Lock.h"

namespace magio {

struct EventLoop::Impl: public ExecutionContext {
    std::atomic_flag                    stop_flag;

    RingQueue<Handler>                  idle_tasks;
    TimingTaskManager                   timed_tasks;

    bool                                use_lock = false;
    std::mutex                          idle_m; // protect idle
    std::mutex                          timed_m; // protect timed
    std::atomic_size_t                  count = 0; //

    // io
    std::once_flag                      once_f; // only open once
    plat::Runtime                       runtime;
    plat::IOLoop                        io_loop;

    void post(Handler&& handler) override;
    void dispatch(Handler&& handler) override;
    TimerID set_timeout(size_t ms, Handler&& handler) override;
    void clear(TimerID id) override;
    IOService get_service() override;

    bool poll();

    Impl(size_t workers)
        : runtime(workers) 
    {

    }

    ~Impl() {
        MAGIO_CHECK_RESOURCE;
    }
};

CLASS_PIMPL_IMPLEMENT(EventLoop)

EventLoop::EventLoop(size_t worker_threads) {
    assert(worker_threads >= 1);
    impl = new Impl{worker_threads - 1};
    impl->use_lock = worker_threads == 1? false : true;
}

void EventLoop::post(Handler&& handler) {
    impl->post(std::move(handler));
}

void EventLoop::dispatch(Handler&& handler) {
    impl->dispatch(std::move(handler));
}

TimerID EventLoop::set_timeout(size_t ms, Handler&& handler) {
    return impl->set_timeout(ms, std::move(handler));
}

void EventLoop::clear(TimerID id) {
    impl->clear(id);
}

bool EventLoop::poll() {
    return impl->poll();
}

void EventLoop::run() {
    impl->stop_flag.clear();
    while (impl->poll());
}

void EventLoop::stop() {
    impl->stop_flag.test_and_set();
}

IOService EventLoop::get_service() {
    return impl->get_service();
}
 
AnyExecutor EventLoop::get_executor() const {
    return AnyExecutor(impl);
}

void EventLoop::Impl::post(Handler&& handler) {
    UseLock lk(idle_m, use_lock);
    count.fetch_add(1, std::memory_order_acquire);
    idle_tasks.push(std::move(handler));
}

void EventLoop::Impl::dispatch(Handler &&handler) {
    post(std::move(handler));
}

TimerID EventLoop::Impl::set_timeout(size_t ms, Handler&& handler) {
    UseLock lk(timed_m, use_lock);
    count.fetch_add(1, std::memory_order_acquire);
    auto id = timed_tasks.set_timeout(ms, std::move(handler));
    return id;
}

void EventLoop::Impl::clear(TimerID id) {
    std::lock_guard lk(timed_m);
    timed_tasks.cancel(id);
    count.fetch_sub(1, std::memory_order_release);
}

IOService EventLoop::Impl::get_service() {
    return {&io_loop, &runtime, &once_f};
}

bool EventLoop::Impl::poll() {
    if (stop_flag.test(std::memory_order_relaxed)) {
        return false;
    }

    for (; ;) {
        Handler task;
        {
            UseLock lk(idle_m, use_lock);
            if (idle_tasks.empty()) {
                break;
            }
            task = std::move(idle_tasks.front());
            idle_tasks.pop();
        }

        task();
        count.fetch_sub(1, std::memory_order_release);
    }

    for (; ;) {
        MaybeUninit<Handler> may_h;
        {
            UseLock lk(timed_m, use_lock);
            may_h = timed_tasks.get_expired_task();
        }

        if (!may_h) {
            break;
        }

        may_h.unwrap()();
        count.fetch_sub(1, std::memory_order_release);
    }

    // io
    if (auto ec = io_loop.poll(0)) {
        if (ec.value() == -2 && count.load() == 0) {
            return false;
        }
    }

    return true;
}
 
}