#include <cstdio>
#include <exception>
#include "magio/Magico.h"
#include "magio/core/Error.h"
#include "magio/coro/Coro.h"
#include "magio/coro/CoSpawn.h"

using namespace std;
using namespace magio;

Coro<void> amain() {
    try {
        throw runtime_error("Error");
    } catch(const exception& e) {
        printf("Amain: %s\n", e.what());
        throw e;
    }
    co_return;
}

int main() {
    Magico loop(1);

    magio::co_spawn(loop.get_executor(), amain(), [](exception_ptr eptr) {
        try {
            if (eptr) {
                rethrow_exception(eptr);
            }
        } catch(const exception& e) {
            printf("Main: %s\n", e.what());
        }
    });

    loop.run();
}