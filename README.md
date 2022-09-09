# magio

---
magio是一个基于事件循环和线程池的协程库

```cpp
Coro<void> amain(const char* host, short port) {
    try {
        std::array<char, 1024> buf;

        auto client = co_await TcpClient::create();
        auto conn = co_await client.connect(host, port);

        for (int i = 0; i < 5; ++i) {
            co_await conn.send("Hello server", 12);
            size_t recv_len = co_await conn.receive(buf.data(), buf.size());
            cout << string_view(buf.data(), recv_len) << '\n';
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain("127.0.0.1", 8080), detached);
    loop.run();
}
```

```cpp
Coro<void> process(TcpStream stream) {
    array<char, 1024> buf;
    try {
        for (; ;) {
            size_t read_len = co_await stream.read(buf.data(), buf.size());
            cout << string_view(buf.data(), read_len) << '\n';
            co_await stream.write("Hello client", 12);
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

Coro<void> amain(const char* host, short port) {
    try {
        auto executor = co_await this_coro::executor;
        auto server = co_await TcpServer::bind(host, port);
        for (; ;) {
            auto stream = co_await server.accept();
            co_spawn(executor, process(std::move(stream)), detached);
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain("127.0.0.1", 8080), detached);
    loop.run();
}
```
