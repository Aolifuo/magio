#include <iostream>
#include "magio/timer/Timer.h"
#include "magio/Magico.h"

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
    magio::Magico loop(1);

    loop.set_timeout(1000, [&loop]{
        func(loop.get_executor(), 1); 
    });
    
    loop.run();
}