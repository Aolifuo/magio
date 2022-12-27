#include <random>
#include <algorithm>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

template<typename Iter>
Coro<> fork_join_sort(ThreadPool& exe, Iter bg, Iter ed, size_t times) {
    using Value = typename iterator_traits<Iter>::value_type;
    if (bg >= ed) {
        co_return;
    }
    Value first = *bg;

    if (times > 0) {
        Iter it = partition(bg, ed, [first](Value& v) {
            return v < first;
        });
        co_await join(
            fork_join_sort(exe, bg, it, times - 1),
            fork_join_sort(exe, it + 1, ed, times - 1)
        );
    } else {
        co_await exe.spawn_blocking([bg, ed] {
            sort(bg, ed);
        });
    }
    co_return;
}

Coro<> amain(ThreadPool& pool) {
    vector<int> vec1(1e8);
    random_device dev;
    default_random_engine eng(dev());
    uniform_int_distribution<> uid;

    for (auto& v : vec1) {
        v = uid(eng);
    }

    auto bg = TimerClock::now();
    sort(vec1.begin(), vec1.end());
    auto dif = TimerClock::now() - bg;
    M_INFO("{}", chrono::duration_cast<chrono::milliseconds>(dif));

    auto vec2 = vec1;
    bg = TimerClock::now();
    co_await fork_join_sort(pool, vec2.begin(), vec2.end(), 3);
    dif = TimerClock::now() - bg;
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