#include "magio/EventLoop.h"
#include "magio/ThreadPoolv2.h"
#include "magio/Timer.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/Coro.h"
#include "magio/coro/ThisCoro.h"
#include <exception>

using namespace std;
using namespace magio;

Coro<int> get_num(int n) {
    std::printf("get num %d\n", n);
    co_return n;
}

Coro<void> may_throw() {
    throw exception("error");
    co_return;
}

Coro<void> amain() {
    auto executor = co_await this_coro::executor;
    Timer timer(executor, 1000);

    auto [a, b, c] = 
        co_await coro_join(timer.async_wait(use_coro), get_num(1), get_num(2));

    std::printf("%d + %d = %d\n", b, c, b + c);

    auto [d, e, _] = co_await coro_join(may_throw(), get_num(4), get_num(114514));
    
    std::printf("final %d\n", e);
}

int main() {
    //EventLoop loop;
    ThreadPoolv2 loop(10);
    co_spawn(loop.get_executor(), amain(), 
        [](std::exception_ptr e) {
            try {
                if (e) {
                    rethrow_exception(e);
                }
            } catch(const exception& e) {
                printf("%s\n", e.what());
            }
            
        });
    //loop.run();
    loop.join();
}