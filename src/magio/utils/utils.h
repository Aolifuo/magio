#pragma once

#include <tuple>
#include <type_traits>

namespace magio {

namespace util {

template<typename...Ts>
struct TypeList { };

template<typename...>
struct TupleFlatImpl;

template<typename...>
struct TupleFlat;

template<typename...>
struct RemoveVoid;

template<typename>
struct IsTuple: std::false_type {};

template<typename...Ts>
struct TupleFlatImpl<TypeList<Ts...>> {
    using Tuple = std::conditional_t<sizeof...(Ts) == 0, void, std::tuple<Ts...>>;
};

template<typename...Ts, typename...Us, typename...Ks>
struct TupleFlatImpl<TypeList<Ts...>, std::tuple<Us...>, Ks...>
    : TupleFlatImpl<TypeList<Ts..., Us...>, Ks...>{};

template<typename...Ts, typename T, typename...Us>
struct TupleFlatImpl<TypeList<Ts...>, T, Us...>
    : TupleFlatImpl<TypeList<Ts..., T>, Us...> {};

template<typename...Ts, typename...Us>
struct TupleFlatImpl<TypeList<Ts...>, void, Us...>
    : TupleFlatImpl<TypeList<Ts...>, Us...> {};

template<typename...Ts>
struct TupleFlat: TupleFlatImpl<TypeList<>, Ts...> {};

template<typename...Ts>
struct IsTuple<std::tuple<Ts...>>: std::true_type {};

template<typename...Ts>
struct RemoveVoid<TypeList<Ts...>> {
    using Tuple = std::conditional_t<sizeof...(Ts) == 0, void, std::tuple<Ts...>>;
    static constexpr size_t Length = sizeof...(Ts);
};

template<typename...Ts, typename...Us>
struct RemoveVoid<TypeList<Ts...>, void, Us...>
    : RemoveVoid<TypeList<Ts...>, Us...> {};

template<typename...Ts, typename T, typename...Us>
struct RemoveVoid<TypeList<Ts...>, T, Us...>
    : RemoveVoid<TypeList<Ts..., T>, Us...> {};

template<typename...Ts>
struct RemoveVoid: RemoveVoid<TypeList<>, Ts...> {};

}
}