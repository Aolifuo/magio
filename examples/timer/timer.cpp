#include <iostream>
#include "magio/Timer.h"
#include "magio/EventLoop.h"

using namespace std;

int main() {
    magio::EventLoop loop;

    magio::Timer timer(loop.get_executor(), 2000);
    timer.async_wait([] { cout << "hello\n";  });

    loop.run();
}