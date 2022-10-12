#include "magio/magio.h"

using namespace std;
using namespace magio;

Coro<int> factorial(std::string_view name, int num) {
    int res = 1;

    for (int i = 2; i <= num; ++i) {
        printf("Task %s: Compute factorial %d, now i = %d\n", name.data(), num, i);
        co_await timeout(1000);
        res *= i;
    }
    printf("Task %s: factorial %d = %d\n", name.data(), num, res);
    co_return res;
}

int main() {
    Magico loop(1);
    co_spawn(
        loop.get_executor(),
        []() -> Coro<> {
            auto [a, b, c] = co_await coro_join(
                factorial("A", 2),
                factorial("B", 3),
                factorial("C", 4)
            );
            printf("%d %d %d\n", a, b, c);
        }()
    );
    loop.run();
}