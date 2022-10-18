#pragma once

#include "magio/core/MaybeUninit.h"
#include "magio/coro/Coro.h"

namespace magio {

namespace detail {

template<typename T, typename U>
typename util::Flat<T, U>::Tuple tuple_return_impl(T&& t, U&& u) {
    if constexpr (util::IsTuple<T>::value && util::IsTuple<U>::value) {
        return std::tuple_cat(std::move(t), std::move(u));
    } else if constexpr (util::IsTuple<T>::value) {
        return std::apply([u = std::move(u)](auto&&...args) mutable {
            return std::make_tuple(std::move(args)..., std::move(u));
        }, t);
    } else if constexpr (util::IsTuple<U>::value) {
        return std::apply([t = std::move(t)](auto&&...args) mutable {
            return std::make_tuple(std::move(t), std::move(args)...);
        }, u);
    } else {
        return {std::move(t), std::move(u)};
    }
}

template<typename T, typename U>
Coro<typename util::Flat<T, U>::Tuple> seq_coro_impl(Coro<T> coro1, Coro<U> coro2) {
    if constexpr (std::is_void_v<T> && std::is_void_v<U>) {
        co_await coro1;
        co_await coro2;
        co_return;
    } else if constexpr (std::is_void_v<T>) {
        co_await coro1;
        auto u = co_await coro2;

        co_return std::move(u);
    } else if constexpr (std::is_void_v<U>) {
        auto t = co_await coro1;
        co_await coro2;

        co_return std::move(t);
    } else {
        auto t = co_await coro1;
        auto u = co_await coro2;

        co_return tuple_return_impl(std::move(t), std::move(u));
    }
}

template<typename T, typename U>
Coro<typename util::Flat<T, U>::Tuple> con_coro_impl(Coro<T> coro1, Coro<U> coro2) {
    std::atomic_int count = 2;
    std::exception_ptr eptr;
    MaybeUninit<util::VoidToUnit<T>> left;
    MaybeUninit<util::VoidToUnit<U>> right;

    co_await Awaitable {
        [&](AnyExecutor exe, Waker waker, size_t _) {
            coro1.set_completion_handler(
                [&count, &eptr, &left, waker](Expected<util::VoidToUnit<T>, std::exception_ptr> exp) {
                    --count;
                    if (!exp) {
                        eptr = exp.unwrap_err(); 
                    } else {
                        left = exp.unwrap();
                    }
                    
                    if (0 == count) {
                        waker.wake();
                    }
                });

            coro2.set_completion_handler(
                [&count, &eptr, &right, waker](Expected<util::VoidToUnit<U>, std::exception_ptr> exp) {
                    --count;
                    if (!exp) {
                        eptr = exp.unwrap_err();
                    } else {
                        right = exp.unwrap();
                    }
                    
                    if (0 == count) {
                        waker.wake();
                    }
                });

            coro1.wake(exe);
            coro2.wake(exe);
        }
    };

    if (eptr) {
        std::rethrow_exception(eptr);
    }

    if constexpr (std::is_void_v<T> && std::is_void_v<U>) {
        co_return;
    } else if constexpr (std::is_void_v<T>) {
        co_return right.unwrap();
    } else if constexpr (std::is_void_v<U>) {
        co_return left.unwrap();
    } else {
        co_return tuple_return_impl(left.unwrap(), right.unwrap());
    }
}

// template<typename T, typename U>
// Coro<> race_coro_impl(Coro<T> coro1, Coro<U> coro2) {
//     // TODO
//     co_return;
// }

}

namespace operators {

template<typename Left, typename Right>
auto operator| (Coro<Left>&& coro1, Coro<Right>&& coro2) {
    return detail::seq_coro_impl(std::move(coro1), std::move(coro2));
}

template<typename Left, typename Right>
auto operator&& (Coro<Left>&& coro1, Coro<Right>&& coro2) {
    return detail::con_coro_impl(std::move(coro1), std::move(coro2));
}

// template<typename Left, typename Right>
// auto operator|| (Coro<Left>&& coro1, Coro<Right>&& coro2) {
//     return detail::race_coro_impl(std::move(coro1), std::move(coro2));
// }

}

}