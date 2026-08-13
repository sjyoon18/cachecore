#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdbool.h>

struct hashmap;

size_t hashmap_size(const struct hashmap *map);

typedef void (*hashmap_visit_fn)(
    const char *key,
    const char *value,
    void *context
);

struct hashmap *hashmap_create(size_t bucket_count);
bool hashmap_put(
    struct hashmap *map,
    const char *key,
    const char *value
);

const char *hashmap_get(
    struct hashmap *map,
    const char *key
);

bool hashmap_remove(
    struct hashmap *map,
    const char *key
);

void hashmap_foreach(
    struct hashmap *map,
    hashmap_visit_fn visit,
    void *context
);

void hashmap_destroy(struct hashmap *map);

#endif
