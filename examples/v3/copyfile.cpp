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
        size_t rd = co_await from.read(buf, sizeof(buf)) | throw_on_err;
        if (rd == 0) {
            break;
        }
        co_await to.write(buf, rd) | throw_on_err;
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(copyfile(), [](exception_ptr eptr, Unit) {
        try {
            try_rethrow(eptr);
        } catch(const system_error& err) {
            M_ERROR("{}", err.what());
        }
        this_context::stop();
    });
    ctx.start();
}