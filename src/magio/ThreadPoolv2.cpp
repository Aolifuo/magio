#include "magio/ThreadPoolv2.h"

#include <cstdio>
#include "magio/core/Queue.h"
#include "magio/core/TimingTask.h"

namespace magio {

struct ThreadPoolv2::Impl: public ExecutionContext {
    enum State {
        Stop, Running, PendingDestroy
    };
    State state = Stop;
    RingQueue<CompletionHandler> idle_tasks;
    TimingTaskManager pending_tasks;
    std::list<WaitingCompletionHandler> waiting_task;

    std::mutex idle_m;
    std::mutex pending_m;
    std::condition_variable idle_condvar;
    std::condition_variable pending_condvar;

    std::vector<std::jthread> threads;

    void post(CompletionHandler handler) override;
    void waiting(WaitingCompletionHandler handler) override;
    TimerID set_timeout(size_t ms, CompletionHandler handler) override;
    void clear(TimerID id) override;
    bool poll() override;
    void wait();
    void join();
    void detach();
    void destroy();

    void worker();
    void poller();
};

CLASS_PIMPL_IMPLEMENT(ThreadPoolv2)

ThreadPoolv2::ThreadPoolv2(size_t thread_num) {
    impl = new Impl;

    for (size_t i = 0; i < thread_num; ++i) {
        impl->threads.emplace_back(&Impl::worker, impl);
    }
    impl->threads.emplace_back(&Impl::poller, impl);

    start();
}

void ThreadPoolv2::post(CompletionHandler handler) {
    impl->post(std::move(handler));
}

void ThreadPoolv2::waiting(WaitingCompletionHandler handler) {
    impl->waiting(std::move(handler));
}

TimerID ThreadPoolv2::set_timeout(size_t ms, CompletionHandler handler) {
    return impl->set_timeout(ms, std::move(handler));
}

void ThreadPoolv2::clear(TimerID id) {
    impl->clear(id);
}

void ThreadPoolv2::start() {
    std::scoped_lock lk(impl->idle_m, impl->pending_m);
    impl->state = Impl::Running;
    impl->idle_condvar.notify_all();
    impl->pending_condvar.notify_all();
}

void ThreadPoolv2::stop() {
    std::scoped_lock lk(impl->idle_m, impl->pending_m);
    impl->state = Impl::Stop;
}

void ThreadPoolv2::wait() {
    impl->wait();
}

void ThreadPoolv2::join() {
    impl->join();
}

void ThreadPoolv2::detach() {
    impl->detach();
}

AnyExecutor ThreadPoolv2::get_executor() {
    return AnyExecutor(impl);
}

void ThreadPoolv2::Impl::post(CompletionHandler handler) {
    std::lock_guard lk(idle_m);
    idle_tasks.push(std::move(handler));
    idle_condvar.notify_one();
}

void ThreadPoolv2::Impl::waiting(WaitingCompletionHandler handler) {
    std::lock_guard lk(pending_m);
    waiting_task.push_back(std::move(handler));
    pending_condvar.notify_one();
}

TimerID ThreadPoolv2::Impl::set_timeout(size_t ms, CompletionHandler handler) {
    std::lock_guard lk(pending_m);
    TimerID id = pending_tasks.set_timeout(ms, std::move(handler));
    pending_condvar.notify_one();
    return id;
}

void ThreadPoolv2::Impl::clear(TimerID id) {
    std::lock_guard lk(pending_m);
    pending_tasks.cancel(id);
}

void ThreadPoolv2::Impl::wait() {
    for (; ;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::scoped_lock lk(idle_m, pending_m);
        if (idle_tasks.empty() && pending_tasks.empty() && waiting_task.empty()) {
            return;
        }
    }
}

void ThreadPoolv2::Impl::join() {
    wait();
    destroy();
    for (auto& th : threads) {
        th.join();
    }
}

void ThreadPoolv2::Impl::detach() {
    
}

void ThreadPoolv2::Impl::destroy() {
    std::scoped_lock lk(idle_m, pending_m);
    state = PendingDestroy;
    idle_condvar.notify_all();
    pending_condvar.notify_all();
}   

void ThreadPoolv2::Impl::worker() {
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
    }
}

void ThreadPoolv2::Impl::poller() {
    for (; ;) {
        std::unique_lock lk(pending_m);
        pending_condvar.wait(lk, [this] {
            return (state == Running && (!pending_tasks.empty() || !waiting_task.empty())) 
                || state == PendingDestroy;
        });

        if (state == PendingDestroy) {
            return;
        }

        for (; ;) {
            if (auto res = pending_tasks.get_expired_task(); res) {
                lk.unlock();
                post(res.unwrap());
                lk.lock();
            } else {
                break;
            }
        }

        for (auto it = waiting_task.begin(); it != waiting_task.end(); ++it) {
            if ((*it)()) {
                waiting_task.erase(it);
            }
        }
    }
}

bool ThreadPoolv2::Impl::poll() {
    return true;
}

}