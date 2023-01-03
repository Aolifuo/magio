#include <filesystem>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

namespace fs = filesystem;

Coro<> count_lines_impl(string path, Mutex& co_m, Condition& cond, size_t& lines, size_t& count_down) {
    char buf[1024];
    std::error_code ec;
    File file(path.c_str(), File::ReadOnly);
    if (!file) {
        M_ERROR("cannot open {}", path);
        co_return;
    }
    for (; ;) {
        size_t len = co_await file.read(buf, sizeof(buf), ec);
        if (ec || len == 0) {
            break;
        }
        string_view sv(buf, len);
        for (size_t pos = 0; (pos = sv.find("\n", pos)) != string_view::npos;) {
            ++pos;
            ++lines;
        }
    }

    --count_down;
    if (0 == count_down) {
        cond.notify_one();
    }
}

Coro<> count_lines(const char* dir_name) {
    size_t lines = 0;
    size_t count_down = 0;
    size_t file_num = 0;
    Mutex co_m;
    Condition cond;
    if (fs::is_regular_file(dir_name)) {
        count_down = 1;
        this_context::spawn(count_lines_impl(dir_name, co_m, cond, lines, count_down));
    } else {
        for (auto entry : fs::recursive_directory_iterator(dir_name)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            ++count_down;
            ++file_num;
            this_context::spawn(count_lines_impl(entry.path().string(), co_m, cond, lines, count_down));
        }
    }

    auto lock = co_await co_m.lock_guard();
    co_await cond.wait(lock, [&] { return 0 == count_down; });
    M_INFO("file num: {}, lines: {}", file_num, lines);
    co_return this_context::stop();
}

int main() {
    CoroContext ctx(128);
    ctx.spawn(count_lines("path/to/dir"));
    ctx.start();
}