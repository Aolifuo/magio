#include <iostream>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

CoroContext base{1};
ReadablePipe read_end;
WritablePipe write_end;

void stop() {
    read_end.close();
    write_end.close();
    base.execute([] {
        base.stop();
    });
}

Coro<> console_read(WritablePipe& write_end) {
    string line;
    error_code ec;
    
    for (; ;) {
        if (!getline(cin, line) || line.empty()) {
            co_return stop();
        }

        co_await write_end.write(line.c_str(), line.length(), ec);
        if (ec) {
            M_ERROR("write_end error: {}", ec.value());
            co_return stop();
        }
    }
}

Coro<> console_write(ReadablePipe& read_end) {
    char buf[1024];
    error_code ec;
    
    for (; ;) {
        size_t len = co_await read_end.read(buf, sizeof(buf), ec);
        if (ec) {
            M_ERROR("read_end error: {}", ec.value());
            break;
        }
        cout << string_view(buf, len) << '\n';
    }
}

int main() {
    MultithreadedContexts ctxs(2, 2);
    error_code ec;
    tie(read_end, write_end) = make_pipe(ec);
    ctxs.get(0).spawn(console_read(write_end));
    ctxs.get(1).spawn(console_write(read_end));
    ctxs.start_all();
}