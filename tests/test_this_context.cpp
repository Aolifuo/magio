#include "preload.h"
#include "magio/ThisContext.h"


Coro<PrintCopy> task() {
    cout << "task\n";
    co_await timeout(1000);
    cout << "timeout\n";
    co_return {};
}

Coro<> amain() {
    this_context::spawn(task());
    cout << "amain\n";
    co_return;
}

int main() {
    this_context::spawn(amain(), [](Expected<Unit, exception_ptr> exp) {
        cout << "end\n";
    });
    this_context::run();
}