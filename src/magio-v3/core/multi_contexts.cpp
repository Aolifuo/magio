#include "magio-v3/core/multi_contexts.h"

#include <thread>

#include "magio-v3/core/logger.h"

namespace magio {

MultithreadedContexts::MultithreadedContexts(size_t num, size_t every)
    : base_ctx_(LocalContext)
    , every_entries_(every)
    , thread_id_(CurrentThread::get_id())
    , build_ctx_wg_(num)
    , start_wg_(1)
    , contexts_(num)
{
    if (!LocalContext) {
        M_FATAL("{}", "There is no context in this thread");
    }

    if (num == 0) {
        M_FATAL("{}", "num == 0");
    }

    for (size_t i = 0; i < contexts_.size(); ++i) {
        threads_.emplace_back(&MultithreadedContexts::run_in_background, this, i);
    }
    build_ctx_wg_.wait();
}

MultithreadedContexts::~MultithreadedContexts() {
    state_ = PendingDestroy;
    for (size_t i = 0; i < contexts_.size(); ++i) {
        contexts_[i]->execute([this, i] {
            contexts_[i]->stop();
        });
    }

    for (size_t i = 0; i < contexts_.size(); ++i) {
        if (threads_[i].joinable()) {
            threads_[i].join();
        }
    }
}

void MultithreadedContexts::start_all() {
    if (!assert_in_self_thread()) {
        M_FATAL("{}", "you cannot start all contexts in other thread");
    }

    if (state_ != Stopping) {
        M_FATAL("{}", "The contexts have been started");
    }

    state_ = Running;
    start_wg_.done();
    base_ctx_->start();
}

CoroContext& MultithreadedContexts::next_context() {
    size_t old = next_idx_;
    next_idx_ = (next_idx_ + 1) % contexts_.size();
    return *contexts_[old];
}

CoroContext& MultithreadedContexts::get(size_t i) {
    return *contexts_[i % contexts_.size()];
}

void MultithreadedContexts::run_in_background(size_t id) {
    contexts_[id] = std::make_unique<CoroContext>(every_entries_);
    build_ctx_wg_.done();
    start_wg_.wait();
    contexts_[id]->start();
    M_TRACE("{}", "One thread exits from multi contexts");
}

bool MultithreadedContexts::assert_in_self_thread() {
    return thread_id_ == CurrentThread::get_id();
}

}