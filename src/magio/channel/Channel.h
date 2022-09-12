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
    Channel(AnyExecutor executor)
        : exeutor_(executor)
    {}

    void async_send(Ts...args) {
        data_queue_.push(std::make_tuple(std::move(args)...));
    }

    template<typename Fn>
    requires std::is_invocable_v<Fn, Ts...>
    void async_receive(Fn fn) {
        exeutor_.waiting([fn = std::move(fn), p = this->shared_from_this()] {
            if (auto res = p->data_queue_.try_take()) {
                auto tup = res.unwrap();
                std::apply([&fn](auto&&...args) {
                    fn(std::move(args)...);
                }, tup);

                return true;
            } else {
                return false;
            }
            
        });
    }

    Coro<std::tuple<Ts...>> async_receive(UseCoro) {
        MaybeUninit<std::tuple<Ts...>> ret;
        co_await Coro<void>{
            [&ret, exe = exeutor_, p = this->shared_from_this()](std::coroutine_handle<> h) mutable {
                exe.waiting([&ret, exe, h, p]() mutable {
                    if (auto res = p->data_queue_.try_take()) {
                        ret = std::move(res);
                        exe.post([h] { h.resume(); });
                        return true;
                    } else {
                        return false;
                    }
                });
            }
        };

        co_return ret.unwrap();
    }
private:
    AnyExecutor exeutor_;
    BlockedQueue<std::tuple<Ts...>> data_queue_;
};

}