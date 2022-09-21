#include "magio/ThreadPool.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/Coro.h"
#include <thread>
#include <iostream>

using namespace std;
using namespace magio;

Coro<void> func() {
    cout << this_thread::get_id() << '\n';
    co_return;
}

int main() {
    ThreadPool pool(10);

    co_spawn(
        pool.get_executor(),
        coro_join(func(), func(), func(), func()),
        detached);
    
    pool.join();
}