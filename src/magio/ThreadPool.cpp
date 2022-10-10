#include "magio/ThreadPool.h"

#include <cstdio>
#include <list>
#include <thread>
#include "magio/core/Queue.h"
#include "magio/core/TimingTask.h"

namespace magio {

struct ThreadPool::Impl: public ExecutionContext {
    enum State {
        Stop, Running, PendingDestroy
    };

    State                                   state = Stop;
    RingQueue<Handler>                      idle_tasks{64};
    TimingTaskManager                       timed_tasks;

    std::mutex                              idle_m; // protect idle
    std::mutex                              timed_m; // protect timed
    std::condition_variable                 idle_cv;
    std::condition_variable                 timed_cv;
    std::atomic_size_t                      count = 0;

    std::vector<std::thread>                threads;

    void post(Handler&& handler) override;
    void dispatch(Handler&& handler) override;
    TimerID set_timeout(size_t ms, Handler&& handler) override;
    void clear(TimerID id) override;
    IOService get_service() override;
    void wait();
    void join();
    void attach();
    void destroy();

    void worker();
    void poller();

    ~Impl() {
        join();
    }
};

CLASS_PIMPL_IMPLEMENT(ThreadPool)

ThreadPool::ThreadPool(size_t thread_num) {
    impl = new Impl;

    for (size_t i = 0; i < thread_num; ++i) {
        impl->threads.emplace_back(&Impl::worker, impl);
    }
    impl->threads.emplace_back(&Impl::poller, impl);

    run();
}

void ThreadPool::post(Handler&& handler) {
    impl->post(std::move(handler));
}

void ThreadPool::dispatch(Handler &&handler) {
    impl->dispatch(std::move(handler));
}

TimerID ThreadPool::set_timeout(size_t ms, Handler&& handler) {
    return impl->set_timeout(ms, std::move(handler));
}

void ThreadPool::clear(TimerID id) {
    impl->clear(id);
}

void ThreadPool::run() {
    {
        std::scoped_lock lk(impl->idle_m, impl->timed_m);
        impl->state = Impl::Running;
    }
    impl->idle_cv.notify_all();
    impl->timed_cv.notify_all();
}

void ThreadPool::stop() {
    {
        std::scoped_lock lk(impl->idle_m, impl->timed_m);
        impl->state = Impl::Stop;
    }
    impl->idle_cv.notify_all();
    impl->timed_cv.notify_all();
}

void ThreadPool::wait() {
    impl->wait();
}

void ThreadPool::join() {
    impl->join();
}

void ThreadPool::attach() {
    impl->attach();
}

AnyExecutor ThreadPool::get_executor() const {
    return AnyExecutor(impl);
}

void ThreadPool::Impl::post(Handler&& handler) {
    count.fetch_add(1, std::memory_order_acquire);
    {
        std::lock_guard lk(idle_m);
        idle_tasks.push(std::move(handler));
    }
    idle_cv.notify_one();
}

void ThreadPool::Impl::dispatch(Handler &&handler) {
    post(std::move(handler));
}

TimerID ThreadPool::Impl::set_timeout(size_t ms, Handler&& handler) {
    TimerID id;
    count.fetch_add(1, std::memory_order_acquire);
    {
        std::lock_guard lk(timed_m);
        id = timed_tasks.set_timeout(ms, std::move(handler));
    }
    timed_cv.notify_one();
    return id;
}

void ThreadPool::Impl::clear(TimerID id) {
    {
        std::lock_guard lk(timed_m);
        timed_tasks.cancel(id);
    }
    count.fetch_sub(1, std::memory_order_release);
}

IOService ThreadPool::Impl::get_service() {
    return {};
}

void ThreadPool::Impl::wait() {
    for (; ;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(114));
        if (count.load() == 0) {
            return;
        }
    }
}

void ThreadPool::Impl::join() {
    wait();
    destroy();
    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
}

void ThreadPool::Impl::attach() {
    worker();
}

void ThreadPool::Impl::destroy() {
    {
        std::scoped_lock lk(idle_m, timed_m);
        state = PendingDestroy;
    }
    idle_cv.notify_all();
    timed_cv.notify_all();
}   

void ThreadPool::Impl::worker() {
    for (; ;) {
        std::unique_lock lk(idle_m);
        idle_cv.wait(lk, [this] {
            return (state == Running && !idle_tasks.empty()) 
                || state == PendingDestroy;
        });

        if (state == PendingDestroy) {
            return;
        }

        auto task = std::move(idle_tasks.front());
        idle_tasks.pop();
        lk.unlock();

        task();
        count.fetch_sub(1, std::memory_order_release);
    }
}

void ThreadPool::Impl::poller() {
    for (; ;) {
        std::unique_lock lk(timed_m);
        timed_cv.wait(lk, [this] {
            return (state == Running && !timed_tasks.empty()) 
                || state == PendingDestroy;
        });

        if (state == PendingDestroy) {
            return;
        }

        for (; ;) {
            if (auto res = timed_tasks.get_expired_task(); res) {
                lk.unlock();
                post(res.unwrap());
                count.fetch_sub(1, std::memory_order_release);
                lk.lock();
            } else {
                break;
            }
        }
    }
}

}