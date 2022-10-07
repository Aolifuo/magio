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
    RingQueue<CompletionHandler>            idle_tasks{64};
    TimingTaskManager                       pending_tasks;
    RingQueue<WaitingCompletionHandler>     waiting_tasks;

    std::mutex                              idle_m;
    std::mutex                              pending_m;
    std::condition_variable                 idle_condvar;
    std::condition_variable                 pending_condvar;
    std::atomic_size_t                      count = 0;

    std::vector<std::thread>                threads;

    void post(CompletionHandler&& handler) override;
    void dispatch(CompletionHandler&& handler) override;
    void waiting(WaitingCompletionHandler&& handler) override;
    TimerID set_timeout(size_t ms, CompletionHandler&& handler) override;
    void clear(TimerID id) override;
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

    start();
}

void ThreadPool::post(CompletionHandler&& handler) {
    impl->post(std::move(handler));
}

void ThreadPool::dispatch(CompletionHandler &&handler) {
    impl->dispatch(std::move(handler));
}

void ThreadPool::waiting(WaitingCompletionHandler&& handler) {
    impl->waiting(std::move(handler));
}

TimerID ThreadPool::set_timeout(size_t ms, CompletionHandler&& handler) {
    return impl->set_timeout(ms, std::move(handler));
}

void ThreadPool::clear(TimerID id) {
    impl->clear(id);
}

void ThreadPool::start() {
    {
        std::scoped_lock lk(impl->idle_m, impl->pending_m);
        impl->state = Impl::Running;
    }
    impl->idle_condvar.notify_all();
    impl->pending_condvar.notify_all();
}

void ThreadPool::stop() {
    std::scoped_lock lk(impl->idle_m, impl->pending_m);
    impl->state = Impl::Stop;
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

void ThreadPool::Impl::post(CompletionHandler&& handler) {
    {
        std::lock_guard lk(idle_m);
        idle_tasks.push(std::move(handler));
    }
    count.fetch_add(1, std::memory_order_relaxed);
    idle_condvar.notify_one();
}

void ThreadPool::Impl::dispatch(CompletionHandler &&handler) {
    post(std::move(handler));
}

void ThreadPool::Impl::waiting(WaitingCompletionHandler&& handler) {
    {
        std::lock_guard lk(pending_m);
        waiting_tasks.emplace(std::move(handler)); 
    }
    count.fetch_add(1, std::memory_order_relaxed);
    pending_condvar.notify_one();
}

TimerID ThreadPool::Impl::set_timeout(size_t ms, CompletionHandler&& handler) {
    TimerID id;
    {
        std::lock_guard lk(pending_m);
        id = pending_tasks.set_timeout(ms, std::move(handler));
    }
    count.fetch_add(1, std::memory_order_relaxed);
    pending_condvar.notify_one();
    return id;
}

void ThreadPool::Impl::clear(TimerID id) {
    {
        std::lock_guard lk(pending_m);
        pending_tasks.cancel(id);
    }
    count.fetch_sub(1);
}

void ThreadPool::Impl::wait() {
    for (; ;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (count.load(std::memory_order_seq_cst) == 0) {
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
        std::scoped_lock lk(idle_m, pending_m);
        state = PendingDestroy;
    }
    idle_condvar.notify_all();
    pending_condvar.notify_all();
}   

void ThreadPool::Impl::worker() {
    for (; ;) {
        std::unique_lock lk(idle_m);
        idle_condvar.wait(lk, [this] {
            return (state == Running && !idle_tasks.empty()) || state == PendingDestroy;
        });

        if (state == PendingDestroy) {
            return;
        }

        auto task = std::move(idle_tasks.front());
        idle_tasks.pop();
        lk.unlock();

        task();
        count.fetch_sub(1);
    }
}

void ThreadPool::Impl::poller() {
    for (; ;) {
        std::unique_lock lk(pending_m);
        pending_condvar.wait(lk, [this] {
            return (state == Running && (!pending_tasks.empty() || !waiting_tasks.empty())) 
                || state == PendingDestroy;
        });

        if (state == PendingDestroy) {
            return;
        }

        for (; ;) {
            if (auto res = pending_tasks.get_expired_task(); res) {
                lk.unlock();
                post(res.unwrap());
                count.fetch_sub(1);
                lk.lock();
            } else {
                break;
            }
        }

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
}

}