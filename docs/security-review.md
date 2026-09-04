# Security Review

This document records security hypotheses, experiments, confirmed findings, fixes, and regression evidence produced during the CacheCore security review.

A suspected problem is not classified as a confirmed finding until its behavior is reproduced or otherwise established with sufficient evidence.

---

## SR-01: Client Disconnect During Response Write

**Status:** Resolved

**Attack surface:** Response writing and client disconnection

**Assets at risk:** Service availability and server-process integrity

### Hypothesis

If a client resets its TCP connection after sending a command but before CacheCore writes the response, the server's `write()` call may generate `SIGPIPE`. Because CacheCore does not currently configure `SIGPIPE` handling, the signal may terminate the entire server process.

### Expected Secure Behavior

Failure to write to one disconnected client should end only that client connection. The server process should remain alive and continue accepting and serving other clients.

### Test Plan

1. Start CacheCore and verify that it responds to `PING`.
2. Connect an adversarial client.
3. Send commands that cause the server to produce responses.
4. Reset the client socket without reading those responses.
5. Check whether CacheCore remains running.
6. Open a fresh connection and send `PING`.
7. Record the server's exit status and output if it terminated.

### Evidence

The issue was reproduced on macOS using an adversarial client that sent
100 `PING` commands and then performed an abortive TCP close without
reading the responses.

CacheCore reported:

```text
Client connected
write: Broken pipe
```

The server process terminated, a fresh client could not connect, and the
shell reported exit status 141. This corresponds to termination by
signal 13 (SIGPIPE).

The experiment did not trigger the input-size limit, confirming that it
exercised the response-writing path.

### Impact

An unauthenticated remote client can terminate the entire CacheCore
server by disconnecting while the server is writing a response. This
violates service availability and server-process integrity.

### Root Cause

Before the fix, CacheCore wrote responses with `write()` without
suppressing or handling `SIGPIPE`. When a peer had already closed its
connection, a response write could generate `SIGPIPE`, whose default
disposition terminated the process.

### Fix

CacheCore now configures the process-wide disposition of `SIGPIPE` as
`SIG_IGN` using `sigaction()` before creating sockets or worker threads.

When a response is written to a disconnected client, `write()` now
returns an error instead of terminating the server process. The client
handler reports the error, destroys the current parsed command, and
returns. The worker then removes and closes only the failed client
connection.

### Regression Evidence

After applying the fix, the adversarial client was run three times. In
each trial, it sent 100 `PING` commands and reset its connection without
reading the responses.

CacheCore reported `write: Broken pipe` for each failed connection but
remained running. A fresh client sent `PING` after every trial and
received `PONG`.

The server was then stopped normally with `SIGINT`. The result confirms
that a response-write failure is contained to the responsible client
connection and no longer compromises server availability.

---

## SR-02: Idle Connection Worker Exhaustion

**Status:** Mitigated

**Attack surface:** Network listener, connection lifecycle, and worker pool

**Assets at risk:** Service availability

### Hypothesis

CacheCore assigns each accepted persistent connection to one worker, and
the worker performs a blocking `read()` without an inactivity deadline.

An unauthenticated client may therefore open a connection, send no
command, and occupy a worker indefinitely. Four idle clients may occupy
all four workers, preventing legitimate connections from being processed.

The connection queue does not prevent this condition. It can retain
additional client jobs, but queued jobs cannot execute until an occupied
worker becomes available.

### Expected Secure Behavior

A bounded number of idle clients should not permanently consume all
request-processing capacity. CacheCore should eventually reclaim
connections that make no progress and continue serving active clients.

### Test Plan

1. Start CacheCore and confirm that a normal `PING` receives `PONG`.
2. Open four client connections, matching the number of server workers.
3. Keep all four connections open without sending commands.
4. Open a fifth connection and send `PING`.
5. Check whether the fifth client receives `PONG` within a short bounded
   interval.
6. Close the idle connections.
7. Verify that CacheCore resumes processing normal requests.
8. Record the server output and observed timing.

### Evidence

The issue was reproduced by opening four TCP connections, matching
CacheCore's four-worker pool, and keeping all four connections open
without sending commands.

Before the idle connections were opened, a normal `PING` received
`PONG`. While all four idle connections remained open, a fifth client
connected and sent `PING` but received no response within the two-second
test interval.

After the four idle connections were closed, CacheCore resumed processing
requests and a fresh `PING` received `PONG`.

The server log showed that the fifth connection was accepted even though
its command was not processed. This demonstrates that TCP acceptance and
application-level service availability are separate events.

### Impact

Four unauthenticated remote clients can consume all request-processing
workers without sending any commands. Legitimate requests then remain
queued without making progress, allowing a small number of clients to
deny service for an unbounded period.

The bounded connection queue limits queued memory growth, but it does not
preserve request-processing capacity or guarantee progress.

### Root Cause

CacheCore assigns one worker to each persistent client for the full
lifetime of that connection. Each worker performs a blocking `read()`
without an inactivity deadline.

Because the worker pool contains four threads, four silent connections
can block every worker indefinitely. Increasing the queue capacity would
allow more connections to wait but would not make a worker available.

### Mitigation

CacheCore now configures a ten-second receive timeout on every accepted
client socket before adding it to the client manager or worker queue.

If a blocking `read()` receives no data during that interval, it returns
with `EAGAIN` or `EWOULDBLOCK`. The client handler identifies this as an
inactivity timeout and returns, after which the worker removes and closes
the client connection.

A read interrupted by a signal with `EINTR` is retried rather than being
misclassified as a client failure.

### Regression Evidence

Four silent connections were again opened to occupy all four workers. A
fifth client connected and sent `PING`.

The fifth client initially waited, but received `PONG` after the idle
connections reached the server's configured timeout. The client-observed
wait was 8.11 seconds because the timed request began after the idle
workers had already spent part of their ten-second timeout interval.

After the timeout, a fresh `PING` received `PONG` immediately.

Normal persistent-client behavior was also tested. A connected client
sent `PING`, remained inactive for approximately three seconds, sent
another `PING`, and received `PONG` for both requests. It then sent
`QUIT` and received `BYE`. This confirms that successful reads begin new
inactivity intervals rather than imposing a maximum connection lifetime.

### Residual Risk

The mitigation bounds completely silent connections, but it does not
remove the architectural coupling between one persistent connection and
one worker.

A client that sends data before each timeout expires receives another
inactivity interval and may continue occupying its worker. Additionally,
a socket waiting in the job queue has not yet begun a blocking `read()`,
so its receive timeout does not expire while it is merely queued.

Stronger protection would require broader architectural controls such as
event-driven connection multiplexing, request deadlines, admission
control, or fairness limits. Those changes are outside this targeted
mitigation and should not be implied by the current result.
