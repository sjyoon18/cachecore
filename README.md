# CacheCore

## Overview

CacheCore is a Redis-inspired key-value server that I built in C to understand how networking, concurrency, persistence, recovery, and performance interact inside a stateful server.

The project began as an in-memory hashmap and command parser. I developed it incrementally into a persistent multi-client TCP service with stream-aware command framing, a fixed worker pool, a bounded connection queue, synchronized shared state, append-only persistence, incomplete-tail recovery, log compaction, coordinated shutdown, layered tests, and a synchronized throughput benchmark.

Each major feature was introduced in response to a limitation exposed by the previous design. The goal was not to reproduce Redis or create a production-ready database, but to learn the mechanisms and trade-offs behind reliable systems software by implementing and testing them directly.

## Verified Features

- A custom, case-sensitive, newline-delimited TCP protocol with `PING`, `SET`, `GET`, `DEL`, and `QUIT`.
- Persistent connections with buffering for fragmented reads and multiple commands received together.
- A dynamically resized, separate-chaining hashmap for in-memory storage.
- Four fixed worker threads and a bounded 16-entry connection queue.
- Mutex-protected shared database state across concurrent clients.
- Append-only logging for `SET` and `DEL`, with `fsync` before the in-memory mutation.
- Startup replay, truncation of an incomplete trailing record, and automatic AOF compaction.
- Coordinated `SIGINT` shutdown that closes active client sockets and joins workers.
- Unit, integration, concurrent stress, and synchronized benchmark coverage.

## Design Evolution and Lessons

- I first separated command parsing, storage, and server responsibilities so each layer could evolve independently.
- TCP networking exposed the difference between messages and byte streams, leading to explicit newline framing, buffering, and persistent connections.
- Moving from one client to concurrent clients introduced shared-state ownership, synchronization, bounded work queues, and shutdown coordination.
- Adding persistence showed me that durability depends on write ordering and clearly defined recovery limits, not merely writing data to a file.
- Layered tests and a synchronized benchmark turned observed behavior into evidence I could reproduce and reason about.

## Architecture at a Glance

```text
+------------+
| TCP client |
+-----+------+
      |
      | newline-delimited commands (port 6379)
      v
+--------------+
| TCP listener |
+------+-------+
       |
       v
+--------------------------------+
| Bounded connection queue       |
| Capacity: 16                   |
+---------------+----------------+
                |
                v
+--------------------------------+
| Worker pool                    |
| 4 threads                      |
+---------------+----------------+
                |
                | one worker owns each connection
                v
+--------------------------------+
| Persistent connection handler  |
| Buffering, framing, dispatch   |
+-----------+--------------------+
            |
            +---- PING / QUIT ------> Direct response
            |
            +---- SET / GET / DEL
                         |
                         v
              +----------------------+
              | Database API         |
              | Single mutex         |
              +----------+-----------+
                         |
             +-----------+-----------+
             |                       |
          GET path              SET / DEL path
             |                       |
             |                       v
             |              +------------------+
             |              | Append to AOF    |
             |              | and fsync        |
             |              +--------+---------+
             |                       |
             |                       | then mutate
             v                       v
              +----------------------+
              | In-memory hashmap    |
              +----------------------+

Startup:     AOF -------- replay --------> hashmap
Compaction:  hashmap ---- snapshot ------> AOF
```

- The listener accepts connections and rejects a new client if the queue is full.
- A worker owns a persistent connection until that client disconnects or sends `QUIT`.
- One database mutex serializes `SET`, `GET`, and `DEL` operations.
- Successful mutations are persisted before the in-memory hashmap is changed.

## Build and Run

### Requirements

- A C11 compiler
- POSIX sockets and pthreads
- `make`
- `nc`/netcat for manual protocol testing (optional)

CacheCore was developed and tested on macOS.

### Build

```sh
make
```

### Start the Server

```sh
make run
```

The server listens on port `6379`. From another terminal:

```sh
nc 127.0.0.1 6379
```

The AOF file is created in the server's working directory.

## Protocol Commands

| Command | Normal response | Description |
| --- | --- | --- |
| `PING` | `PONG` | Check that the server is responsive. |
| `SET <key> <value>` | `OK` | Store or replace a value. |
| `GET <key>` | `<value>` or `NOT_FOUND` | Retrieve a value. |
| `DEL <key>` | `OK` or `NOT_FOUND` | Delete a key. |
| `QUIT` | `BYE` | Close the connection. |

Commands are case-sensitive and newline-terminated. Keys and values cannot contain whitespace.

Example session:

```text
PING
PONG
SET language c
OK
GET language
c
DEL language
OK
QUIT
BYE
```

## Testing

Run the automated unit and database/AOF integration tests:

```sh
make test
```

This covers the hashmap, command parser, database operations, AOF replay, incomplete-tail recovery, and compaction.

To run the concurrent TCP stress test, start the server and use a second terminal:

```sh
make stress-client
```

The stress client opens eight persistent connections. Each performs 1,000 validated `SET`/`GET` cycles. The final regression pass also verified the protocol over one persistent connection, persistence across restart, coordinated shutdown with an active client, and port reuse after shutdown.

## Benchmark

With the server running, execute:

```sh
make benchmark
```

The benchmark measures steady-state synchronous `GET` throughput over the local loopback interface. Connection setup, the initial `SET`, worker readiness, `QUIT`, and cleanup are outside the timed interval. Each client keeps one request outstanding at a time; the benchmark does not use pipelining.

Three trials were run for each client count, with 10,000 requests per client:

| Clients | Total GET requests | Median throughput |
| ---: | ---: | ---: |
| 1 | 10,000 | 53,439 requests/s |
| 2 | 20,000 | 94,493 requests/s |
| 4 | 40,000 | 146,014 requests/s |

These are local macOS measurements and are intended to show scaling within this design, not production or distributed performance. See [Testing and Benchmarking](docs/testing-and-benchmarking.md) for the methodology and interpretation.

## Limitations and Security Scope

- CacheCore uses a custom protocol and is not Redis-compatible.
- The server binds to all interfaces and provides no authentication, authorization, or encryption.
- Persistent clients occupy workers for the lifetime of their connections, and a full queue causes new connections to be rejected.
- One database mutex serializes all data operations, favoring simple correctness over maximum parallelism.
- AOF recovery repairs only an incomplete trailing record; a malformed complete record causes startup failure.
- Compaction uses `fsync` and atomic rename for the AOF file but does not `fsync` the containing directory.
- Coordinated graceful shutdown is implemented for `SIGINT`.

CacheCore is an educational systems project and should not be exposed as a production service.

## Documentation

- [Architecture](docs/architecture.md)
- [Persistence and Recovery](docs/persistence-and-recovery.md)
- [Testing and Benchmarking](docs/testing-and-benchmarking.md)

## Project Status

The implementation is feature-complete for its intended learning scope. Repository hygiene and the final regression pass are complete, with no functional regressions found across automated tests, TCP behavior, concurrent stress, restart persistence, coordinated shutdown, and a benchmark smoke run.

The most valuable result for me was learning to treat system behavior as a chain of explicit guarantees: frame the byte stream, bound concurrent work, define shared-state ownership, persist mutations in the correct order, recover only what the format can safely identify, and measure performance with controlled timing boundaries.
