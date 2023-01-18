#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> copyfile() {
    auto from = File::open("from", File::ReadOnly);
    auto to = File::open("to", File::WriteOnly | File::Create | File::Truncate);
    if (!from || !to) {
        M_FATAL("cannot open file {} or {}", "from", "to");
    }

    char buf[1024];
    for (; ;) {
        error_code ec;
        size_t rd = co_await from.read(buf, sizeof(buf), ec);
        if (ec || rd == 0) {
            break;
        }
        co_await to.write(buf, rd, ec);
    }
    this_context::stop();
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(copyfile());
    ctx.start();
}