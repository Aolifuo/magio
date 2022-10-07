#include "preload.h"
#include "magio/EventLoop.h"

int main() {
    EventLoop loop;

    bool flag = false;

    loop.waiting([&] {
        if (flag) {
            cout << "over\n";
            return true;
        }
        return false;
    });

    loop.set_timeout(2000, [&] {
        flag = true;
    });

    loop.run();
}