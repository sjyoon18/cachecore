#include "database.h"
#include "hashmap.h"
#include "aof.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>


#define INITIAL_BUCKET_COUNT 16

struct database {
    struct hashmap *map;
    pthread_mutex_t mutex;
    struct aof *aof;
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

    if (pthread_mutex_init(&db->mutex, NULL) != 0) {
        hashmap_destroy(db->map);
        free(db);
        return NULL;
    }

    db->aof = aof_open();

    if (db->aof == NULL) {
        pthread_mutex_destroy(&db->mutex);
        hashmap_destroy(db->map);
        free(db);
        return NULL;
    }

    if (!aof_replay(db->aof, db->map)) {
        aof_close(db->aof);
        pthread_mutex_destroy(&db->mutex);
        hashmap_destroy(db->map);
        free(db);
        return NULL;
    }

    return db;
}

void db_destroy(struct database *db) {
    if (db == NULL) {
        return;
    }

    aof_close(db->aof);
    pthread_mutex_destroy(&db->mutex);
    hashmap_destroy(db->map);
    free(db);
}

enum db_result db_set(
    struct database *db,
    const char *key,
    const char *value
) {
    if (db == NULL || key == NULL || value == NULL) {
        return DB_ERROR;
    }

    pthread_mutex_lock(&db->mutex);

    if (!aof_append_set(db->aof, key, value)) {
        pthread_mutex_unlock(&db->mutex);
        return DB_FATAL;
    }

    if (!hashmap_put(db->map, key, value)) {
        pthread_mutex_unlock(&db->mutex);
        return DB_FATAL;
    }

    pthread_mutex_unlock(&db->mutex);

    return DB_OK;
}

char *db_get(struct database *db, const char *key) {
    if (db == NULL || key == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&db->mutex);

    const char *value = hashmap_get(db->map, key);

    if (value == NULL) {
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    char *copy = malloc(strlen(value) + 1);

    if (copy != NULL) {
        strcpy(copy, value);
    }

    pthread_mutex_unlock(&db->mutex);

    return copy;
}

enum db_result db_del(
    struct database *db,
    const char *key
) {
    if (db == NULL || key == NULL) {
        return DB_ERROR;
    }

    pthread_mutex_lock(&db->mutex);

    const char *value = hashmap_get(db->map, key);

    if (value == NULL) {
        pthread_mutex_unlock(&db->mutex);
        return DB_NOT_FOUND;
    }

    if (!aof_append_del(db->aof, key)) {
        pthread_mutex_unlock(&db->mutex);
        return DB_FATAL;
    }

    if (!hashmap_remove(db->map, key)) {
        pthread_mutex_unlock(&db->mutex);
        return DB_FATAL;
    }

    pthread_mutex_unlock(&db->mutex);

    return DB_OK;
}
