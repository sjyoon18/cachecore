#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_PORT 6379
#define SERVER_ADDRESS "127.0.0.1"

#define BUFFER_SIZE 1024
#define ITERATIONS 5000
#define THREAD_COUNT 16

#define STRESS_OK NULL
#define STRESS_FAIL ((void *)1)

struct stress_context {
    int thread_id;
};

static int write_all(
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

        if (written <= 0) {
            return -1;
        }

        total_written += (size_t)written;
    }

    return 0;
}

static int read_line(
    int fd,
    char *buffer,
    size_t capacity
) {
    size_t length = 0;

    while (length + 1 < capacity) {
        char byte;

        ssize_t bytes_read = read(fd, &byte, 1);

        if (bytes_read <= 0) {
            return -1;
        }

        buffer[length++] = byte;

        if (byte == '\n') {
            buffer[length] = '\0';
            return 0;
        }
    }

    return -1;
}

static int send_command(
    int fd,
    const char *command,
    const char *expected_response
) {
    if (write_all(fd, command, strlen(command)) != 0) {
        perror("write");
        return -1;
    }

    char response[BUFFER_SIZE];

    if (read_line(fd, response, sizeof(response)) != 0) {
        fprintf(stderr, "Failed to read response\n");
        return -1;
    }

    if (strcmp(response, expected_response) != 0) {
        fprintf(
            stderr,
            "Unexpected response.\n"
            "Command: %s"
            "Expected: %s"
            "Received: %s",
            command,
            expected_response,
            response
        );

        return -1;
    }

    return 0;
}

static void *stress_worker(void *arg) {
    struct stress_context *context = arg;

    int thread_id = context->thread_id;

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("socket");
        return STRESS_FAIL;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(SERVER_PORT);

    if (inet_pton(
        AF_INET,
        SERVER_ADDRESS,
        &address.sin_addr
    ) != 1) {
        fprintf(stderr, "inet_pton failed\n");
        close(fd);
        return STRESS_FAIL;
    }

    if (connect(
        fd,
        (struct sockaddr *)&address,
        sizeof(address)
    ) == -1) {
        perror("connect");
        close(fd);
        return STRESS_FAIL;
    }

    char command[BUFFER_SIZE];
    char expected[BUFFER_SIZE];

    for (int i = 0; i < ITERATIONS; i++) {
        snprintf(
            command,
            sizeof(command),
            "SET thread%d_key value_%d\n",
            thread_id,
            i
        );

        if (send_command(fd, command, "OK\n") != 0) {
            close(fd);
            return STRESS_FAIL;
        }

        snprintf(
            command,
            sizeof(command),
            "GET thread%d_key\n",
            thread_id
        );

        snprintf(
            expected,
            sizeof(expected),
            "value_%d\n",
            i
        );

        if (send_command(fd, command, expected) != 0) {
            close(fd);
            return STRESS_FAIL;
        }
    }

    if (send_command(fd, "QUIT\n", "BYE\n") != 0) {
        close(fd);
        return STRESS_FAIL;
    }

    close(fd);

    return STRESS_OK;
}

int main(void) {
    pthread_t threads[THREAD_COUNT];
    struct stress_context contexts[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        contexts[i].thread_id = i;

        if (pthread_create(
            &threads[i],
            NULL,
            stress_worker,
            &contexts[i]
        ) != 0) {
            fprintf(stderr, "Failed to create thread\n");
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        void *result = NULL;

        if (pthread_join(
            threads[i],
            &result
        ) != 0) {
            fprintf(stderr, "Failed to join thread\n");
            return EXIT_FAILURE;
        }

        if (result != STRESS_OK) {
            fprintf(stderr, "Stress worker %d failed\n", i);
            return EXIT_FAILURE;
        }
    }

    printf(
        "%d stress clients completed successfully.\n",
        THREAD_COUNT
    );

    return EXIT_SUCCESS;
}
