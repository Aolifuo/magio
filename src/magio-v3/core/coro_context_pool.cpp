#include "magio-v3/core/coro_context_pool.h"

#include <thread>

#include "magio-v3/core/logger.h"

namespace magio {

CoroContextPool::CoroContextPool(size_t num, size_t every)
    : every_entries_(every)
    , thread_id_(CurrentThread::get_id())
    , build_ctx_wg_(num - 1)
    , start_wg_(1)
    , contexts_(num)
{
    if (num < 1) {
        M_FATAL("{}", "num must >= 1");
    }

    contexts_[0] = std::make_unique<CoroContext>(every);
    for (size_t i = 1; i < contexts_.size(); ++i) {
        threads_.emplace_back(&CoroContextPool::run_in_background, this, i);
    }
    build_ctx_wg_.wait();
}

CoroContextPool::~CoroContextPool() {
    state_ = PendingDestroy;
    for (size_t i = 0; i < contexts_.size(); ++i) {
        contexts_[i]->execute([this, i] {
            contexts_[i]->stop();
        });
    }

    for (size_t i = 0; i < threads_.size(); ++i) {
        if (threads_[i].joinable()) {
            threads_[i].join();
        }
    }
}

void CoroContextPool::start_all() {
    if (!assert_in_self_thread()) {
        M_FATAL("{}", "you cannot start all contexts in other thread");
    }

    if (state_ != Stopping) {
        M_FATAL("{}", "The contexts have been started");
    }

    state_ = Running;
    start_wg_.done();
    contexts_[0]->start();
}

CoroContext& CoroContextPool::next_context() {
    size_t old = next_idx_;
    next_idx_ = (next_idx_ + 1) % contexts_.size();
    return *contexts_[old];
}

CoroContext& CoroContextPool::get(size_t i) {
    return *contexts_[i % contexts_.size()];
}

void CoroContextPool::run_in_background(size_t id) {
    contexts_[id] = std::make_unique<CoroContext>(every_entries_);
    build_ctx_wg_.done();
    start_wg_.wait();
    contexts_[id]->start();
    M_TRACE("{}", "One thread exits from multi contexts");
}

bool CoroContextPool::assert_in_self_thread() {
    return thread_id_ == CurrentThread::get_id();
}

}