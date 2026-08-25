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
#define CLIENT_COUNT 4
#define REQUESTS_PER_CLIENT 10000

struct benchmark_sync {
    pthread_mutex_t mutex;

    pthread_cond_t ready_condition;
    pthread_cond_t start_condition;
    pthread_cond_t done_condition;
    pthread_cond_t cleanup_condition;

    int ready_count;
    int done_count;
    int start;
    int cleanup_allowed;
    int abort;
};

struct benchmark_context {
    int client_id;
    int failed;
    struct benchmark_sync *sync;
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

static void benchmark_fail(struct benchmark_context *context) {
    context->failed = 1;

    pthread_mutex_lock(&context->sync->mutex);

    context->sync->abort = 1;

    pthread_cond_broadcast(&context->sync->ready_condition);
    pthread_cond_broadcast(&context->sync->start_condition);
    pthread_cond_broadcast(&context->sync->done_condition);
    pthread_cond_broadcast(&context->sync->cleanup_condition);

    pthread_mutex_unlock(&context->sync->mutex);
}

static void *benchmark_worker(void *arg) {
    struct benchmark_context *context = arg;

    int client_id = context->client_id;

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("socket");
        benchmark_fail(context);
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
        benchmark_fail(context);
        close(fd);
        return NULL;
    }

    if (connect(
        fd,
        (struct sockaddr *)&address,
        sizeof(address)
    ) == -1) {
        fprintf(stderr, "Client %d: connect failed: ", client_id);
        perror(NULL);

        benchmark_fail(context);
        close(fd);
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
        benchmark_fail(context);
        close(fd);
        return NULL;
    }

    if (read_line(fd, response, sizeof(response)) != 0) {
        fprintf(stderr, "Client %d: setup SET response failed\n", client_id);
        benchmark_fail(context);
        close(fd);
        return NULL;
    }

    if (strcmp(response, "OK\n") != 0) {
        fprintf(
            stderr,
            "Client %d: expected OK, received %s",
            client_id,
            response
        );

        benchmark_fail(context);
        close(fd);

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

    pthread_mutex_lock(&context->sync->mutex);

    context->sync->ready_count++;
    pthread_cond_signal(&context->sync->ready_condition);

    while (!context->sync->start && !context->sync->abort) {
        pthread_cond_wait(
            &context->sync->start_condition,
            &context->sync->mutex
        );
    }

    if (context->sync->abort) {
        pthread_mutex_unlock(&context->sync->mutex);
        close(fd);
        return NULL;
    }

    pthread_mutex_unlock(&context->sync->mutex);

    for (int i = 0; i < REQUESTS_PER_CLIENT; i++) {
        if (write_all(fd, command, strlen(command)) != 0) {
            fprintf(
                stderr,
                "Client %d: GET write failed at request %d\n",
                client_id,
                i
            );

            benchmark_fail(context);
            close(fd);
            return NULL;
        }

        if (read_line(fd, response, sizeof(response)) != 0) {
            fprintf(
                stderr,
                "Client %d: GET response failed at request %d\n",
                client_id,
                i
            );

            benchmark_fail(context);
            close(fd);
            return NULL;
        }

        if (strcmp(response, expected) != 0) {
            fprintf(
                stderr,
                "Unexpected response for client %d: %s",
                client_id,
                response
            );

            benchmark_fail(context);
            close(fd);
            return NULL;
        }
    }

    pthread_mutex_lock(&context->sync->mutex);

    context->sync->done_count++;
    pthread_cond_signal(&context->sync->done_condition);

    while(!context->sync->cleanup_allowed && !context->sync->abort) {
        pthread_cond_wait(
            &context->sync->cleanup_condition,
            &context->sync->mutex
        );
    }

    if (context->sync->abort) {
        pthread_mutex_unlock(&context->sync->mutex);
        close(fd);
        return NULL;
    }

    pthread_mutex_unlock(&context->sync->mutex);

    write_all(fd, "QUIT\n", 5);
    read_line(fd, response, sizeof(response));

    close(fd);

    return NULL;
}

int main(void) {
    struct benchmark_sync sync;

    pthread_mutex_init(&sync.mutex, NULL);
    pthread_cond_init(&sync.ready_condition, NULL);
    pthread_cond_init(&sync.start_condition, NULL);
    pthread_cond_init(&sync.done_condition, NULL);
    pthread_cond_init(&sync.cleanup_condition, NULL);

    sync.ready_count = 0;
    sync.done_count = 0;
    sync.start = 0;
    sync.cleanup_allowed = 0;
    sync.abort = 0;

    pthread_t threads[CLIENT_COUNT];
    struct benchmark_context contexts[CLIENT_COUNT];

    struct timespec start;
    struct timespec end;

    for (int i = 0; i < CLIENT_COUNT; i++) {
        contexts[i].client_id = i;
        contexts[i].failed = 0;
        contexts[i].sync = &sync;

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

    pthread_mutex_lock(&sync.mutex);

    while (sync.ready_count < CLIENT_COUNT && !sync.abort) {
        pthread_cond_wait(&sync.ready_condition, &sync.mutex);
    }

    if (!sync.abort) {
        clock_gettime(CLOCK_MONOTONIC, &start);

        sync.start = 1;

        pthread_cond_broadcast(&sync.start_condition);
    }

    pthread_mutex_unlock(&sync.mutex);

    pthread_mutex_lock(&sync.mutex);

    while (sync.done_count < CLIENT_COUNT && !sync.abort) {
        pthread_cond_wait(&sync.done_condition, &sync.mutex);
    }

    if (!sync.abort) {
        clock_gettime(CLOCK_MONOTONIC, &end);
    }

    sync.cleanup_allowed = 1;
    pthread_cond_broadcast(&sync.cleanup_condition);

    pthread_mutex_unlock(&sync.mutex);

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
        pthread_mutex_destroy(&sync.mutex);
        pthread_cond_destroy(&sync.ready_condition);
        pthread_cond_destroy(&sync.start_condition);
        pthread_cond_destroy(&sync.done_condition);
        pthread_cond_destroy(&sync.cleanup_condition);

        return EXIT_FAILURE;
    }

    double seconds = elapsed_seconds(start, end);

    size_t total_requests = (size_t)CLIENT_COUNT * REQUESTS_PER_CLIENT;
    
    double requests_per_second = (double)total_requests / seconds;

    printf("Concurrent GET benchmark\n");
    printf("Number of clients: %d\n", CLIENT_COUNT);
    printf("Requests per client: %d\n", REQUESTS_PER_CLIENT);
    printf("Total requests: %zu\n", total_requests);
    printf("Time: %.3f seconds\n", seconds);
    printf("Throughput: %.0f requests per second\n", requests_per_second);

    pthread_mutex_destroy(&sync.mutex);
    pthread_cond_destroy(&sync.ready_condition);
    pthread_cond_destroy(&sync.start_condition);
    pthread_cond_destroy(&sync.done_condition);
    pthread_cond_destroy(&sync.cleanup_condition);

    return EXIT_SUCCESS;
}
