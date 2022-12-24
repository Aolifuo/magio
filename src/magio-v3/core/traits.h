#ifndef MAGIO_CORE_TRAITS_H_
#define MAGIO_CORE_TRAITS_H_

#include <tuple>

namespace magio {

template<bool Cond>
using constraint = std::enable_if_t<Cond, int>;

// function 
// function noexcept
// function pointer
// function pointer noexcept

// memberfunc pointer
// memberfunc pointer const
// memberfunc pointer noexcept
// memberfunc pointer const noexcept

// Functor

template<typename>
struct Function;

template<typename Ret, typename...Args>
struct Function<Ret(Args...)> {
    using ReturnType = Ret;
    using Params = std::tuple<Args...>;
};

template<typename Functor>
struct Function
    : Function<decltype(&Functor::operator())> { };

template<typename Ret, typename...Args>
struct Function<Ret(Args...) noexcept>
    : Function<Ret(Args...)> { };

template<typename Ret, typename...Args>
struct Function<Ret(*)(Args...)>
    : Function<Ret(Args...)> { };

template<typename Ret, typename...Args>
struct Function<Ret(*)(Args...) noexcept>
    : Function<Ret(Args...)> { };

template<typename Class, typename Ret, typename...Args>
struct Function<Ret(Class::*)(Args...)>
    : Function<Ret(Args...)> { };

template<typename Class, typename Ret, typename...Args>
struct Function<Ret(Class::*)(Args...) const>
    : Function<Ret(Args...)> { };

template<typename Class, typename Ret, typename...Args>
struct Function<Ret(Class::*)(Args...) noexcept>
    : Function<Ret(Args...)> { };

template<typename Class, typename Ret, typename...Args>
struct Function<Ret(Class::*)(Args...) const noexcept>
    : Function<Ret(Args...)> { };

// is_range
// std::begin() std::end()

template<typename, typename = void>
struct IsRange: std::false_type { };

template<typename Range>
struct IsRange<Range, std::void_t<decltype(std::declval<Range>().begin()), decltype(std::declval<Range>().end())>>
    : std::true_type { };

template<typename Range>
struct RangeTraits {
    using Iterator = decltype(std::declval<Range>().begin());
    using ValueType = std::remove_reference_t<decltype(*std::declval<Range>().begin())>;
};

}

#endif