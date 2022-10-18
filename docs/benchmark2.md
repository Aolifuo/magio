# Benchmark

- [Benchmark](#benchmark)
  - [Result](#result)
    - [Cpp Magio](#cpp-magio)
    - [Cpp Asio](#cpp-asio)
    - [Golang](#golang)

## Result

> wrk -t 8 -c 100000 -d 10 <http://172.30.48.1:8000/>

### Cpp Magio

```shell
Running 10s test @ http://172.30.48.1:8000/
  8 threads and 100000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    28.88ms   29.48ms   1.19s    89.69%
    Req/Sec    24.37k    13.02k  119.42k    75.63%
  1663209 requests in 10.10s, 580.53MB read
  Socket errors: connect 90011, read 2, write 703, timeout 0
Requests/sec: 164650.34
Transfer/sec:     57.47MB
```

### Cpp Asio

```shell
Running 10s test @ http://172.30.48.1:8000/
  8 threads and 100000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    33.27ms   44.99ms   1.59s    94.57%
    Req/Sec    21.77k     8.79k   45.04k    65.92%
  1669901 requests in 10.09s, 582.87MB read
  Socket errors: connect 90011, read 613, write 1173, timeout 0
Requests/sec: 165560.01
Transfer/sec:     57.79MB
```

### Golang

```shell
Running 10s test @ http://172.30.48.1:8000/
  8 threads and 100000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    27.66ms   39.68ms   2.00s    97.24%
    Req/Sec    20.80k     9.51k   49.54k    65.92%
  1633321 requests in 10.09s, 573.22MB read
  Socket errors: connect 90011, read 1025, write 808, timeout 566
Requests/sec: 161929.66
Transfer/sec:     56.83MB
```
