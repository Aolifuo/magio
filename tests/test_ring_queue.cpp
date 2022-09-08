#include <algorithm>
#include <functional>
#include <ranges>
#include <vector>
#include "preload.h"
#include "magio/core/Queue.h"

struct NoCopy {
    NoCopy(int i) {
        ++NUM;
        value = i;
    }

    NoCopy(NoCopy&& other) {
        ++NUM;
        value = other.value;
    }

    ~NoCopy() {
        --NUM;
    }


    inline static int NUM = 0;
    int value = 0;
};

TestResults test_push() {
    RingQueue<NoCopy> queue{};

    ranges::for_each(views::iota(0, 100), [&](int i){ queue.push({i}); });
    
    TESTCASE(
        test(NoCopy::NUM, 100, "Num is not 100"),
        test(queue.size(), 100, "Queue size is not 100")
    );
}

TestResults test_pop() {
    RingQueue<NoCopy> queue(100);
    vector<int> out;

    ranges::for_each(views::iota(0, 100), [&](int i){ queue.push({i}); });
    for (int i = 0; i < 50; ++i) {
        out.push_back(queue.front().value);
        queue.pop();
    }
    ranges::for_each(views::iota(100, 150), [&](int i) { queue.push({i}); });

    TESTCASE(
        test(NoCopy::NUM, 100, "Num is not 100"),
        test(ranges::equal(out, views::iota(0, 50)), "Not equal"),
        test(queue.size(), 100, "Queue size is not 100")
    );
}

TestResults test_end() {
    RingQueue<function<void()>> queue(10);

    TESTCASE(
        test(NoCopy::NUM, 0, "Not 0")
    );
}

int main() {
    TESTALL(
        test_push(),
        test_pop(),
        test_end()
    );
}