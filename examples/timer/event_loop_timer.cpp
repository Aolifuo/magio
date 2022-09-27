#include <iostream>
#include "magio/timer/Timer.h"
#include "magio/EventLoop.h"
#include "magio/execution/Execution.h"

using namespace std;

void func(magio::AnyExecutor executor, size_t times) {
    cout << "call once\n";
    if (times == 5) {
        return;
    }
    executor.set_timeout(1000, [=] {
        func(executor, times);
    });
}

int main() {
    magio::EventLoop loop;

    loop.set_timeout(1000, [&loop]{
        func(loop.get_executor(), 1); 
    });
    
    loop.run();
}