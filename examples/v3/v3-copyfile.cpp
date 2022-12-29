#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> copyfile() {
    std::error_code ec;
    File from("from", File::ReadOnly);
    File to("to", File::WriteOnly | File::Create);

    char buf[1024]{};
    for (; ;) {
        size_t rd = co_await from.read(buf, sizeof(buf), ec);
        if (rd == 0) {
            break;
        }
        co_await to.write(buf, rd, ec);
    }
    this_context::stop();
}

int main() {
    CoroContext ctx(100);
    this_context::spawn(copyfile());
    ctx.start();
}