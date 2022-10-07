#pragma once

#include "magio/coro/Coro.h"
#include "magio/utils/ScopeGuard.h"

namespace magio {

namespace util {

struct Guard {};

template<typename...Ts>
struct ReverseTupleImpl;

template<typename...Ts>
struct ReverseTupleImpl<std::tuple<Ts...>> {
    using type = std::tuple<Ts...>;
};

template<typename...Ts, typename T, typename...Us>
struct ReverseTupleImpl<std::tuple<Ts...>, T, Us...>
    : ReverseTupleImpl<std::tuple<T, Ts...>, Us...> { };

template<typename T, typename...Ts>
struct ReverseTuple: ReverseTupleImpl<std::tuple<T>, Ts...> { };

}

namespace detail {

enum ChainMethod {
    Sequential, Race, Concurrent
};

template<typename Ret>
Coro<Ret> coro_void_to_unit(Coro<Ret>&& coro) {
    return std::move(coro);
}

Coro<Unit> coro_void_to_unit(Coro<void>&& coro) {
    co_await coro;
    co_return Unit{};
}

template<typename T, typename...Ts>
struct CoroChain {
public:
    using ReturnType = typename util::ReverseTuple<
        util::VoidToUnit<T>,
        util::VoidToUnit<Ts>...
    >::type;

    CoroChain(const CoroChain&) = delete;
    CoroChain(CoroChain&&) noexcept = default;
    CoroChain& operator=(CoroChain&&) noexcept = default;

    CoroChain(ChainMethod method, Coro<T>&& coro, Coro<Ts>&&...coros) 
        : method_(method)
        , coro_tup_(std::make_tuple(std::move(coro), std::move(coros)...))
    { }

    template<size_t N>
    constexpr auto& get() {
        return std::get<N>(coro_tup_);
    }

    auto operator co_await() {
        return invoke(std::move(coro_tup_), std::index_sequence_for<T, Ts...>());
    }
private:
    template<size_t...Idx, typename...Args>
    Coro<ReturnType> invoke(std::tuple<Coro<Args>...>&& tup, std::index_sequence<Idx...>) {
        constexpr size_t Len = sizeof...(Args);
        typename util::ReverseTuple<
            MaybeUninit<util::VoidToUnit<Args>>...
        >::type may_res;

        std::exception_ptr eptr;

        if (method_ == Sequential) {
            ((std::get<Idx>(may_res) = co_await coro_void_to_unit(std::move(std::get<Len - Idx - 1>(tup)))), ...);

            co_return std::apply([](auto&&...args){
                return std::make_tuple(args.unwrap()...);
            }, may_res);
        }

        if (method_ == Concurrent) {
            std::atomic_size_t count = Len;
            auto exe = co_await this_coro::executor;

            co_await Coro<> {
                [&tup, &eptr, &may_res, &count, exe](coroutine_handle<> h) {
                    (std::get<Idx>(tup).set_completion_handler(
                        [&eptr, &may_res, &count, h](Expected<util::VoidToUnit<Args>, std::exception_ptr> exp) mutable {
                            if (!exp) {
                                eptr = exp.unwrap_err();
                            } else {
                                std::get<Len - Idx - 1>(may_res) = exp.unwrap();
                            }

                            count.fetch_sub(1);
                            if (count.load() == 0) {
                                h.resume();
                            }
                        }
                    ), ...);

                    (std::get<Idx>(tup).wake(exe, true), ...);
                }
            };
        }

        if (eptr) {
            std::rethrow_exception(eptr);
        }

        co_return std::apply([](auto&&...args) {
            return std::make_tuple(args.unwrap()...);
        }, may_res);
    }

    ChainMethod method_;
    std::tuple<Coro<T>, Coro<Ts>...> coro_tup_;
};

template<typename Ret, typename...Ts>
detail::CoroChain<Ret, Ts...> operation_impl(
    detail::ChainMethod method, 
    detail::CoroChain<Ts...>&& chain, 
    Coro<Ret>&& coro) 
{
    return [method, chain = std::move(chain), coro = std::move(coro)]<size_t...Idx>(std::index_sequence<Idx...>) mutable {
        return detail::CoroChain<Ret, Ts...>{
            method,
            std::move(coro), 
            std::move(chain.template get<Idx>())...
        };
    }(std::index_sequence_for<Ts...>());
}

}

namespace operation {

template<typename Ret, typename...Ts>
detail::CoroChain<Ret, Ts...> operator| (detail::CoroChain<Ts...>&& chain, Coro<Ret>&& coro) {
    return detail::operation_impl(detail::ChainMethod::Sequential, std::move(chain), std::move(coro));
}

template<typename T, typename U>
detail::CoroChain<U, T> operator| (Coro<T>&& left, Coro<U>&& right) {
    return detail::CoroChain<U, T>{detail::ChainMethod::Sequential, std::move(right), std::move(left)};
}

template<typename Ret, typename...Ts>
detail::CoroChain<Ret, Ts...> operator&& (detail::CoroChain<Ts...>&& chain, Coro<Ret>&& coro) {
    return detail::operation_impl(detail::ChainMethod::Concurrent, std::move(chain), std::move(coro));
}

template<typename T, typename U>
detail::CoroChain<U, T> operator&& (Coro<T>&& left, Coro<U>&& right) {
    return detail::CoroChain<U, T>{detail::ChainMethod::Concurrent, std::move(right), std::move(left)};
}

}

template<typename...Rets>
Coro<std::tuple<util::VoidToUnit<Rets>...>> coro_join(Coro<Rets>&&...coros) {
    detail::CoroChain<Rets...> chain(detail::ChainMethod::Concurrent, std::move(coros)...);
    co_return co_await chain;
}


}