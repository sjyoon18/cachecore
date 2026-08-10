#include "aof.h"
#include "hashmap.h"
#include "command.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define LOG_BUFFER_SIZE 1024
#define LOG_FILE "cachecore.aof"

struct aof {
    int fd;
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

struct aof *aof_open(void) {

    struct aof *aof = malloc(sizeof(*aof));

    if (aof == NULL) {
        return NULL;
    }
    
    aof->fd = open(
        LOG_FILE,
        O_RDWR | O_CREAT | O_APPEND,
        0644
    );

    if (aof->fd == -1) {
        free(aof);
        return NULL;
    }

    return aof;
}


bool aof_append_set(
    struct aof *aof,
    const char *key,
    const char *value
) {
    if (aof == NULL || key == NULL || value == NULL) {
        return false;
    }

    char buffer[LOG_BUFFER_SIZE];

    int length = snprintf(
        buffer,
        sizeof(buffer),
        "SET %s %s\n",
        key, value
    );

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return false;
    }

    if (!write_all(aof->fd, buffer, (size_t)length)) {
        return false;
    }

    if (fsync(aof->fd) == -1) {
        return false;
    }

    return true;
}


bool aof_append_del(
    struct aof *aof,
    const char *key
) {
    if (aof == NULL || key == NULL) {
        return false;
    }

    char buffer[LOG_BUFFER_SIZE];

    int length = snprintf(
        buffer,
        sizeof(buffer),
        "DEL %s\n",
        key
    );

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return false;
    }

    if (!write_all(aof->fd, buffer, (size_t)length)) {
        return false;
    }

    if (fsync(aof->fd) == -1) {
        return false;
    }

    return true;
}

bool aof_replay(struct aof *aof, struct hashmap *map) {
    if (aof == NULL || map == NULL) {
        return false;
    }

    char *buffer = NULL;
    struct command *command = NULL;
    bool success = false;

    off_t file_size = lseek(aof->fd, 0, SEEK_END);

    if (file_size == -1) {
        return false;
    }

    if (file_size == 0) {
        return true;
    }

    if (lseek(aof->fd, 0, SEEK_SET) == -1) {
        return false;
    }

    buffer = malloc((size_t)file_size + 1);

    if (buffer == NULL) {
        return false;
    }

    size_t total_read = 0;

    while (total_read < (size_t)file_size) {
        ssize_t bytes_read = read(
            aof->fd,
            buffer + total_read,
            (size_t)file_size - total_read
        );

        if (bytes_read == -1) {
            goto cleanup;
        }

        if (bytes_read == 0) {
            break;
        }

        total_read += (size_t)bytes_read;
    }

    if (total_read != (size_t)file_size) {
        goto cleanup;
    }

    if (buffer[total_read - 1] != '\n') {
        while(total_read > 0 && buffer[total_read - 1] != '\n') {
            total_read--;
        }
        if (ftruncate(aof->fd, (off_t)total_read) == -1) {
            free(buffer);
            return false;
        }
    }

    buffer[total_read] = '\0';

    char *saveptr = NULL;
    char *line = strtok_r(buffer, "\n", &saveptr);

    while (line != NULL) {
        command = parse_command(line);

        if (command == NULL) {
            goto cleanup;
        }

        switch (command->type) {
            case COMMAND_SET:
                if (!hashmap_put(map, command->key, command->value)) {
                    goto cleanup;
                }
                break;

            case COMMAND_DEL:
                hashmap_remove(map, command->key);
                break;

            default:
                goto cleanup;

        }

        command_destroy(command);
        command = NULL;

        line = strtok_r(NULL, "\n", &saveptr);
    }

    success = true;

    cleanup:
        command_destroy(command);
        free(buffer);
        return success;
}

void aof_close(struct aof *aof) {
    if (aof == NULL) {
        return;
    }
    close(aof->fd);
    free(aof);
}
