# Architecture

CacheCore is divided into small components with clear responsibilities. This separation let me add networking, concurrency, persistence, and shutdown behavior incrementally without turning the server into one tightly coupled file.

## Request Lifecycle

A request moves through the server as follows:

1. The listener accepts a TCP connection on port `6379`.
2. The client manager records the socket so it can be interrupted during shutdown.
3. The connection is submitted to a bounded queue with a capacity of 16.
4. One of four worker threads removes it from the queue.
5. That worker owns the persistent connection until the client disconnects or sends `QUIT`.
6. The connection handler extracts newline-terminated commands from the TCP byte stream.
7. The parser converts each complete command into a structured representation.
8. `PING` and `QUIT` are handled directly; `SET`, `GET`, and `DEL` go through the shared database.
9. A newline-terminated response is written back to the client.

If the connection queue is full, the newly accepted client is removed from the client manager and its socket is closed.

## Component Responsibilities

| Component | Responsibility |
| --- | --- |
| `server.c` | Listening socket, client handling, stream framing, dispatch, and shutdown coordination |
| `command.c` | Command validation and parsing |
| `thread_pool.c` | Fixed workers, bounded job submission, and worker shutdown |
| `queue.c` | FIFO storage for pending connection jobs |
| `client_manager.c` | Tracking sockets that must be interrupted during shutdown |
| `database.c` | Synchronization and ordering of database operations |
| `hashmap.c` | In-memory key-value storage |
| `aof.c` | Append-only persistence, replay, recovery, and compaction |

## TCP Framing

One of the most important networking lessons in this project was that TCP provides a byte stream, not individual messages. One `read()` can therefore contain:

- part of one command;
- exactly one command; or
- several commands together.

The connection handler keeps an accumulation buffer and processes only complete newline-terminated commands. After processing one command, it moves any remaining bytes to the beginning of the buffer for the next iteration.

This supports both fragmented commands and multiple commands arriving in one read. Input that exceeds the fixed 1,024-byte buffer is rejected by closing the connection.

The protocol is deliberately small and case-sensitive. Keys and values cannot contain whitespace.

## Concurrency Model

CacheCore uses four fixed worker threads instead of creating one thread per connection. This bounds thread creation and makes resource use predictable.

The job queue has its own mutex and condition variable. Workers sleep while the queue is empty and wake when a connection is submitted. During destruction, the pool enters shutdown mode, drains queued work, and joins every worker.

A worker owns an entire persistent connection, not a single command. This keeps connection state local to one thread and simplifies framing, but it also means an idle persistent client occupies a worker. Four such clients can occupy the full worker pool, leaving later connections queued until one disconnects.

Three separate locks protect different kinds of shared state:

- The thread-pool mutex protects the job queue and pool shutdown state.
- The client-manager mutex protects the active socket list.
- The database mutex protects the hashmap and AOF operation ordering.

The single database mutex serializes `SET`, `GET`, and `DEL`. I chose this design because it gave the project a simple correctness model before attempting finer-grained locking.

For `GET`, the database copies the stored value while holding the mutex. The caller owns that copy, so it remains valid after the lock is released.

## In-Memory Storage

The hashmap uses separate chaining to handle collisions. It begins with 16 buckets and doubles its bucket count when inserting a new entry after the number of entries reaches the current bucket count.

The hashmap owns copies of its keys and values. Updating an existing key replaces its value without increasing the entry count.

The hashmap itself does not perform synchronization. Concurrency is handled by the database layer, which keeps storage policy separate from shared-state ownership.

## Coordinated Shutdown

The `SIGINT` handler performs only signal-safe work: it sets a `sig_atomic_t` flag. The interrupted `accept()` call then allows the main server loop to stop.

Shutdown proceeds in this order:

1. Call `shutdown()` on every tracked client socket.
2. Wake blocked connection handlers by interrupting their socket operations.
3. Put the thread pool into shutdown mode.
4. Drain queued connection jobs and join every worker.
5. Destroy the client manager.
6. Close the AOF and destroy the database.
7. Close the listening socket.

This ordering ensures workers finish using shared structures before those structures are destroyed.

## Design Trade-offs

The architecture favors explicit ownership and understandable guarantees over maximum throughput.

Its main limitations are:

- persistent clients can occupy all workers;
- all database operations share one mutex;
- buffers and command sizes are fixed;
- the server binds to all interfaces;
- there is no authentication, authorization, or encryption.

These are acceptable within the project’s educational scope. The architecture taught me that concurrency is not simply adding threads: it requires bounded work, clear ownership, synchronization boundaries, and a shutdown path designed alongside normal execution.
