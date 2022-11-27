#include "magio/ThreadPool.h"

#include <thread>
#include "magio/core/Queue.h"
#include "magio/coro/Awaitbale.h"
#include "magio/core/TimedTask.h"
#include "magio/plat/io_service.h"

namespace magio {

struct ThreadPool::Impl: public ExecutionContext {
    enum State {
        Stop, Running, PendingDestroy
    };

    State                                   state = Stop;
    RingQueue<Waker>                        queued_wakers{64};
    TimedTaskManager                        timed_tasks;

    std::mutex                              wakers_m; // protect idle
    std::mutex                              timed_m; // protect timed
    std::condition_variable                 wakers_cv;
    std::condition_variable                 timed_cv;
    std::atomic_size_t                      count = 0;

    plat::IOService                         service; //empty

    std::vector<std::thread>                threads;


    void async_wake(Waker waker) override;
    TimerID invoke_after(TimerHandler&& handler, size_t ms) override;
    bool cancel(TimerID id) override;
    plat::IOService& get_service() override;
    void wait();
    void join();
    void attach();
    void destroy();

    void worker();
    void time_poller();

    ~Impl() {
        join();
    }
};

CLASS_PIMPL_IMPLEMENT(ThreadPool)

ThreadPool::ThreadPool(size_t thread_num, bool flag) {
    impl = new Impl;

    for (size_t i = 0; i < thread_num; ++i) {
        impl->threads.emplace_back(&Impl::worker, impl);
    }
    impl->threads.emplace_back(&Impl::time_poller, impl);

    run();
}

void ThreadPool::run() {
    {
        std::scoped_lock lk(impl->wakers_m, impl->timed_m);
        impl->state = Impl::Running;
    }
    impl->wakers_cv.notify_all();
    impl->timed_cv.notify_all();
}

void ThreadPool::stop() {
    {
        std::scoped_lock lk(impl->wakers_m, impl->timed_m);
        impl->state = Impl::Stop;
    }
    impl->wakers_cv.notify_all();
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

void ThreadPool::Impl::async_wake(Waker waker) {
    count.fetch_add(1, std::memory_order_acquire);
    {
        std::lock_guard lk(wakers_m);
        queued_wakers.emplace(waker);
    }
    wakers_cv.notify_one();
}

TimerID ThreadPool::Impl::invoke_after(TimerHandler&& handler, size_t ms) {
    TimerID id;
    count.fetch_add(1, std::memory_order_acquire);
    {
        std::lock_guard lk(timed_m);
        id = timed_tasks.set_timeout(ms, std::move(handler));
    }
    timed_cv.notify_one();
    return id;
}

bool ThreadPool::Impl::cancel(TimerID id) {
    bool flag = false;
    {
        std::lock_guard lk(timed_m);
        flag = timed_tasks.cancel(id);
    }
    if (flag) {
        count.fetch_sub(1, std::memory_order_release);
        return true;
    }
    return false;
}

plat::IOService& ThreadPool::Impl::get_service() {
    return service;
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
        std::scoped_lock lk(wakers_m, timed_m);
        state = PendingDestroy;
    }
    wakers_cv.notify_all();
    timed_cv.notify_all();
}   

void ThreadPool::Impl::worker() {
    for (; ;) {
        std::unique_lock lk(wakers_m);
        wakers_cv.wait(lk, [this] {
            return (state == Running && !queued_wakers.empty()) 
                || state == PendingDestroy;
        });

        if (state == PendingDestroy) {
            return;
        }

        auto waker = queued_wakers.front();
        queued_wakers.pop();
        lk.unlock();

        waker.wake();
        count.fetch_sub(1, std::memory_order_release);
    }
}

void ThreadPool::Impl::time_poller() {
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
                res.unwrap()(false);
                count.fetch_sub(1, std::memory_order_release);
            } else {
                break;
            }
        }
    }
}

}