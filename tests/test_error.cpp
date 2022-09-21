#include "preload.h"
#include "magio/core/MaybeUninit.h"
#include "magio/core/Error.h"

struct Foo {
    Foo() {
        ++COUNT;
    }

    Foo(const Foo& other) {
        ++COUNT;
    }

    Foo(Foo&& other) {
        ++COUNT;
    }

    Foo& operator=(const Foo& other) {
        return *this;
    }

    Foo& operator=(Foo&& other) {
        return *this;
    }

    ~Foo() {
        --COUNT;
    }

    inline static int COUNT = 0;
};

struct NoCopy {
    NoCopy() = default;
    NoCopy(NoCopy &&) = default;
    NoCopy& operator=(NoCopy &&) = default;
};

TestResults test_maybe_uninit() {
    Foo::COUNT = 0;
    {
        MaybeUninit<Foo> f1{Foo{}};
        MaybeUninit<Foo> f2;
        f2 = std::move(f1);
        MaybeUninit<Foo> f3{std::move(f2)};
        Foo f;
        f3 = f;
        MaybeUninit<Foo> f4{f3};
        MaybeUninit<Foo> f5;
        f5 = f4;
    }

    {
        MaybeUninit<NoCopy> n1{NoCopy{}};
        MaybeUninit<NoCopy> n2{std::move(n1)};
        MaybeUninit<NoCopy> n3;
        n3 = std::move(n2);
    }

    TESTCASE(
        test(Foo::COUNT, 0, "")
    );
}

TestResults test_expected() {
    Foo::COUNT = 0;
    {
        Expected<Foo> f1{Foo{}};
        Expected<Foo> f2{std::move(f1)};
        Expected<Foo> f3{f2};
        Expected<Foo> f4{Error{"fuckfuck"}};
        f4 = f3;
        f4 = std::move(f2);
        Expected<Foo> f5{f4};
        f5 = Error{"ass"};
    }

    {
        Expected<NoCopy> n1{NoCopy{}};
        Expected<NoCopy> n2{std::move(n1)};
        Expected<NoCopy> n3{Error{""}};
        n3 = std::move(n2);
    }

    TESTCASE(
        test(Foo::COUNT, 0, "")
    );
}

TestResults test_error_chain() {
    auto f1 = [] {
        return Expected<NoCopy>(NoCopy());
    };

    auto f2 = [](NoCopy nc) {
        return Expected<string>("hello");
    };

    auto f3 = [](string s) {
        return Expected<NoCopy>(Error("failed"));
    };

    auto f4 = [](Error err) {
        return Expected<NoCopy>(NoCopy());
    };

    auto res = f1()
        .and_then(f2)
        .and_then(f3);

    res.unwrap_err();

    auto res3 = f1()
        .and_then(f2)
        .and_then(f3)
        .or_else(f4);

    res3.unwrap();

    return {};
}

TestResults test_map() {

    auto f1 = [] {
        return Expected<int>(1);
    };

    auto f2 = [] {
        return Expected<>(Error("ss"));
    };

    auto res = f1()
        .map([](int a) {
            return NoCopy();
        })
        .map([](NoCopy c) {
            return string("hh");
        });

    res.unwrap();

    auto res2 = f2()
        .map([](Unit u) {
            return 1;
        });
    
    res2.unwrap_err();

    return {};
}

int main() {
    
    TESTALL(
        test_maybe_uninit(),
        test_expected(),
        test_error_chain(),
        test_map()
    );
}