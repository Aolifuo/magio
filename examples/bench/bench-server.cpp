#include <iostream>
#include "magio/magio.h"

using namespace std;
using namespace magio;

//  ./ab.exe -n 1000000 -c 1000 -k http://127.0.0.1:8000/
// wrk

const string HEAHER =
    "HTTP/1.1 200 OK\r\n"
    "Server: Asio\r\n"
    "Accept: */*\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 274\r\n"
    "\r\n";

const char HTML[] = 
    R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Document</title>
</head>
<body>
    Hello world
</body>
</html>)";


const string RESP = HEAHER + HTML;

Coro<void> process(TcpStream stream) {
    try {
        char buf[1024];
        for (; ;) {
            co_await stream.read(buf, sizeof(buf));
            co_await stream.write(RESP.c_str(), RESP.length());
        }
    } catch (const runtime_error& e) {
        // cout << e.what() << '\n';
    }
}

Coro<void> amain(Magico& loop) {
    try {
        auto server = co_await TcpServer::bind("0.0.0.0", 8000);
        for (; ;) {
            auto stream = co_await server.accept();
            co_spawn(co_await this_coro::executor, process(std::move(stream)));
        }
    } catch (const runtime_error& e) {
        cout << e.what() << '\n';
    }
}

int main() {
    Magico loop(16);
    co_spawn(loop.get_executor(), amain(loop));
    loop.run();
}