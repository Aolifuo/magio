#include "magio/Magico.h"

#include "magio/core/Queue.h"
#include "magio/core/Logger.h"
#include "magio/core/TimedTask.h"
#include "magio/coro/Awaitbale.h"
#include "magio/plat/io_service.h"
#include "magio/sync/Spin.h"

namespace magio {

struct Magico::Impl: public ExecutionContext {
    std::atomic_flag                stop_flag;

    RingQueue<Waker>                queued_wakers;
    TimedTaskManager                timed_tasks;

    bool                            use_lock = false;
    std::mutex                      wakers_m;           // protect wakers
    std::mutex                      timed_m;            // protect timed

    std::atomic_size_t              count = 0;          // idle timed and io tasks

    // io
    plat::IOService                 service;
    std::vector<std::thread>        extra_threads;      // only for io service

    void async_wake(Waker waker) override;
    TimerID invoke_after(TimerHandler&& handler, size_t ms) override;
    bool cancel(TimerID id) override;
    plat::IOService& get_service() override;

    bool poll();                                        // posted tasks and timed_tasks
    void worker();                                      // io poll

    // may throw
    Impl(size_t workers)
        : service(count) 
    {
        if (auto ec = service.open()) {
            M_ERROR("{}", ec.message());
            throw std::system_error(ec);
        }

        // extra threads
        for (size_t i = 0; i < workers - 1; ++i) {
            extra_threads.emplace_back(&Impl::worker, this);
        }
    }

    ~Impl() {
        service.close();
        for (auto& th : extra_threads) {
            if (th.joinable()) {
                th.join();
            }
        }
    }
};

CLASS_PIMPL_IMPLEMENT(Magico)

Magico::Magico(size_t worker_threads) {
    assert(worker_threads >= 1);
    impl = new Impl{worker_threads};
    impl->use_lock = worker_threads == 1? false : true;
}

void Magico::run() {
    impl->stop_flag.clear();
    while (impl->poll());
}

void Magico::stop() {
    impl->stop_flag.test_and_set();
}
 
AnyExecutor Magico::get_executor() const {
    return AnyExecutor(impl);
}

void Magico::Impl::async_wake(Waker waker) {
    UseLock lk(wakers_m, use_lock);
    count.fetch_add(1, std::memory_order_acquire);
    queued_wakers.emplace(waker);
}

TimerID Magico::Impl::invoke_after(TimerHandler&& handler, size_t ms) {
    UseLock lk(timed_m, use_lock);
    count.fetch_add(1, std::memory_order_acquire);
    auto id = timed_tasks.set_timeout(ms, std::move(handler));
    return id;
}

bool Magico::Impl::cancel(TimerID id) {
    UseLock lk(timed_m, use_lock);
    bool flag = timed_tasks.cancel(id);
    if (flag) {
        count.fetch_sub(1, std::memory_order_release);
        return true;
    }
    return false;
}

plat::IOService& Magico::Impl::get_service() {
    return service;
}

bool Magico::Impl::poll() {
    if (stop_flag.test(std::memory_order_relaxed)) {
        return false;
    }

    for (; ;) {
        Waker waker;
        {
            UseLock lk(wakers_m, use_lock);
            if (queued_wakers.empty()) {
                break;
            }
            waker = queued_wakers.front();
            queued_wakers.pop();
        }

        waker.wake();
        count.fetch_sub(1, std::memory_order_release);
    }

    for (; ;) {
        MaybeUninit<TimerHandler> may_h;
        {
            UseLock lk(timed_m, use_lock);
            may_h = timed_tasks.get_expired_task();
        }

        if (!may_h) {
            break;
        }

        may_h.unwrap()(false);
        count.fetch_sub(1, std::memory_order_release);
    }

    // io
    if (auto ec = service.poll(0)) {
        M_ERROR("main loop error: {}", ec.message());
        return false;
    }

    return count.load() != 0;
}

void Magico::Impl::worker() {
    for (; ;) {
        if (auto ec = service.poll(plat::MAGIO_INFINITE)) {
            M_ERROR("sub loop error: {}", ec.message());
            return;
        }
    }
}

}