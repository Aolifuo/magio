#include "magio/Magico.h"

#include <cassert>
#include <thread>
#include "magio/core/Queue.h"
#include "magio/core/TimingTask.h"
#include "magio/plat/io_service.h"
#include "magio/sync/Lock.h"

namespace magio {

struct Magico::Impl: public ExecutionContext {
    std::atomic_flag                stop_flag;

    RingQueue<Handler>              posted_tasks;
    TimingTaskManager               timed_tasks;

    bool                            use_lock = false;
    std::mutex                      posted_m;           // protect posted
    std::mutex                      timed_m;            // protect timed

    std::atomic_size_t              count = 0;          // idle timed and io tasks

    // io
    plat::IOService                 service;
    std::vector<std::thread>        extra_threads;      // only for io service

    void post(Handler&& handler) override;
    void dispatch(Handler&& handler) override;
    TimerID set_timeout(size_t ms, Handler&& handler) override;
    void clear(TimerID id) override;
    plat::IOService& get_service() override;

    bool poll();                                        // posted tasks and timed_tasks
    void worker();                                      // io poll

    // may throw
    Impl(size_t workers)
        : service(count) 
    {
        if (auto ec = service.open()) {
            DEBUG_LOG(ec.message());
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
        MAGIO_CHECK_RESOURCE;
    }
};

CLASS_PIMPL_IMPLEMENT(Magico)

Magico::Magico(size_t worker_threads) {
    assert(worker_threads >= 1);
    impl = new Impl{worker_threads};
    impl->use_lock = worker_threads == 1? false : true;
}

void Magico::post(Handler&& handler) {
    impl->post(std::move(handler));
}

void Magico::dispatch(Handler&& handler) {
    impl->dispatch(std::move(handler));
}

TimerID Magico::set_timeout(size_t ms, Handler&& handler) {
    return impl->set_timeout(ms, std::move(handler));
}

void Magico::clear(TimerID id) {
    impl->clear(id);
}

bool Magico::poll() {
    return impl->poll();
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

void Magico::Impl::post(Handler&& handler) {
    UseLock lk(posted_m, use_lock);
    count.fetch_add(1, std::memory_order_acquire);
    posted_tasks.push(std::move(handler));
}

void Magico::Impl::dispatch(Handler &&handler) {
    post(std::move(handler));
}

TimerID Magico::Impl::set_timeout(size_t ms, Handler&& handler) {
    UseLock lk(timed_m, use_lock);
    count.fetch_add(1, std::memory_order_acquire);
    auto id = timed_tasks.set_timeout(ms, std::move(handler));
    return id;
}

void Magico::Impl::clear(TimerID id) {
    std::lock_guard lk(timed_m);
    timed_tasks.cancel(id);
    count.fetch_sub(1, std::memory_order_release);
}

plat::IOService& Magico::Impl::get_service() {
    return service;
}

bool Magico::Impl::poll() {
    if (stop_flag.test(std::memory_order_relaxed)) {
        return false;
    }

    for (; ;) {
        Handler task;
        {
            UseLock lk(posted_m, use_lock);
            if (posted_tasks.empty()) {
                break;
            }
            task = std::move(posted_tasks.front());
            posted_tasks.pop();
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
    if (auto ec = service.poll(0)) {
        DEBUG_LOG("main loop error", ec.message());
        return true;
    }

    return count.load() != 0;
}

void Magico::Impl::worker() {
    for (; ;) {
        if (auto ec = service.poll(plat::MAGIO_INFINITE)) {
            DEBUG_LOG(ec.message());
            return;
        }
    }
}

}