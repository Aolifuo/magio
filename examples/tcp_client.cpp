#include <array>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace magio::operation;

Coro<> amain(Magico& loop) {
    try {
        auto stream = co_await TcpStream::connect("127.0.0.1", 8000);

        cout << stream.local_address().to_string()
             << " connect "
             << stream.remote_address().to_string()
             << '\n';

        array<char, 1024> buf;
        for (int i = 0; i < 5; ++i) {
            auto [_, rdlen] = co_await (
                stream.write("Hello server..", 14) &&
                stream.read(buf.data(), buf.size())
            );
            cout << string_view(buf.data(), rdlen) << '\n';
        }
    } catch(const std::exception& err) {
        cout <<  err.what() << '\n';
    }

    loop.stop();
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain(loop));
    loop.run();
}