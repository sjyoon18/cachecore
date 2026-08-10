#include "database.h"
#include "hashmap.h"
#include "command.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


#define INITIAL_BUCKET_COUNT 16
#define LOG_FILE "cachecore.aof"
#define LOG_BUFFER_SIZE 1024

struct database {
    struct hashmap *map;
    pthread_mutex_t mutex;
    int log_fd;
};

static bool write_all(
    int fd,
    const char *buffer,
    size_t length
) {
    size_t total_written = 0;

    while (total_written < length) {
        ssize_t written = write(
            fd,
            buffer + total_written,
            length - total_written
        );

        if (written == -1) {
            return false;
        }

        if (written == 0) {
            return false;
        }

        total_written += (size_t)written;
    }

    return true;
}

static bool append_log(
    struct database *db,
    const char *operation,
    const char *key,
    const char *value
) {
    char buffer[LOG_BUFFER_SIZE];

    int length;

    if (value != NULL) {
        length = snprintf(buffer, sizeof(buffer), "%s %s %s\n", operation, key, value);
    } else {
        length = snprintf(buffer, sizeof(buffer), "%s %s\n", operation, key);
    }

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return false;
    }

    if (!write_all(db->log_fd, buffer, (size_t)length)) {
        return false;
    }

    if (fsync(db->log_fd) == -1) {
        return false;
    }

    return true;
}

static bool replay_log(struct database *db) {
    off_t file_size = lseek(db->log_fd, 0, SEEK_END);

    if (file_size == -1) {
        return false;
    }

    if (file_size == 0) {
        return true;
    }

    if (lseek(db->log_fd, 0, SEEK_SET) == -1) {
        return false;
    }

    char *buffer = malloc((size_t)file_size + 1);

    if (buffer == NULL) {
        return false;
    }

    size_t total_read = 0;

    while (total_read < (size_t)file_size) {
        ssize_t bytes_read = read(
            db->log_fd,
            buffer + total_read,
            (size_t)file_size - total_read
        );

        if (bytes_read == -1) {
            free(buffer);
            return false;
        }

        if (bytes_read == 0) {
            break;
        }

        total_read += (size_t)bytes_read;
    }

    if (total_read != (size_t)file_size) {
        free(buffer);
        return false;
    }

    if (buffer[total_read - 1] != '\n') {
        while(total_read > 0 && buffer[total_read - 1] != '\n') {
            total_read--;
        }
        if (ftruncate(db->log_fd, (off_t)total_read) == -1) {
            free(buffer);
            return false;
        }
    }

    buffer[total_read] = '\0';

    char *line = strtok(buffer, "\n");

    while (line != NULL) {
        struct command *command = parse_command(line);

        if (command == NULL) {
            free(buffer);
            return false;
        }

        switch (command->type) {
            case COMMAND_SET:
                if (!hashmap_put(db->map, command->key, command->value)) {
                    command_destroy(command);
                    free(buffer);
                    return false;
                }
                break;

            case COMMAND_DEL:
                hashmap_remove(db->map, command->key);
                break;

            default:
                command_destroy(command);
                free(buffer);
                return false;

        }

        command_destroy(command);

        line = strtok(NULL, "\n");
    }

    free(buffer);

    return true;
}

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

    db->log_fd = open(
        LOG_FILE,
        O_RDWR | O_CREAT | O_APPEND,
        0644
    );

    if (db->log_fd == -1) {
        pthread_mutex_destroy(&db->mutex);
        hashmap_destroy(db->map);
        free(db);
        return NULL;
    }

    if (!replay_log(db)) {
        close(db->log_fd);
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

    close(db->log_fd);
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

    if (!append_log(db, "SET", key, value)) {
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

    if (!append_log(db, "DEL", key, NULL)) {
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
