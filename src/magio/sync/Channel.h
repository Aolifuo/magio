#pragma once

#include "magio/core/Execution.h"
#include "magio/core/MaybeUninit.h"
#include "magio/core/Queue.h"
#include "magio/coro/Coro.h"

namespace magio {

template<typename...Ts>
requires (std::is_object_v<Ts> && ...)
class Channel: public std::enable_shared_from_this<Channel<Ts...>> {
public:
    enum State{
        Stop, Running
    };

    Channel() = default;
    Channel(AnyExecutor executor): executor_(executor) {}

    void async_send(Ts...args) {
        std::unique_lock lk(m_);
        if (state_ == Stop) {
            return;
        }

        if (!consumer_.empty()) {
            auto fn = std::move(consumer_.front());
            consumer_.pop();
            lk.unlock();
            fn(std::move(args)...);
        } else {
            producer_.push(std::make_tuple(std::move(args)...));
        }
    }

    template<typename Fn>
    requires std::is_invocable_v<Fn, Ts...>
    void async_receive(Fn fn) {
        std::unique_lock lk(m_);
        if (state_ == Stop) {
            return;
        }

        if (!producer_.empty()) {
            auto tup = std::move(producer_.front());
            producer_.pop();
            lk.unlock();
            
            std::apply([&fn](auto&&...args) {
                fn(std::move(args)...);
            }, tup);
            return;
        }

        consumer_.push(std::move(fn));
        lk.unlock();

        if (executor_) {
            executor_.waiting([p = this->shared_from_this()] {
                std::lock_guard lk(p->m_);
                return p->consumer_.empty();
            });
        }
    }

    Coro<std::tuple<Ts...>> async_receive(UseCoro) {
        std::unique_lock lk(m_);
        if (state_ == Stop) {
            throw std::runtime_error("The channel is in a stopped state");
        }

        if (!producer_.empty()) {
            auto tup = std::move(producer_.front());
            producer_.pop();
            co_return tup;
        }

        MaybeUninit<std::tuple<Ts...>> ret;
        co_await Coro<void>{
            [&lk, &ret, exe = executor_, p = this->shared_from_this()](std::coroutine_handle<> h) mutable {
                p->consumer_.push([&ret, h](Ts...args) {
                    ret = std::make_tuple(std::move(args)...);
                    h.resume();
                });
                lk.unlock();

                if (exe) {
                    exe.waiting([p] {
                        std::lock_guard lk(p->m_);
                        return p->consumer_.empty();
                    });
                }
                
            }
        };

        co_return ret.unwrap();
    }

    void stop() {
        std::unique_lock lk(m_);
        state_ = Stop;
        producer_.clear();
        consumer_.clear();
    }

    void start() {
        std::unique_lock lk(m_);
        state_ = Running;
    }
private:
    State state_ = Running;
    AnyExecutor executor_;
    RingQueue<std::tuple<Ts...>> producer_;
    RingQueue<std::function<void(Ts...)>> consumer_;
    std::mutex m_;
};

}