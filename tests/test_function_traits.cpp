#include <utility>
#include "preload.h"
#include "magio/utils/Function.h"

struct Foo {
    int& fun();
    void fun2(int[3], const char*) const noexcept;
    double&& operator()() const noexcept;
};

TestResults test_return_type() {
    auto g = [](int)->string { return ""; };

    TESTCASE(
        test(same_as<typename FunctorTraits<int(int, double)>::ReturnType, int>),
        test(same_as<typename FunctorTraits<void()>::ReturnType, void>),
        test(same_as<typename FunctorTraits<decltype(&Foo::fun)>::ReturnType, int&>),
        test(same_as<typename FunctorTraits<decltype(g)>::ReturnType, string>),
        test(same_as<typename FunctorTraits<Foo>::ReturnType, double&&>)
    );
}

TestResults test_arguments() {
    auto g = [](vector<int>, void(*)(int)){};

    TESTCASE(
        test(same_as<typename FunctorTraits<void(int, string)>::Arguments, TypeList<int, string>>, "1"),
        test(same_as<typename FunctorTraits<decltype(&Foo::fun2)>::Arguments, TypeList<int*, const char*>>, "2"),
        test(same_as<typename FunctorTraits<decltype(g)>::Arguments, TypeList<vector<int>, void(*)(int)>>, "3"),

        test(FunctorTraits<void(int, double, char)>::Arguments::Length, 3, "4"),
        test(FunctorTraits<void()>::Arguments::Length, 0, "5"),
        
        test(same_as<typename FunctorTraits<void(short, int, long)>::Arguments::At<1>, int>, "6"),
        test(same_as<typename FunctorTraits<void(char, size_t)>::Arguments::At<1>, size_t>, "7"),

        test(FunctorTraits<void(int, double)>::Arguments::Eq<int, double>, "8")
    );
}


int main() {
    TESTALL(
        test_return_type(),
        test_arguments()
    );
}