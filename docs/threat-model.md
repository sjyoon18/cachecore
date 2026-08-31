# Threat Model

## Purpose

This threat model defines the security boundaries and assumptions used to review CacheCore. The goal is to identify security-relevant failure modes, test them with reproducible evidence, and selectively harden the educational server.

This review does not claim that CacheCore is suitable for production deployment.

---

## Scope

### In Scope

- TCP connection handling and stream framing.
- Command parsing, validation, and dispatch.
- Memory and resource behavior reachable through client input.
- Worker-pool and connection-queue availability.
- Database synchronization and key-value state integrity.
- AOF appending, replay, incomplete-tail recovery, and compaction.
- Startup and shutdown behavior when clients or persistence data behave unexpectedly.
- Risks created by the absence of authentication, authorization, and transport encryption.

### Out of Scope

- Implementing production authentication, authorization, or TLS.
- Protecting a host whose operating system or CacheCore account is already compromised.
- Defending against an attacker with unrestricted write access to the working directory.
- Vulnerabilities in the operating-system kernel, compiler, or standard libraries.
- Physical attacks and hardware compromise.
- Large distributed denial-of-service attacks.
- Replication, backups, and disaster recovery beyond the existing AOF design.

---

## Assets

- **Key-value confidentiality:** Stored keys and values, including their representation in the AOF, should not be disclosed to unauthorized parties.

- **Database integrity:** The logical key-value state should change only through intended operations. Malformed input, concurrency, or corrupted persistence data should not cause unintended state changes.

- **Service availability:** CacheCore should remain responsive to legitimate clients and should not crash, hang, or exhaust its resources because of one malicious or malformed client.

- **Durability and recoverability:** Successfully acknowledged mutations should be recoverable after a supported shutdown or restart. The reconstructed logical state should match the state represented by complete, valid AOF records.

- **Server-process integrity:** Untrusted network input or malformed AOF data should not cause memory corruption, invalid memory access, or unintended control flow.

---

## Attacker Model

### Remote Network Client

The primary attacker is an unauthenticated remote client that can reach CacheCore's TCP port. CacheCore cannot distinguish this attacker from an ordinary client because the protocol provides no authentication.

The attacker can:

- Open one or more TCP connections.
- Keep connections open without sending complete commands.
- Send arbitrary byte sequences.
- Split a command across multiple writes.
- Send multiple commands in one write.
- Send invalid, oversized, or unterminated commands.
- Choose command timing and ordering.
- Disconnect at any point during request processing.
- Read every response returned over their connections.

The attacker cannot initially:

- Execute code on the CacheCore host.
- Modify CacheCore's binary or source code.
- Directly access the server process's memory.
- Modify files protected by the host operating system.
- Compromise the operating-system kernel, compiler, or standard libraries.

### Persistence and Local Environment

The remote network attacker cannot directly modify the AOF but can influence its contents through accepted `SET` and `DEL` commands.

The security review also treats the AOF as potentially malformed recovery input. Incomplete writes, manual damage, or storage corruption may produce data that does not follow the expected record format. CacheCore should handle such data without memory corruption or unintended code execution, although refusing to start may be an acceptable response to a malformed complete record.

The operating system and filesystem permission model are trusted. An attacker who controls the account running CacheCore, can replace the server binary, or has unrestricted write access to the working directory is outside this review's threat model.

The AOF is not encrypted. Confidentiality of data at rest therefore depends on the host filesystem and its permissions.

---

## Attack Surface Map

| Surface | Attacker influence | Assets at risk | Existing boundary or control |
| --- | --- | --- | --- |
| Network listener and connection lifecycle | A remote client can open connections, keep them open, disconnect at arbitrary times, and repeat this behavior. | Availability and process integrity | Four workers, a 16-entry queue, and rejection when the queue is full |
| TCP stream framing | A client controls all bytes, fragmentation, timing, command termination, and whether multiple commands arrive together. | Process integrity and availability | Fixed 1,024-byte buffers and connection closure when accumulated input is too large |
| Command parser | A client supplies command names, arguments, whitespace, embedded bytes, and invalid command forms. | Process integrity and database integrity | Exact command names and argument counts; invalid commands produce `ERROR` |
| Command execution and database API | Any connected client can issue valid `GET`, `SET`, and `DEL` commands because there is no access control. | Confidentiality, integrity, availability, and durability | A single database mutex preserves operation ordering |
| Response writing and client disconnection | A client controls whether its socket remains open while the server writes a response. | Process integrity and availability | A partial-write loop handles short writes and interrupted system calls |
| AOF append path | A client indirectly controls persisted keys, values, mutation frequency, and log growth through `SET` and `DEL`. | Integrity, durability, disk availability, and service availability | Records are appended and `fsync`ed before memory mutation |
| AOF replay | Startup processes the complete AOF, including malformed, incomplete, or unexpectedly large contents. | Process integrity, database integrity, recoverability, and availability | Incomplete-tail truncation and rejection of malformed complete records |
| AOF compaction | Repeated mutations can trigger a full snapshot while the database mutex is held. | Availability, integrity, and durability | Temporary-file write, `fsync`, atomic rename, and serialized database access |
