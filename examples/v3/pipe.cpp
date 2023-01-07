#include <iostream>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

class ConsoleReadWrite {
public:
    ConsoleReadWrite(CoroContextPool& pool): ctx_pool_(pool)
    { }

    void start() {
        ctx_pool_.next_context().spawn(console_read(), [](exception_ptr eptr, Unit) {
            this_context::stop();
        });
        ctx_pool_.next_context().spawn(console_write(), [](exception_ptr eptr, Unit) {
            this_context::stop();
        });
    }

private:
    Coro<> console_read() {
        string line;
        
        for (; ;) {
            error_code ec;
            if (!getline(cin, line) || line.empty()) {
                break;
            }

            co_await write_end_.write(line.c_str(), line.length(), ec);
            if (ec) {
                M_ERROR("write_end error: {}", ec.message());
                break;
            }
        }

        read_end_.close();
        write_end_.close();
    }

    Coro<> console_write() {
        char buf[1024];
        
        for (; ;) {
            error_code ec;
            size_t len = co_await read_end_.read(buf, sizeof(buf), ec);
            if (ec) {
                M_ERROR("read_end error: {}", ec.message());
                break;
            }
            cout << string_view(buf, len) << '\n';
        }
    }

    CoroContextPool& ctx_pool_;
    ReadablePipe read_end_;
    WritablePipe write_end_;
};

int main() {
    CoroContextPool ctxs(2, 2);
    ConsoleReadWrite crw(ctxs);
    crw.start();
    ctxs.start_all();
}