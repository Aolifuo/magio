#include "preload.h"
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/Operation.h"

using namespace magio::operation;

Coro<int> get_int(int n) {
    cout << n << '\n';
    co_return n;
}

Coro<double> get_double(double n) {
    cout << n << '\n';
    
    co_return n;
}

Coro<string> get_string(string_view s) {
    cout << s << '\n';
    co_await timeout(2000);
    co_return string(s);
}

Coro<void> may_throw() {
    cout << "ttttt" << '\n';
    // throw std::runtime_error("error");
    co_return;
}

Coro<void> test() {
    // auto chain1 = get_int(1) | get_string("hello") | may_throw() | get_double(4.3);
    // auto [a, b, c, d] = co_await chain1;
    co_await get_int(1);
    co_await get_string("hello");
    co_await may_throw();
    co_await get_double(4.3);

    // auto chain2 = get_double(2.2) && get_string("world") && get_int(33) && may_throw();
    // auto [a, b, c, d] = co_await chain2;
}

int main() {
    EventLoop loop;
    co_spawn(loop.get_executor(), test(), [](Expected<Unit, exception_ptr> ep) {
        if (!ep) {
            try {
                rethrow_exception(ep.unwrap_err());
            } catch(const exception& err) {
                cout << err.what() << '\n';
            }
        }
    });
    loop.run();
}