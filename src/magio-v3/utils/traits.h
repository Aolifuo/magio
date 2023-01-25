#ifndef MAGIO_UTILS_TRAITS_H_
#define MAGIO_UTILS_TRAITS_H_

#include <tuple>

namespace magio {

namespace detail {

template<typename...Ts>
struct TypeList { };

template<typename, size_t, typename...>
struct FindNonVoidPlace;

template<size_t...Idx, size_t N, typename First, typename...Ts>
struct FindNonVoidPlace<std::index_sequence<Idx...>, N, First, Ts...>
    : FindNonVoidPlace<std::index_sequence<Idx..., N>, N + 1, Ts...>
{ };

template<size_t...Idx, size_t N, typename...Ts>
struct FindNonVoidPlace<std::index_sequence<Idx...>, N, void, Ts...>
    : FindNonVoidPlace<std::index_sequence<Idx..., (size_t)-1>, N, Ts...>
{ };

template<size_t...Idx, size_t N>
struct FindNonVoidPlace<std::index_sequence<Idx...>, N> { 
    using Sequence = std::index_sequence<Idx...>;
};

template<typename...>
struct RemoveVoid;

template<typename...Ts, typename First, typename...Us>
struct RemoveVoid<TypeList<Ts...>, First, Us...>
    : RemoveVoid<TypeList<Ts..., First>, Us...> { };

template<typename...Ts, typename...Us>
struct RemoveVoid<TypeList<Ts...>, void, Us...>
    : RemoveVoid<TypeList<Ts...>, Us...> { };

template<typename...Ts>
struct RemoveVoid<TypeList<Ts...>> {
    using Tuple = std::tuple<Ts...>;
};

}

template<typename...Ts>
using NonVoidPlaceSequence 
    = typename detail::FindNonVoidPlace<std::index_sequence<>, 0, Ts...>::Sequence;

template<typename...Ts>
using RemoveVoidTuple = typename detail::RemoveVoid<detail::TypeList<>, Ts...>::Tuple;

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