#include "preload.h"
#include "magio/Magico.h"


int main() {
    Magico loop;

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