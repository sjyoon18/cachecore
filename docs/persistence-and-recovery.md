# Persistence and Recovery

CacheCore began as a purely in-memory database. Adding persistence forced me to define exactly when a mutation becomes durable, what happens after a partial write, and which kinds of corruption the server can safely repair.

## Append-Only Format

Mutations are stored in `cachecore.aof` using the same command format accepted by the server:

```text
SET language C
SET city Seoul
DEL city
```

Only `SET` and `DEL` appear in the AOF. Reads do not change state and therefore do not need to be persisted.

Reusing the command format keeps the log readable and lets recovery use the existing parser. The trade-off is that the AOF inherits the protocol’s restrictions: commands are case-sensitive, newline-terminated, and cannot contain whitespace inside keys or values.

## Mutation Ordering

Every database operation executes under the database mutex. A successful `SET` follows this order:

1. Append the `SET` record to the AOF.
2. Call `fsync()` on the AOF.
3. Update the in-memory hashmap.
4. Check whether compaction is necessary.
5. Release the mutex and return success.

A successful `DEL` first verifies that the key exists, then follows the same persistence-before-mutation ordering.

Persisting first prevents the server from acknowledging an in-memory change that has not reached the AOF. If the process stops after the record becomes durable but before the hashmap changes, replay can restore the intended state at the next startup.

A persistence, hashmap, or compaction failure is treated as fatal. The server does not continue operating after it can no longer maintain its persistence guarantees.

The mutex also preserves the order of concurrent mutations. Without it, records from different workers could be persisted in an order that did not match the in-memory state.

## Startup Replay

Database creation opens the AOF and reconstructs the hashmap before the server begins accepting commands.

Replay performs these steps:

1. Determine the current file size.
2. Read the complete file into memory.
3. Check whether the final byte is a newline.
4. If not, truncate backward to the last complete record.
5. Parse each remaining line.
6. Apply `SET` and `DEL` operations to the hashmap in order.
7. Count the retained log records for the compaction policy.

Replaying in order naturally preserves overwrites and deletions. The most recent operation for a key determines its recovered state.

## Incomplete-Tail Recovery

A process can stop after writing only part of the final record. For example:

```text
SET name Alice
SET city Seoul
SET broken
```

Because the last line has no terminating newline, CacheCore treats it as incomplete and truncates it. The two preceding records remain valid and are replayed.

This recovery policy is intentionally narrow. CacheCore repairs only an incomplete trailing record. A malformed record that already ends in a newline causes replay—and therefore startup—to fail.

The AOF has no checksum or record length, so the server cannot safely infer how arbitrary interior corruption should be repaired. Failing closed is safer than silently inventing state.

## Automatic Compaction

An append-only log grows even when repeatedly overwriting one key. CacheCore tracks:

```text
obsolete entries = total AOF entries - live hashmap entries
```

When the number of obsolete entries reaches 100, the AOF is compacted while the database mutex is held.

Compaction works as follows:

1. Create and truncate `cachecore.aof.tmp`.
2. Visit every live hashmap entry.
3. Write one `SET` record for each entry.
4. `fsync()` and close the temporary file.
5. Atomically rename it to `cachecore.aof`.
6. Reopen the new AOF in append mode.
7. Reset the log-entry count to the number of live keys.

Holding the database mutex prevents mutations from changing the hashmap while its snapshot is being written.

The temporary-file approach also avoids modifying the active log in place. If snapshot generation fails before the rename, the previous AOF remains available.

## Guarantees and Limits

Within its intended scope, CacheCore provides these guarantees:

- Successful mutations are appended and `fsync`ed before memory changes.
- Mutations from different workers are ordered by the database mutex.
- Complete AOF records are replayed in order.
- An incomplete final record is discarded.
- Compaction retains the latest live state.

The persistence model does not provide:

- transactions across multiple commands;
- checksums or repair of arbitrary corruption;
- replication or backups;
- a configurable `fsync` policy;
- directory `fsync` after renaming the compacted file.

The last point means the implementation demonstrates safe replacement through temporary-file `fsync` and atomic rename, but does not claim the strongest possible durability against power loss at every filesystem boundary.

The main lesson for me was that persistence is not merely writing commands to a file. It is a contract involving operation order, failure behavior, recovery boundaries, and an honest statement of what the format cannot safely recover.
