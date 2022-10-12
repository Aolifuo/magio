#include <iostream>
#include "magio/timer/Timer.h"
#include "magio/Magico.h"

using namespace std;

int main() {
    magio::Magico loop;

    magio::Timer timer(loop.get_executor(), 2000);
    timer.async_wait([] { cout << "hello\n";  });

    loop.run();
}