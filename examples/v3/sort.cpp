#include <random>
#include <algorithm>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

template<typename Iter, typename Pred>
Coro<> fork_join_sort(ThreadPool& exe, Iter bg, Iter ed, Pred pred, size_t times) {
    if (bg >= ed) {
        co_return;
    }

    if (times > 0) {
        auto mid = bg + (ed - bg) / 2;
        nth_element(bg, mid, ed, pred);
        co_await join(
            fork_join_sort(exe, bg, mid, pred, times - 1),
            fork_join_sort(exe, mid + 1, ed, pred, times - 1)
        );
    } else {
        co_await exe.spawn_blocking([bg, ed, pred] {
            sort(bg, ed, pred);
        });
    }
    co_return;
}

Coro<> amain(ThreadPool& pool) {
    vector<int> vec(1e8);
    random_device dev;
    default_random_engine eng(dev());
    uniform_int_distribution<> uid;
    for (auto& v : vec) {
        v = uid(eng);
    }
    
    auto bg = TimerClock::now();
    // sort(vec.begin(), vec.end(), greater<>());
    co_await fork_join_sort(pool, vec.begin(), vec.end(), greater<>(), 3);
    auto dif = TimerClock::now() - bg;
    M_INFO("{}", chrono::duration_cast<chrono::milliseconds>(dif));

    co_return this_context::stop();
}

int main() {
    CoroContext ctx(128);
    ThreadPool pool(8);
    ctx.spawn(amain(pool));
    pool.start();
    ctx.start();
}