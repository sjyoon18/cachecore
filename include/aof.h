#ifndef AOF_H
#define AOF_H

#include <stdbool.h>

struct hashmap;
struct aof;

struct aof *aof_open(void);

bool aof_append_set(
    struct aof *aof,
    const char *key,
    const char *value
);

bool aof_append_del(
    struct aof *aof,
    const char *key
);

bool aof_replay(
    struct aof *aof,
    struct hashmap *map
);

bool aof_maybe_compact(
    struct aof *aof,
    struct hashmap *map
);

bool aof_compact(
    struct aof *aof,
    struct hashmap *map
);

void aof_close(struct aof *aof);

#endif
