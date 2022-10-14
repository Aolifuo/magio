#include "preload.h"
#include "magio/Magico.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/Operators.h"

using namespace magio::operators;

Coro<int> get_num(int n) {
    cout << "get num" << n << '\n';
    co_return n;
}

Coro<NoCopy> nocp() {
    co_return NoCopy{};
}


Coro<> may_throw() {
    throw runtime_error("throw error");
    co_return;
}

Coro<> delay(size_t ms) {
    co_await sleep(ms);
    cout << "delay " << ms << '\n';
}


Coro<> test_life_span() {
    cout << "start " << __func__ << '\n';
    co_await sleep(1000);
    cout << "after sleep\n";
    cout << co_await get_num(10) << '\n';
    co_return;
}

Coro<> test_seq() {
    auto seq1 = sleep(1000) | get_num(1);
    auto seq2 = sleep(1000) | get_num(4) | std::move(seq1);
    co_await seq2;
}

Coro<> test_con() {
    auto con1 = delay(500) | delay(100) && sleep(1000) && get_num(7);
    auto con2 = delay(1000) && get_num(9) | std::move(con1);
    auto [a, b] = co_await con2; 
    cout << a << " " << b << '\n'; 
}

Coro<> test_race() {
    auto race1 = delay(1000)|| delay(400) || sleep(900);
    auto race2 = delay(1200) || std::move(race1) || delay(700);
    cout << "beg\n";
    co_await race2;
    cout << "ok\n";
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), test_con(), [](Expected<Unit, exception_ptr> exp) {
        if (!exp) {
            try {
                rethrow_exception(exp.unwrap_err());
            } catch(const exception& err) {
                cout << err.what() << '\n';
            }
        }
    });


    loop.run();
}