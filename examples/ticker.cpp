#include "magio/magio.h"

using namespace std;
using namespace magio;

Coro<> ticker(size_t times, size_t ms, Handler func) {
    for (size_t i = 0; i < times; ++i) {
        co_await sleep(ms);
        func();
    }
}

int main() {
    Magico loop;
    co_spawn(
        loop.get_executor(),
        ticker(10, 1000, [] {
            cout << "tick" << endl;
        })
    );
    loop.run();
}