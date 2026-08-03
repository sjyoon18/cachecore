#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdbool.h>

struct hashmap;

struct hashmap *hashmap_create(size_t bucket_count);
bool hashmap_put(struct hashmap *map, const char *key, const char *value);
const char *hashmap_get(struct hashmap *map, const char *key);
bool hashmap_remove(struct hashmap *map, const char *key);
void hashmap_destroy(struct hashmap *map);

#endif
