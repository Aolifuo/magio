#include <cstdio>
#include <exception>
#include "magio/EventLoop.h"
#include "magio/coro/Coro.h"
#include "magio/coro/CoSpawn.h"

using namespace std;
using namespace magio;

Coro<void> amain() {
    try {
        throw exception("Error");
    } catch(const exception& e) {
        printf("Amain: %s\n", e.what());
        throw e;
    }
    co_return;
}

int main() {
    EventLoop loop;

    magio::co_spawn(loop.get_executor(), amain(), [](exception_ptr ep) {
        try {
            if (ep) {
                rethrow_exception(ep);
            }
        } catch(const exception& e) {
            printf("Main: %s\n", e.what());
        }
    });

    loop.run();
}