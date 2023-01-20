#include <filesystem>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

namespace fs = filesystem;

class CountLines {
public:
    CountLines(const char* dir_name): dir_name_(dir_name)
    { }

    void start() {
        this_context::spawn(count_all());
    }

private:
    Coro<> count_all() {
        size_t file_num = 0;
        if (!fs::exists(dir_name_) || !fs::is_directory(dir_name_)) {
            M_FATAL("{} does not exit or is not dir", dir_name_);
        }

        for (auto& entry : fs::recursive_directory_iterator(dir_name_)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            count_down_++;
            this_context::spawn(count_one_file(entry.path().string()));
        }

        file_num = count_down_;
        auto lock = co_await mutex_.lock_guard();
        co_await cond_.wait(lock, [&] { return count_down_ == 0; });
        M_INFO("file num: {}, lines: {}", file_num, lines_);
        this_context::stop();
    }

    Coro<> count_one_file(string path) {
        char buf[1024];
        auto file = File::open(path.c_str(), File::ReadOnly);
        if (!file) {
            M_ERROR("cannot open {}", path);
            co_return;
        }

        for (; ;) {
            error_code ec;
            size_t len = co_await file.read(buf, sizeof(buf), ec);
            if (ec || len == 0) {
                break;
            }
            string_view sv(buf, len);
            for (size_t pos = 0; (pos = sv.find('\n', pos)) != string_view::npos;) {
                ++pos;
                ++lines_;
            }
        }

        --count_down_;
        if (count_down_ == 0) {
            cond_.notify_all();
        }
    }

    Mutex mutex_;
    Condition cond_;

    string dir_name_;
    size_t count_down_ = 0;
    size_t lines_ = 0;
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        M_FATAL("{}", "please input one dir name");
    }

    CoroContext ctx(128);
    CountLines cl(argv[1]);
    cl.start();
    ctx.start();
}