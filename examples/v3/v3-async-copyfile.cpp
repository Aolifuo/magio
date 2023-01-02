#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

class CopyFile {
public:
    void from_to(const char* p1, const char* p2) {
        from_.open(p1, File::ReadOnly);
        to_.open(p2, File::WriteOnly | File::Create | File::Truncate);

        if (!from_ || !to_) {
            M_FATAL("from: {}, to: {}", (bool)from_, (bool)to_);
        }
        read();
    }

private:
    void read() {
        from_.read(buf_, sizeof(buf_), [&](std::error_code ec, size_t len) {
            if (ec || len == 0) {
                M_ERROR("read error: {}", len == 0 ? "EOF" : to_string(ec.value()));
                return this_context::stop();
            }
            write(len);
        });
    }

    void write(size_t len) {
        to_.write(buf_, len, [&](std::error_code ec, size_t len) {
            if (ec) {
                M_ERROR("write error: {}", ec.value());
                return this_context::stop();
            }
            read();
        });
    }

    File from_;
    File to_;
    char buf_[1024];
};

int main() {
    CoroContext ctx(128);
    CopyFile cf;
    cf.from_to("from", "to");
    ctx.start();
}