# Benchmark

- [Benchmark](#benchmark)
  - [Result](#result)
    - [Cpp asio](#cpp-asio)
    - [Cpp libuv](#cpp-libuv)
    - [Cpp magio](#cpp-magio)
    - [Rust tokio](#rust-tokio)
    - [NodeJs](#nodejs)
  - [Test code](#test-code)
    - [Cpp asio code](#cpp-asio-code)
    - [Cpp libuv code](#cpp-libuv-code)
    - [Cpp magio code](#cpp-magio-code)
    - [Rust tokio code](#rust-tokio-code)
    - [NodeJs code](#nodejs-code)

## Result

> Test command: ab -n 1000000 -c 1000 -k <http://127.0.0.1:8000/>

### Cpp asio

```shell
Server Software:
Server Hostname:        127.0.0.1
Server Port:            8000

Document Path:          /
Document Length:        0 bytes

Concurrency Level:      1000
Time taken for tests:   23.591 seconds
Complete requests:      1000000
Failed requests:        0
Non-2xx responses:      1000000
Keep-Alive requests:    1000000
Total transferred:      106000000 bytes
HTML transferred:       0 bytes
Requests per second:    42389.54 [#/sec] (mean)
Time per request:       23.591 [ms] (mean)
Time per request:       0.024 [ms] (mean, across all concurrent requests)
Transfer rate:          4387.98 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.0      0       1
Processing:     2   24   4.3     23     160
Waiting:        0   23   4.3     23     160
Total:          2   24   4.3     23     160

Percentage of the requests served within a certain time (ms)
  50%     23
  66%     23
  75%     24
  80%     24
  90%     27
  95%     30
  98%     33
  99%     37
 100%    160 (longest request)
```

### Cpp libuv

```shell
Server Software:
Server Hostname:        127.0.0.1
Server Port:            8000

Document Path:          /
Document Length:        0 bytes

Concurrency Level:      1000
Time taken for tests:   23.043 seconds
Complete requests:      1000000
Failed requests:        0
Non-2xx responses:      1000000
Keep-Alive requests:    1000000
Total transferred:      106000000 bytes
HTML transferred:       0 bytes
Requests per second:    43397.97 [#/sec] (mean)
Time per request:       23.043 [ms] (mean)
Time per request:       0.023 [ms] (mean, across all concurrent requests)
Transfer rate:          4492.37 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.0      0       1
Processing:     2   23   3.9     23     151
Waiting:        0   23   3.9     23     151
Total:          2   23   3.9     23     151

Percentage of the requests served within a certain time (ms)
  50%     23
  66%     23
  75%     23
  80%     24
  90%     26
  95%     28
  98%     31
  99%     33
 100%    151 (longest request)
```

### Cpp magio

```shell
Server Software:
Server Hostname:        127.0.0.1
Server Port:            8000

Document Path:          /
Document Length:        0 bytes

Concurrency Level:      1000
Time taken for tests:   23.219 seconds
Complete requests:      1000000
Failed requests:        0
Non-2xx responses:      1000000
Keep-Alive requests:    1000000
Total transferred:      106000000 bytes
HTML transferred:       0 bytes
Requests per second:    43068.93 [#/sec] (mean)
Time per request:       23.219 [ms] (mean)
Time per request:       0.023 [ms] (mean, across all concurrent requests)
Transfer rate:          4458.31 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.0      0       1
Processing:     2   23   4.4     23     153
Waiting:        0   23   4.4     23     153
Total:          2   23   4.4     23     153

Percentage of the requests served within a certain time (ms)
  50%     23
  66%     23
  75%     23
  80%     24
  90%     26
  95%     29
  98%     36
  99%     39
 100%    153 (longest request)
```

### Rust tokio

```shell
Server Software:
Server Hostname:        127.0.0.1
Server Port:            8000

Document Path:          /
Document Length:        0 bytes

Concurrency Level:      1000
Time taken for tests:   21.638 seconds
Complete requests:      1000000
Failed requests:        0
Non-2xx responses:      1000000
Keep-Alive requests:    1000000
Total transferred:      106000000 bytes
HTML transferred:       0 bytes
Requests per second:    46215.20 [#/sec] (mean)
Time per request:       21.638 [ms] (mean)
Time per request:       0.022 [ms] (mean, across all concurrent requests)
Transfer rate:          4784.00 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.0      0      16
Processing:     0   22  38.2     17     700
Waiting:        0   22  38.2     17     700
Total:          0   22  38.2     17     700

Percentage of the requests served within a certain time (ms)
  50%     17
  66%     19
  75%     22
  80%     23
  90%     32
  95%     34
  98%     78
  99%    199
 100%    700 (longest request)
```

### NodeJs

``` shell
Server Software:
Server Hostname:        127.0.0.1
Server Port:            8000

Document Path:          /
Document Length:        0 bytes

Concurrency Level:      1000
Time taken for tests:   25.450 seconds
Complete requests:      1000000
Failed requests:        0
Non-2xx responses:      1000000
Keep-Alive requests:    1000000
Total transferred:      106000000 bytes
HTML transferred:       0 bytes
Requests per second:    39292.92 [#/sec] (mean)
Time per request:       25.450 [ms] (mean)
Time per request:       0.025 [ms] (mean, across all concurrent requests)
Transfer rate:          4067.43 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.0      0       1
Processing:     0   25   7.4     26     172
Waiting:        0   25   7.4     26     172
Total:          0   25   7.4     26     173

Percentage of the requests served within a certain time (ms)
  50%     26
  66%     28
  75%     31
  80%     32
  90%     34
  95%     36
  98%     39
  99%     41
 100%    173 (longest request)
```

## Test code

### Cpp asio code

```cpp
#include "asio/awaitable.hpp"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"

using namespace asio;
using namespace std;

awaitable<void> handle_connection(ip::tcp::socket sock) {
    std::array<char, 1024> buf;
    for (; ;) {
        auto rdlen = co_await sock.async_receive(buffer(buf), use_awaitable);
        co_await sock.async_send(buffer(buf, rdlen), use_awaitable);
    }
}

awaitable<void> amain() {
    auto exe = co_await this_coro::executor;
    ip::tcp::endpoint ep(ip::make_address("127.0.0.1"), 8000);
    ip::tcp::acceptor acceptor(exe, ep);

    for (; ;) {
        ip::tcp::socket sock(exe);
        co_await acceptor.async_accept(sock, use_awaitable);
        co_spawn(exe, handle_connection(std::move(sock)), detached);
    }
}

int main() {
    io_context ctx(1);
    co_spawn(ctx, amain(), detached);
    ctx.run();
}
```

### Cpp libuv code

```cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uv.h"

typedef struct {
    uv_write_t  req;
    uv_buf_t    buf;
} uv_write_req_t;

void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

void close_cb(uv_handle_t* handle) {
    free(handle);
}

void write_cb(uv_write_t* req, int status) {
    uv_write_req_t* wr = (uv_write_req_t*)req;
    free(wr->buf.base);
    free(wr);
}

void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if (nread <= 0) {
        uv_close((uv_handle_t*)stream, close_cb);
        return;
    }
    uv_write_req_t* wr = (uv_write_req_t*)malloc(sizeof(uv_write_req_t));
    wr->buf.base = buf->base;
    wr->buf.len = nread;
    uv_write(&wr->req, stream, &wr->buf, 1, write_cb);
}

void do_accept(uv_stream_t* server, int status) {
    uv_tcp_t* tcp_stream = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(server->loop, tcp_stream);
    uv_accept(server, (uv_stream_t*)tcp_stream);
    uv_read_start((uv_stream_t*)tcp_stream, alloc_cb, read_cb);
}

int main() {
    uv_loop_t loop;
    uv_loop_init(&loop);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    uv_ip4_addr("127.0.0.1", 8000, &addr);

    uv_tcp_t tcp_server;
    uv_tcp_init(&loop, &tcp_server);
    int ret = uv_tcp_bind(&tcp_server, (struct sockaddr*)&addr, 0);
    if (ret) {
        return -20;
    }
    ret = uv_listen((uv_stream_t*)&tcp_server, SOMAXCONN, do_accept);
    if (ret) {
        return -30;
    }

    uv_run(&loop, UV_RUN_DEFAULT);
    return 0;
}
```

### Cpp magio code

```cpp
#include "magio/Runtime.h"
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/ThisCoro.h"
#include "magio/tcp/Tcp.h"

using namespace std;
using namespace magio;

Coro<void> process(TcpStream stream) {
    for (; ;) {
        auto [buf, rdlen] = co_await stream.vread();
        co_await stream.write(buf, rdlen);
    }
}

Coro<void> amain() {
    auto exe = co_await this_coro::executor;
    auto server = co_await TcpServer::bind("127.0.0.1", 8000);
    for (; ;) {
        auto stream = co_await server.accept();
        co_spawn(exe, process(std::move(stream)), detached);
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}
```

### Rust tokio code

```rust
use tokio::net::{TcpListener, TcpStream};
use std::error::Error;

#[tokio::main(worker_threads = 1)]
async fn main() -> Result<(), Box<dyn Error>> {
    let listener = TcpListener::bind("127.0.0.1:8000").await?;

    loop {
        let (stream, _) = listener.accept().await?;
        tokio::spawn(async move {
            process(stream).await
        });
    }
}

async fn process(mut stream: TcpStream) -> Result<(), std::io::Error> {
    let (mut reader, mut writer) = stream.split();
    loop {
        tokio::io::copy(&mut reader, &mut writer).await?;
    }
}
```

### NodeJs code

```javascript
const net = require('net')

const server = net.createServer();
const PORT = 8000
const HOST = "127.0.0.1"
server.listen(PORT, HOST)

server.on("listening", () => {
  console.log(`server start on ${HOST}:${PORT}`)
})

server.on("connection", (socket) => {
  socket.on("data", (data) => {
    socket.write(data)
  })
})

server.on("close", () => {
  console.log("server end")
})

server.on("error", (err) => {
  console.log(err)
})
```
