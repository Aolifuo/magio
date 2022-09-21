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

TestResults test_Expected() {
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

int main() {
    TESTALL(
        test_maybe_uninit(),
        test_Expected()
    );
}