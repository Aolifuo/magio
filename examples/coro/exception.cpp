#include <cstdio>
#include <exception>
#include "magio/EventLoop.h"
#include "magio/core/Error.h"
#include "magio/coro/Coro.h"
#include "magio/coro/CoSpawn.h"

using namespace std;
using namespace magio;

Coro<string> amain() {
    try {
        throw runtime_error("Error");
    } catch(const exception& e) {
        printf("Amain: %s\n", e.what());
        throw e;
    }
    co_return "a";
}

int main() {
    EventLoop loop;

    magio::co_spawn(loop.get_executor(), amain(), [](Expected<string, exception_ptr> exp) {
        try {
            if (!exp) {
                rethrow_exception(exp.unwrap_err());
            }
        } catch(const exception& e) {
            printf("Main: %s\n", e.what());
        }
    });

    loop.run();
}