# Testing and Benchmarking

I added tests in layers as CacheCore evolved. Each layer answers a different question: whether individual data structures behave correctly, whether persistence survives lifecycle changes, whether the complete server works under concurrency, and how much work the finished design can process.

## Automated Tests

Run the automated suite with:

```sh
make test
```

### Hashmap Unit Tests

The hashmap tests cover:

- insertion and retrieval;
- missing keys;
- overwriting an existing value;
- removing existing and missing keys;
- entry-count maintenance;
- resizing without losing entries;
- visiting every live entry during iteration;
- rejection of an invalid zero-bucket configuration.

These tests isolate storage behavior from networking, persistence, and concurrency.

### Command Parser Unit Tests

The parser tests validate all supported commands:

- `PING`
- `SET`
- `GET`
- `DEL`
- `QUIT`

They also cover unknown commands, missing arguments, and extra arguments. This verifies the grammar separately from command execution.

### Database Integration Tests

The database tests create a temporary working directory so they do not modify the project’s normal AOF.

They verify:

- basic `SET`, `GET`, and `DEL`;
- overwriting an existing value;
- state recovery after closing and reopening the database;
- persistence of deletions;
- recovery of the latest overwritten value;
- truncation of an incomplete final AOF record;
- automatic compaction;
- successful replay after compaction.

These are integration tests because they exercise the database, hashmap, parser, filesystem, AOF replay, and compaction together.

## Concurrent Stress Test

With the server running, execute:

```sh
make stress-client
```

The stress client creates eight persistent TCP connections. Each performs 1,000 validated `SET`/`GET` cycles using its own key and finishes with `QUIT`.

Every response is checked immediately. The test fails if a socket operation fails or the returned value differs from the expected value.

CacheCore has four server workers, so the eight clients are served in groups as workers become available. This completes because the stress clients do not wait at a shared start barrier: the first clients eventually send `QUIT`, allowing queued connections to run.

The stress test checks correctness under concurrent traffic. It is not a performance measurement.

## Final Regression Coverage

The final regression pass also checked behavior that is not fully represented by `make test`:

- multiple commands over one persistent connection;
- persistence across a server restart;
- concurrent stress traffic;
- `SIGINT` while a client is connected;
- client disconnection during shutdown;
- worker cleanup and process exit;
- reuse of port `6379` after shutdown;
- a benchmark smoke run.

Together, these checks cover both normal operation and important lifecycle boundaries.

## Benchmark Method

With the server running, execute:

```sh
make benchmark
```

The benchmark currently uses four client threads and 10,000 `GET` requests per client.

Each benchmark worker:

1. Creates and connects its socket.
2. Sends one setup `SET`.
3. Validates the `OK` response.
4. Waits at the readiness barrier.
5. Performs synchronous `GET` request/response cycles.
6. Validates every returned value.
7. Signals that its measured work is complete.
8. Waits for the main thread to record the ending time.
9. Sends `QUIT` and closes the socket.

The main thread uses `CLOCK_MONOTONIC`. It records the start immediately before releasing the workers and records the end after every worker completes its GET loop.

Connection setup, the setup `SET`, `QUIT`, socket closure, and thread joining are outside the timed interval. The interval still includes normal thread scheduling at the barrier boundaries.

Each client keeps only one request outstanding at a time. There is no pipelining.

## What the Result Measures

Throughput is calculated as:

```text
total successful GET requests / elapsed wall-clock seconds
```

This is aggregate throughput across all benchmark clients, not the rate of one client.

Each measured request includes:

- writing the `GET` command;
- loopback TCP transport;
- server-side framing and parsing;
- database mutex acquisition;
- hashmap lookup and value copying;
- response generation and writing;
- client-side reading and validation.

The result therefore measures end-to-end steady-state synchronous GET throughput on the local machine. It does not measure the hashmap lookup in isolation.

## Results

Three trials were run at each client count with 10,000 requests per client:

| Clients | Trial 1 | Trial 2 | Trial 3 | Median | Median speedup |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 53,439 | 53,714 | 53,071 | 53,439 req/s | 1.00× |
| 2 | 94,580 | 94,493 | 75,561 | 94,493 req/s | 1.77× |
| 4 | 153,081 | 145,559 | 146,014 | 146,014 req/s | 2.73× |

The increase shows that multiple workers improve aggregate throughput. Scaling is not expected to be perfectly linear because every `GET` still uses the same database mutex. Scheduling, socket handling, parsing, memory allocation, and lock contention also add overhead as concurrency increases.

The lower third result for two clients demonstrates normal run-to-run variation. Reporting the median reduces the influence of one unusually slow trial.

These are local macOS measurements and should not be compared directly with production databases or remote-network workloads.

## Client-Count Constraint

The synchronized benchmark should not use more persistent clients than the server has workers.

Before entering the start barrier, every benchmark client sends its setup `SET` and waits for a response. With more than four clients, the first four persistent connections can occupy all four server workers while the remaining connections wait in the queue.

The main benchmark thread waits for every client to finish setup, but the queued clients cannot finish setup until an occupied worker becomes free. The first clients do not send `GET` or `QUIT` until the start barrier is released, producing a deadlock.

Testing more than four simultaneous benchmark clients would require changing the server’s connection ownership model, increasing its worker count, or redesigning the benchmark so queued clients do not participate in the same readiness barrier.

## Interpretation Limits

This benchmark does not measure:

- individual request latency or percentile latency;
- `SET` or `DEL` performance;
- AOF and `fsync` cost;
- pipelined requests;
- remote network performance;
- very large keys or values;
- long-duration stability;
- performance beyond the four-worker design.

The benchmark’s value is narrower: it provides a reproducible measurement boundary and shows how steady-state GET throughput changes as the existing worker pool is used more fully.

The main lesson for me was that a benchmark needs a precise definition of what begins and ends inside the timer. A number becomes meaningful only when the workload, synchronization, timing boundaries, and limitations are documented alongside it.
