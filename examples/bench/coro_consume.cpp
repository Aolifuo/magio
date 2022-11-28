#include "magio/magio.h"

using namespace std;
using namespace magio;

class Counter {
public:
    Counter(string_view name)
        : name_(name)
        , tp(chrono::steady_clock::now()) 
    { }

    ~Counter() {
        auto interval = chrono::steady_clock::now() - tp;
        M_INFO("{}: {}", name_, interval);
    }

private:
    string name_;
    chrono::steady_clock::time_point tp;
};

Coro<> empty_task() {
    co_return;
}

Coro<> test() {
    Counter c("test");
    for (size_t i = 0; i < 100000000; ++i) {
        co_await empty_task();
        co_await this_coro::yield;
    }
}

int main() {
    Magico co;
    co_spawn(co.get_executor(), test());
    co.run();
}