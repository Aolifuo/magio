#include "preload.h"
#include "magio/sync/WaitGroup.h"
#include <atomic>


TestResults test_threads_wait() {
    vector<jthread> threads;
    atomic_int num;
    auto wg = make_shared<WaitGroup>(100);

    for (size_t i = 0; i < 100; ++i) {
        threads.emplace_back([&num, wg] {
            for (size_t i = 0; i < 100000; ++i) {
                num.fetch_add(1, std::memory_order_relaxed);
            }
            wg->done();
        });
    }

    wg->wait();
    wg->done();
    wg->wait();

    TESTCASE(
        test(num.load(), 10000000, "1")
    );
}

int main() {
    TESTALL(
        test_threads_wait()
    );
}