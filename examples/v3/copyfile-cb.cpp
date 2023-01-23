#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

class CopyFile {
public:
    CopyFile(const char* p1, const char* p2)
        : from_(File::open(p1, File::ReadOnly))
        , to_(File::open(p2, File::WriteOnly | File::Create | File::Truncate)) 
    {
        if (!from_ || !to_) {
            M_FATAL("cannot open {} or {}", p1, p2);
        }
    }

    void start() {
        read();
    }

private:
    void read() {
        from_.read(buf_, sizeof(buf_), [&](error_code ec, size_t len) {
            if (ec || len == 0) {
                M_ERROR("read error: {}", ec ? ec.message() : "EOF");
                return this_context::stop();
            }
            write(len);
        });
    }

    void write(size_t len) {
        to_.write(buf_, len, [&](error_code ec, size_t len) {
            if (ec) {
                M_ERROR("write error: {}", ec.message());
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
    CopyFile cf("from", "to");
    cf.start();
    ctx.start();
}