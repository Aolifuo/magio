#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

template<typename Rep, typename Per>
void ticker(const chrono::duration<Rep, Per>& duration, size_t times, Functor<void()>&& func) {
    this_context::expires_after(duration, [duration, times, func = std::move(func)](bool flag) mutable {
        if (!flag) {
            return;
        }
        func();
        --times;
        if (times != 0) {
            ticker(duration, times, std::move(func));
        } else {
            this_context::stop();
        }
    });
}

int main() {
    CoroContext ctx(128);
    ticker(1s, 5, [] {
        M_INFO("{}", "once");
    });
    ctx.start();
}