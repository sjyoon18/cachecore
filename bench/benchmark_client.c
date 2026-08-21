#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_PORT 6379
#define SERVER_ADDRESS "127.0.0.1"
#define BUFFER_SIZE 1024
#define CLIENT_COUNT 16
#define REQUESTS_PER_CLIENT 10000

struct benchmark_context {
    int client_id;
    int failed;
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

static double elapsed_seconds(
    struct timespec start,
    struct timespec end
) {
    double seconds = (double)(end.tv_sec - start.tv_sec);
    double nanoseconds = (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;

    return seconds + nanoseconds;
}

static void *benchmark_worker(void *arg) {
    struct benchmark_context *context = arg;

    int client_id = context->client_id;

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("socket");
        context->failed = 1;
        return NULL;
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
        context->failed = 1;
        return NULL;
    }

    if (connect(
        fd,
        (struct sockaddr *)&address,
        sizeof(address)
    ) == -1) {
        fprintf(stderr, "Client %d: connect failed: ", client_id);
        perror(NULL);

        close(fd);
        context->failed = 1;
        return NULL;
    }

    char command[BUFFER_SIZE];
    char expected[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    snprintf(
        command,
        sizeof(command),
        "SET bench_key_%d value_%d\n",
        client_id,
        client_id
    );

    if (write_all(fd, command, strlen(command)) != 0) {
        fprintf(stderr, "Client %d: setup SET write failed\n", client_id);
        close(fd);
        context->failed = 1;
        return NULL;
    }

    if (read_line(fd, response, sizeof(response)) != 0) {
        fprintf(stderr, "Client %d: setup SET response failed\n", client_id);
        close(fd);
        context->failed = 1;
        return NULL;
    }

    if (strcmp(response, "OK\n") != 0) {
        fprintf(
            stderr,
            "Client %d: expected OK, received %s",
            client_id,
            response
        );

        close(fd);
        context->failed = 1;
        return NULL;
    }

    snprintf(
        command,
        sizeof(command),
        "GET bench_key_%d\n",
        client_id
    );

    snprintf(
        expected,
        sizeof(expected),
        "value_%d\n",
        client_id
    );

    for (int i = 0; i < REQUESTS_PER_CLIENT; i++) {
        if (write_all(fd, command, strlen(command)) != 0) {
            fprintf(
                stderr,
                "Client %d: GET write failed at request %d\n",
                client_id,
                i
            );

            context->failed = 1;
            return NULL;
        }

        if (read_line(fd, response, sizeof(response)) != 0) {
            fprintf(
                stderr,
                "Client %d: GET response failed at request %d\n",
                client_id,
                i
            );

            context->failed = 1;
            return NULL;
        }

        if (strcmp(response, expected) != 0) {
            fprintf(
                stderr,
                "Unexpected response for client %d: %s",
                client_id,
                response
            );

            context->failed = 1;
            return NULL;
        }
    }

    write_all(fd, "QUIT\n", 5);
    read_line(fd, response, sizeof(response));

    close(fd);

    return NULL;
}

int main(void) {
    pthread_t threads[CLIENT_COUNT];
    struct benchmark_context contexts[CLIENT_COUNT];

    struct timespec start;
    struct timespec end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < CLIENT_COUNT; i++) {
        contexts[i].client_id = i;
        contexts[i].failed = 0;

        if (pthread_create(
            &threads[i],
            NULL,
            benchmark_worker,
            &contexts[i]
        ) != 0) {
            fprintf(stderr, "Failed to create benchmark thread\n");
            return EXIT_FAILURE;
        }
    }

    int any_failed = 0;

    for (int i = 0; i < CLIENT_COUNT; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Failed to join benchmark thread\n");
            any_failed = 1;
            continue;
        }

        if (contexts[i].failed) {
            fprintf(stderr, "Benchmark worker %d failed\n", i);
            any_failed = 1;
        }
    }

    if (any_failed) {
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double seconds = elapsed_seconds(start, end);

    size_t total_requests = (size_t)CLIENT_COUNT * REQUESTS_PER_CLIENT;
    
    double requests_per_second = (double)total_requests / seconds;

    printf("Concurrent GET benchmark\n");
    printf("Number of clients: %d\n", CLIENT_COUNT);
    printf("Requests per client: %d\n", REQUESTS_PER_CLIENT);
    printf("Total requests: %zu\n", total_requests);
    printf("Time: %.3f seconds\n", seconds);
    printf("Throughput: %.0f requests per second\n", requests_per_second);

    return EXIT_SUCCESS;
}
