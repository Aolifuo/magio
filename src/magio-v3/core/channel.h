#ifndef MAGIO_CORE_CHANNEL_H_
#define MAGIO_CORE_CHANNEL_H_

#include "magio-v3/core/coro_context.h"

namespace magio {

// thread safe
template<typename T>
class Channel: Noncopyable {
public:
    struct Entry {
        CoroContext* ctx;
        std::coroutine_handle<> handle;
        std::optional<T> value;
    };

    Channel(size_t n = 0) {
        cache_ = n;
    }

    Coro<void> send(T elem) {
        // cq
        auto entry = std::make_unique<Entry>(
            Entry{
                .ctx = LocalContext,
                .value = std::move(elem)
            }
        );

        co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
            entry->handle = h;
            std::unique_ptr<Entry> p;
            bool flag = false;
            {
                std::lock_guard lk(mutex_);
                if (sq_.empty()) {
                    if (cq_.size() < cache_) {
                        flag = true;
                        entry->ctx = nullptr; // resume now
                    }
                    cq_.push_back(std::move(entry));
                } else {
                    p = std::move(sq_.front());
                    sq_.pop_front();
                }
            }
            if (p) {
                p->ctx->wake_in_context(p->handle);
                h.resume();
            } else if (flag) {
                h.resume();
            }
        });
    }

    Coro<T> receive() {
        // sq
        auto entry = std::make_unique<Entry>(
            Entry{
                .ctx = LocalContext
            }
        );

        std::optional<T> result;
        co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
            entry->handle = h;
            std::unique_ptr<Entry> p;
            {
                std::lock_guard lk(mutex_);
                if (cq_.empty()) {
                    sq_.push_back(std::move(entry));
                } else {
                    p = std::move(cq_.front());
                    cq_.pop_front();
                }
            }
            if (p) {
                result.swap(p->value);
                if (p->ctx) {
                    p->ctx->wake_in_context(p->handle);
                }
                h.resume();
            }
        });

        co_return std::move(result.value());
    }

private:
    std::mutex mutex_;
    size_t cache_;
    std::deque<std::unique_ptr<Entry>> sq_;
    std::deque<std::unique_ptr<Entry>> cq_;
};

}

#endif