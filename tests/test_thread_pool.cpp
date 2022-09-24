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
    ThreadPool pool(16);
    
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
    ThreadPool pool(16);

    vector<future<size_t>> res_vec;
    for (int i = 0; i < 100; ++i) {
        res_vec.emplace_back(pool.get_future(sum_from_to, 1, 100));
    }
    
    size_t sum = 0;
    for (auto& fut: res_vec) {
        sum += fut.get();
    }

    TESTCASE(
        test(sum, 505000)
    );
}

int main() {
    TESTALL(
        test_future()
    );
}