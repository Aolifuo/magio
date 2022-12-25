#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> copyfile() {
    std::error_code ec;
    auto file1 = File::open("from", File::ReadOnly);
    auto file2 = File::open("to", File::WriteOnly | File::Create);

    char buf[1024]{};
    for (; ;) {
        size_t rd = co_await file1.read(buf, sizeof(buf), ec);
        if (rd == 0) {
            break;
        }
        co_await file2.write(buf, rd, ec);
    }
    this_context::stop();
}

int main() {
    CoroContext ctx(make_unique<net::IoUring>(10));
    this_context::spawn(copyfile());
    ctx.start();
}