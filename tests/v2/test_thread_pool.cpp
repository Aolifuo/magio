#include "magio/magio.h"
#include "preload.h"

size_t sum_from_to(size_t first, size_t last) {
    size_t res = 0;
    for (size_t i = first; i <= last; ++i) {
        res += i;
    }
    return res;
}

TestResults test_wake() {
    ThreadPool pool(8);
    
    auto waiter = make_shared<WaitGroup>(100);
    atomic_size_t sum = 0;

    for (int i = 0; i < 100; ++i) {
        co_spawn(pool.get_executor(), [&sum, waiter] {
            sum += sum_from_to(0, 100);
            waiter->done();
        });
    }

    waiter->wait();
    TESTCASE(
        test(sum.load(), 505000)
    );
}

TestResults test_future() {
    ThreadPool pool(8);

    vector<future<size_t>> res_vec;
    res_vec.reserve(100);
    int a = 1;
    for (int i = 0; i < 100; ++i) {
        auto fut = co_spawn(pool.get_executor(), [a]()->Coro<size_t> {
            co_return sum_from_to(a, 100);
        }(), use_future);
        res_vec.emplace_back(std::move(fut));
    }
    
    size_t sum = 0;
    for (auto& fut: res_vec) {
        sum += fut.get();
    }

    TESTCASE(
        test(sum, 505000)
    );
}

TestResults test_copy_times() {
    ThreadPool pool(8);

    PrintCopy cp;

    co_spawn(pool.get_executor(), [](PrintCopy pc)->Coro<> {
        co_return;
    }(PrintCopy{}), use_future).get();

    co_spawn(pool.get_executor(), [](PrintCopy& pc)->Coro<> {
        co_return;
    }(cp), use_future).get();

    TESTCASE(
        test(PrintCopy::cp_times, 0, "copy times error")
    );
}

int main() {
    TESTALL(
        test_wake(),
        test_future(),
        test_copy_times()
    );
}