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

    Channel() = default;

    Coro<void> send(T elem) {
        // cq
        Entry entry{
            .ctx = LocalContext,
            .value = std::move(elem)
        };

        co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
            entry.handle = h;
            Entry* p = nullptr;
            {
                std::lock_guard lk(mutex_);
                if (sq_.empty()) {
                    cq_.push_back(&entry);
                } else {
                    p = sq_.front();
                    sq_.pop_front();
                }
            }
            if (p) {
                p->value.swap(entry.value);
                p->ctx->wake_in_context(p->handle);
                h.resume();
            }
        });
    }

    Coro<T> receive() {
        // sq
        Entry entry{
            .ctx = LocalContext,
        };

        co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
            entry.handle = h;
            Entry* p = nullptr;
            {
                std::lock_guard lk(mutex_);
                if (cq_.empty()) {
                    sq_.push_back(&entry);
                } else {
                    p = cq_.front();
                    cq_.pop_front();
                }
            }
            if (p) {
                entry.value.swap(p->value);
                p->ctx->wake_in_context(p->handle);
                h.resume();
            }
        });

        co_return std::move(entry.value.value());
    }

private:
    std::mutex mutex_;
    std::deque<Entry*> sq_;
    std::deque<Entry*> cq_;
};

}

#endif