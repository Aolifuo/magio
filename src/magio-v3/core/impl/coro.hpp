#ifndef MAGIO_CORE_IMPL_CORO_H_
#define MAGIO_CORE_IMPL_CORO_H_

namespace magio {

template<typename...Ts>
inline Coro<> select(Coro<Ts>...coros) {
    auto flag = std::make_shared<bool>(false);
    std::exception_ptr eptr;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) mutable {
        (this_context::spawn(coros, [flag, h, &eptr](std::exception_ptr ep, VoidToUnit<Ts> ret) mutable {
            if (*flag) {
                return;
            }

            *flag = true;
            if (ep) {
                eptr = ep;
            }
            h.resume();
        }), ...);
    });

    if (eptr) {
        std::rethrow_exception(eptr);
    }
}

template<typename...Ts>
inline Coro<RemoveVoidTuple<Ts...>> join(Coro<Ts>...coros) {
    auto count = std::make_shared<size_t>(sizeof...(coros));
    std::exception_ptr eptr;
    RemoveVoidTuple<Ts...> result;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        [&]<size_t...Idx>(std::index_sequence<Idx...>) {
            
            (this_context::spawn(coros, [&eptr, &result, count, h](std::exception_ptr ep, VoidToUnit<Ts> ret) mutable {
                if (*count == 0) {
                    return;
                }

                *count = *count - 1;
                if (ep) {
                    eptr = ep;
                    *count = 0;
                    h.resume();
                } else {
                    if constexpr (Idx != std::numeric_limits<size_t>::max()) {
                        std::get<Idx>(result) = std::move(ret);  
                    }
                    
                    if (*count == 0) {
                        h.resume();
                    }
                }
            }), ...);
        }(NonVoidPlaceSequence<Ts...>{});
    });

    if (eptr) {
        std::rethrow_exception(eptr);
    }

    co_return result;
}

template<typename...Ts>
inline Coro<RemoveVoidTuple<Ts...>> series(Coro<Ts>...coros) {
    RemoveVoidTuple<Ts...> result;

    co_await [&]<size_t...Idx>(std::index_sequence<Idx...>) -> Coro<> {
        (co_await [&]() mutable -> Coro<> {
            if constexpr (Idx != std::numeric_limits<size_t>::max()) {
                std::get<Idx>(result) = co_await coros;
            } else {
                co_await coros;
            }
        }(), ...);
    }(NonVoidPlaceSequence<Ts...>{});

    co_return result;
}

namespace this_coro {

template<typename Rep, typename Per>
inline Coro<> sleep_for(const std::chrono::duration<Rep, Per>& dur) {
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        this_context::expires_after(dur, [h](bool flag) {
            if (flag) {
                h.resume();
            }
        });
    });
}

inline Coro<> sleep_until(const TimerClock::time_point& tp) {
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        this_context::expires_until(tp, [h](bool flag) {
            if (flag) {
                h.resume();
            }
        });
    });
}

}

}

#endif