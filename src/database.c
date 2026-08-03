#include "database.h"
#include "hashmap.h"

#include <stdlib.h>

#define INITIAL_BUCKET_COUNT 16

struct database {
    struct hashmap *map;
};

struct database *db_create(void) {
    struct database *db = malloc(sizeof(*db));

    if (db == NULL) {
        return NULL;
    }

    db->map = hashmap_create(INITIAL_BUCKET_COUNT);

    if (db->map == NULL) {
        free(db);
        return NULL;
    }

    return db;
}

void db_destroy(struct database *db) {
    if (db == NULL) {
        return;
    }

    hashmap_destroy(db->map);
    free(db);
}

bool db_set(struct database *db, const char *key, const char *value) {
    if (db == NULL) {
        return false;
    }

    return hashmap_put(db->map, key, value);
}

const char *db_get(struct database *db, const char *key) {
    if (db == NULL) {
        return NULL;
    }

    return hashmap_get(db->map, key);
}

bool db_del(struct database *db, const char *key) {
    if (db == NULL) {
        return false;
    }

    return hashmap_remove(db->map, key);
}
