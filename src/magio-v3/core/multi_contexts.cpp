#include "magio-v3/core/multi_contexts.h"

#include <thread>

#include "magio-v3/core/logger.h"

namespace magio {

MultithreadedContexts::MultithreadedContexts(size_t num, size_t every)
    : every_entries_(every)
    , wg_(num)
    , contexts_(num)
{
    if (!LocalContext) {
        M_FATAL("{}", "There is no context in this thread");
    }
    thread_id_ = CurrentThread::get_id();
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
    for (size_t i = 0; i < contexts_.size(); ++i) {
        threads_.emplace_back(&MultithreadedContexts::run_in_background, this, i);
    }
    wg_.wait();
}

CoroContext& MultithreadedContexts::next_context() {
    size_t old = next_idx_;
    next_idx_ = (next_idx_ + 1) % contexts_.size();
    return *contexts_[old];
}

void MultithreadedContexts::run_in_background(size_t id) {
    contexts_[id] = std::make_unique<CoroContext>(every_entries_);
    wg_.done();
    contexts_[id]->start();
    M_TRACE("{}", "One thread exits from multi contexts");
}

bool MultithreadedContexts::assert_in_self_thread() {
    return thread_id_ == CurrentThread::get_id();
}

}