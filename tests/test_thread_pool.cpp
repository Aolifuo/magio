#include <chrono>
#include <future>
#include <thread>
#include <type_traits>
#include <vector>
#include "preload.h"
#include "magio/ThreadPool.h"

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
    
    return {};
}

TestResults test_future() {
    ThreadPool pool(10);

    auto fut1 = pool.get_future([](int a, int b) {
        return a + b;
    }, 1, 2);
    
    string str;
    auto fut2 = pool.get_future([](string& s) {
        s = "hello";
    }, ref(str));

    
    pool.join();
    TESTCASE(
        test(fut1.get(), 3, "1"),
        test(str, "hello", "2")
    );
}

int main() {
    TESTALL(
        test_execute(),
        test_future()
    );
}