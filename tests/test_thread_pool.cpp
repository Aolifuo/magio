#include <chrono>
#include <future>
#include <thread>
#include <vector>
#include "preload.h"
#include "magio/core/ThreadPool.h"

struct Foo {
    explicit Foo(int n): n(n) {}

    Foo(const Foo& o) {
        n = o.n;
        ++copy_nums;
    }

    Foo(Foo&& o) {
        n = o.n;
    }

    int n = 0;
    inline static int copy_nums = 0;
};

TestResults test_execute() {
    int res1 = 0, res2 = 0, res3 = 0;
    Foo res4{0};
    int tmp = 1;
    {
        Foo::copy_nums = 0;
        ThreadPool pool(10);
        pool.start();
        
        pool.execute([](int a, int b, int& res) { res = a + b; }, 1, 2, ref(res1));
        pool.execute([](Foo a, int b, int& res) { res = a.n + b; }, Foo{1}, 2, ref(res2));
        pool.execute([foo = Foo{1}](int a, int& res) { res = foo.n + a; }, 2, ref(res3));
        pool.execute([](const int& a, int b){  }, ref(tmp), 2);
        pool.execute([foo = std::move(res4)](int a, int b) mutable { foo.n = foo.n + a; }, 1, 2);
        auto fut = pool.submit([]{});

        fut.wait();
    }
    
    TESTCASE(
        test(res1, 3, "1"),
        test(res2, 3, "2"),
        test(res3, 3, "3"),
        test(Foo::copy_nums, 0, "4"),
    );
}

TestResults test_submit() {
    Foo::copy_nums = 0;
    ThreadPool pool(2);
    pool.start();

    int t1 = 2;
    Foo t2{2};

    auto f1 = pool.submit([](int a, int b){ return a + b; }, 1, 2);
    auto f2 = pool.submit([](int a, int& b){ return a + b; }, 1, ref(t1));
    auto f3 = pool.submit([](int a, Foo b){ return a + b.n; }, 1, Foo{2});
    auto f4 = pool.submit([](int a, Foo&& b){ return a + b.n; }, 1, Foo{2});
    auto f5 = pool.submit([foo = Foo{2}](int a){ return a + foo.n; }, 1);
    auto f6 = pool.submit([](int a, Foo b){ return Foo{a + b.n}; }, 1, t2);

    TESTCASE(
        test(f1.get(), 3, "1"),
        test(f2.get(), 3, "2"),
        test(f3.get(), 3, "3"),
        test(f4.get(), 3, "4"),
        test(f5.get(), 3, "5"),
        test(f6.get().n, 3, "6"),
        test(Foo::copy_nums, 1, "7")
    );
}

int main() {
    TESTALL(
        test_execute(),
        test_submit()
    );
}