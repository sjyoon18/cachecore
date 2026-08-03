#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

struct entry {
    char *key;
    char *value;
    struct entry *next;
};

struct hashmap {
    size_t bucket_count;
    size_t entry_count;
    struct entry **buckets;
};

static size_t hash_string(const char *key) {
    size_t hash = 0;

    while(*key != '\0') {
        hash = hash * 31 + (unsigned char)*key;
        key++;
    }

    return hash;
}

static struct entry *entry_create(const char *key, const char *value) {
    struct entry *entry = malloc(sizeof(*entry));

    if (entry == NULL) {
        return NULL;
    }
    
    entry->key = malloc(strlen(key) + 1);

    if (entry->key == NULL) {
        free(entry);
        return NULL;
    }

    entry->value = malloc(strlen(value) + 1);

    if (entry->value == NULL) {
        free(entry->key);
        free(entry);
        return NULL;
    }

    strcpy(entry->key, key);
    strcpy(entry->value, value);
    entry->next = NULL;

    return entry;
}

static void entry_destroy(struct entry *entry) {
    if (entry == NULL) {
        return;
    }

    free(entry->key);
    free(entry->value);
    free(entry);
}

static bool hashmap_resize(struct hashmap *map, size_t new_bucket_count) {
    if (map == NULL || new_bucket_count == 0) {
        return false;
    }

    struct entry **new_buckets = calloc(new_bucket_count, sizeof(*new_buckets));

    if (new_buckets == NULL) {
        return false;
    }

    for (size_t i = 0; i < map->bucket_count; i++) {

        struct entry *current = map->buckets[i];

        while (current != NULL) {
            struct entry *next = current->next;

            size_t new_bucket_index = hash_string(current->key) % new_bucket_count;
            
            current->next = new_buckets[new_bucket_index];
            new_buckets[new_bucket_index] = current;

            current = next;
        }
    }

    struct entry **old_buckets = map->buckets;

    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;

    free(old_buckets);

    return true;
}

struct hashmap *hashmap_create(size_t bucket_count) {
    if (bucket_count == 0) {
        return NULL;
    }

    struct hashmap *map = malloc(sizeof(*map));
    
    if (map == NULL) {
        return NULL;
    }

    map->buckets = calloc(bucket_count, sizeof(*map->buckets));

    if (map->buckets == NULL) {
        free(map);
        return NULL;
    }

    map->bucket_count = bucket_count;
    map->entry_count = 0;

    return map;
}

bool hashmap_put(struct hashmap *map, const char *key, const char *value) {
    if (map == NULL || key == NULL || value == NULL) {
        return false;
    }

    size_t bucket_index = hash_string(key) % map->bucket_count;

    struct entry *current = map->buckets[bucket_index];

    while(current != NULL) {
        if (strcmp(key, current->key) == 0) {
            char *new_value = malloc(strlen(value) + 1);

            if (new_value == NULL) {
                return false;
            }

            strcpy(new_value, value);

            free(current->value);
            current->value = new_value;
            return true;
        }

        current = current->next;
    }

    if (map->entry_count >= map->bucket_count) {
        if (!hashmap_resize(map, 2 * map->bucket_count)) {
            return false;
        }

        bucket_index = hash_string(key) % map->bucket_count;
    }

    struct entry *entry = entry_create(key, value);

    if (entry == NULL) {
        return false;
    }
    
    entry->next = map->buckets[bucket_index];
    map->buckets[bucket_index] = entry;

    map->entry_count++;

    return true;
}

const char *hashmap_get(struct hashmap *map, const char *key) {
    if (map == NULL || key == NULL) {
        return NULL;
    }

    size_t bucket_index = hash_string(key) % map->bucket_count;
    struct entry *current = map->buckets[bucket_index];

    while (current != NULL) {
        if (strcmp(key, current->key) == 0) {
            return current->value;
        }
        current = current->next;
    }

    return NULL;
}

bool hashmap_remove(struct hashmap *map, const char *key) {
    if (map == NULL || key == NULL) {
        return false;
    }

    size_t bucket_index = hash_string(key) % map->bucket_count;

    struct entry *previous = NULL;
    struct entry *current = map->buckets[bucket_index];

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            if (previous == NULL) {
                map->buckets[bucket_index] = current->next;
            } else {
                previous->next = current->next;
            }
            entry_destroy(current);
            map->entry_count--;
            return true;
        }

        previous = current;
        current = current->next;
    }

    return false;
}

void hashmap_destroy(struct hashmap *map) {
    if (map == NULL) {
        return;
    }
    for (size_t i = 0; i < map->bucket_count; i++) {
        struct entry *entry = map->buckets[i];
        while(entry != NULL) {
            struct entry *next_entry = entry->next;
            entry_destroy(entry);
            entry = next_entry;
        }
    }
    free(map->buckets);
    free(map);
}
