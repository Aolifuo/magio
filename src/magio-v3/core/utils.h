#ifndef MAGIO_CORE_UTILS_H_
#define MAGIO_CORE_UTILS_H_

#include <tuple>
#include <optional>

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
    : FindNonVoidPlace<std::index_sequence<Idx..., std::numeric_limits<size_t>::max()>, N, Ts...>
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
    using TupleOption = std::tuple<std::optional<Ts>...>;
};

}

template<typename...Ts>
using NonVoidPlaceSequence 
    = typename detail::FindNonVoidPlace<std::index_sequence<>, 0, Ts...>::Sequence;

template<typename...Ts>
using RemoveVoidTuple = typename detail::RemoveVoid<detail::TypeList<>, Ts...>::Tuple;

}

#endif