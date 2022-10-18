#pragma once

#include <tuple>
#include <type_traits>

namespace magio {

namespace util {

template<typename...Ts>
struct TypeList { };

template<typename...>
struct FlatImpl;

template<typename...>
struct Flat;

template<typename...>
struct RemoveVoid;

template<typename>
struct IsTuple: std::false_type {};

template<typename...Ts>
struct FlatImpl<TypeList<Ts...>> {
    using Tuple = std::conditional_t<sizeof...(Ts) == 0, void, std::tuple<Ts...>>;
};

template<template<typename...> typename C, typename...Ts, typename...Us, typename...Ks>
struct FlatImpl<TypeList<Ts...>, C<Us...>, Ks...>
    : FlatImpl<TypeList<Ts..., Us...>, Ks...>{};

template<typename...Ts, typename T, typename...Us>
struct FlatImpl<TypeList<Ts...>, T, Us...>
    : FlatImpl<TypeList<Ts..., T>, Us...> {};

template<typename...Ts, typename...Us>
struct FlatImpl<TypeList<Ts...>, void, Us...>
    : FlatImpl<TypeList<Ts...>, Us...> {};

template<typename...Ts>
struct Flat: FlatImpl<TypeList<>, Ts...> {};

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