# Security Review

This document records security hypotheses, experiments, confirmed findings, fixes, and regression evidence produced during the CacheCore security review.

A suspected problem is not classified as a confirmed finding until its behavior is reproduced or otherwise established with sufficient evidence.

## SR-01: Client Disconnect During Response Write

**Status:** Confirmed

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

CacheCore writes responses with write(). When the peer has closed its
connection, the write can generate SIGPIPE. CacheCore does not suppress
or handle this signal, so its default disposition terminates the process.