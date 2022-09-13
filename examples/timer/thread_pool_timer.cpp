#include <iostream>
#include "magio/ThreadPool.h"

using namespace std;

void func(magio::AnyExecutor executor, size_t times) {
    cout << "call once\n";
    if (times == 5) {
        return;
    }
    executor.set_timeout(1000, [=] {
        func(executor, times + 1);
    });
}


int main() {
    magio::ThreadPool pool(10);

    pool.set_timeout(1000, [&pool]{
        func(pool.get_executor(), 1); 
    });

    pool.join();
}