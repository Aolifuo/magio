#include <algorithm>
#include <iterator>
#include <ranges>
#include <string>
#include <thread>
#include <vector>
#include "preload.h"
#include "magio/core/Queue.h"

TestResults test_single_single() {
    NotificationQueue<int> sq;
    vector<int> output;

    {
        jthread producer([&sq]{
            for (int i = 0; i < 100; ++i) {
                sq.push(i);
            }
        });

        jthread consumer([&sq, &output]{
            for (int i = 0; i < 100; ++i) {
                output.push_back(sq.try_take().unwrap());
            }
        });
    }

    
    TESTCASE(
        test(ranges::equal(output, views::iota(0, 100)), "")
    );
}

TestResults test_multi_single() {
    NotificationQueue<int> sq;
    vector<int> output;
    output.reserve(100 * 100);

    {
        vector<jthread> producers;

        for (int i = 0; i < 100; ++i) {
            producers.emplace_back([&sq, i]{
                for (int j = 0; j < 100; ++j) {
                    sq.push(i * 100 + j);
                }
            });
        }

        jthread consumer([&] {
            for (int i = 0; i < 100 * 100; ++i) {
                output.push_back(sq.try_take().unwrap());
            }
        });
    }

    ranges::sort(output);
    TESTCASE(
        test(ranges::equal(output, views::iota(0, 100 * 100)), 
            format("Not equal: from {} to {}, totall {}", output.front(), output.back(), output.size()))
    );
}

TestResults test_stop() {
    NotificationQueue<int, RingQueue<int>> sq;
    {
        jthread producer([&sq]{
            sq.try_take().unwrap();
        });

        sq.stop();
    }

    return {};
}

int main() {
    TESTALL(
        test_single_single(),
        test_multi_single(),
        test_stop()
    );
}