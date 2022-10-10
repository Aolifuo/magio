#include "preload.h"
#include "magio/ThreadPool.h"
#include "magio/sync/WaitGroup.h"

size_t sum_from_to(size_t frist, size_t last) {
    size_t res = 0;
    for (size_t i = frist; i <= last; ++i) {
        res += i;
    }
    return res;
}

TestResults test_post() {
    ThreadPool pool(8);
    
    auto waiter = make_shared<WaitGroup>(100);
    atomic_size_t sum = 0;

    for (int i = 0; i < 100; ++i) {
        pool.post([&sum, waiter] {
            sum += sum_from_to(1, 100);
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
        res_vec.emplace_back(pool.get_future(sum_from_to, a, 100));
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

    pool.get_future([](PrintCopy pc) {

    }, PrintCopy{});

    pool.get_future([](PrintCopy& rpc) {

    }, std::ref(cp));

    TESTCASE(
        test(PrintCopy::cp_times, 1, "copy times error")
    );
}

int main() {
    TESTALL(
        test_future(),
        test_copy_times()
    );
}