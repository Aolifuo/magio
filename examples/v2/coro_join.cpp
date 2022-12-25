#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace operators;

Coro<int> factorial(std::string_view name, int num) {
    int res = 1;

    for (int i = 2; i <= num; ++i) {
        M_INFO("Task {}: Compute factorial {}, now i = {}", name, num, i);
        co_await sleep(1000);
        res *= i;
    }
    M_INFO("Task {}: factorial {} = {}", name, num, res);
    co_return res;
}

int main() {
    Magico loop(1);
    co_spawn(
        loop.get_executor(),
        []() -> Coro<> {
            auto [a, b, c] = co_await(
                factorial("A", 2) &&
                factorial("B", 3) &&
                factorial("C", 4)
            );
            assert(a == 2);
            assert(b == 6);
            assert(c == 24);
        }()
    );
    loop.run();

    MAGIO_CHECK_RESOURCE;
}