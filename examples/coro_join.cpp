#include <cassert>
#include "magio/Timer.h"
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/ThisCoro.h"

using namespace std;
using namespace magio;

Coro<int> factorial(std::string_view name, int num) {
    int res = 1;
    Timer timer(co_await this_coro::executor, 1000);

    for (int i = 2; i <= num; ++i) {
        printf("Task %s: Compute factorial %d, now i = %d\n", name.data(), num, i);
        co_await timer.async_wait(use_coro);
        res *= i;
    }
    printf("Task %s: factorial %d = %d\n", name.data(), num, res);
    co_return res;
}

int main() {
    EventLoop loop;
    co_spawn(
        loop.get_executor(), 
        []() -> Coro<void> {
            auto [a, b, c] = co_await coro_join(
                factorial("A", 2),
                factorial("B", 3),
                factorial("C", 4)
            );
            assert(a == 2);
            assert(b == 6);
            assert(c = 24);
        }(),
        detached
    );
    loop.run();
}