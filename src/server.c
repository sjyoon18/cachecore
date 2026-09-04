#include "server.h"
#include "command.h"
#include "database.h"
#include "thread_pool.h"
#include "client_manager.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define THREAD_COUNT 4
#define QUEUE_CAPACITY 16
#define LISTEN_BACKLOG 64
#define CLIENT_IDLE_TIMEOUT_SECONDS 10

static volatile sig_atomic_t shutdown_requested = 0;

static void handle_sigint(int signo) {
    (void)signo;
    shutdown_requested = 1;
}

struct client_context {
    int client_fd;
    struct database *db;
    struct client_manager *manager;
};

static bool set_client_receive_timeout(int client_fd) {
    struct timeval timeout = {
        .tv_sec = CLIENT_IDLE_TIMEOUT_SECONDS,
        .tv_usec = 0
    };

    if (setsockopt(
        client_fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    ) == -1) {
        perror("setsockopt SO_RCVTIMEO");
        return false;
    }

    return true;
}

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
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (written == 0) {
            return false;
        }

        total_written += (size_t)written;
    }

    return true;
}

static bool execute_command(
    struct database *db,
    struct command *command,
    char *response,
    size_t response_size,
    bool *should_quit
) {
    switch (command->type) {
        case COMMAND_PING:
            snprintf(response, response_size, "PONG\n");
            return true;

        case COMMAND_SET: {
            enum db_result result = db_set(db, command->key, command->value);

            switch(result) {
                case DB_OK:
                    snprintf(response, response_size, "OK\n");
                    return true;

                case DB_ERROR:
                    snprintf(response, response_size, "ERROR\n");
                    return true;
                
                case DB_FATAL:
                    fprintf(stderr, "Fatal database error during SET\n");
                    return false;

                case DB_NOT_FOUND:
                default:
                    snprintf(response, response_size, "ERROR\n");
                    return true;
            }
        }

        case COMMAND_GET: {
            char *value = db_get(db, command->key);

            if (value == NULL) {
                snprintf(response, response_size, "NOT_FOUND\n");
            } else {
                snprintf(response, response_size, "%s\n", value);
                free(value);
            }
            
            return true;
        }

        case COMMAND_DEL: {
            enum db_result result = db_del(db, command->key);
            
            switch(result) {
                case DB_OK:
                    snprintf(response, response_size, "OK\n");
                    return true;

                case DB_NOT_FOUND:
                    snprintf(response, response_size, "NOT_FOUND\n");
                    return true;

                case DB_ERROR:
                    snprintf(response, response_size, "ERROR\n");
                    return true;
                
                case DB_FATAL:
                    fprintf(stderr, "Fatal database error during DEL\n");
                    return false;

                default:
                    snprintf(response, response_size, "ERROR\n");
                    return true;
            }
        }

        case COMMAND_QUIT:
            snprintf(response, response_size, "BYE\n");
            *should_quit = true;
            return true;

        case COMMAND_INVALID:
        default:
            snprintf(response, response_size, "ERROR\n");
            return true;
    }
}

static void handle_client(int client_fd, struct database *db) {
    char read_buffer[BUFFER_SIZE];
    char input_buffer[BUFFER_SIZE];

    size_t input_length = 0;
    bool should_quit = false;

    while(!should_quit) {
        ssize_t bytes_read = read(client_fd, read_buffer, sizeof(read_buffer));

        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Client read timed out\n");
                break;
            }

            perror("read");
            break;
        }

        if (bytes_read == 0) {
            printf("Client disconnected\n");
            break;
        }

        size_t received = (size_t)bytes_read;

        if (input_length + received > sizeof(input_buffer)) {
            printf("Input too large.\n");
            break;
        }

        memcpy(
            input_buffer + input_length,
            read_buffer,
            received
        );

        input_length += received;

        char *newline;

        while ((newline = memchr(input_buffer, '\n', input_length)) != NULL) {
            size_t command_length = (size_t)(newline - input_buffer) + 1;

            char command_buffer[BUFFER_SIZE];

            if (command_length >= sizeof(command_buffer)) {
                printf("Command too large.\n");
                should_quit = true;
                break;
            }

            memcpy(
                command_buffer,
                input_buffer,
                command_length
            );

            command_buffer[command_length] = '\0';

            struct command *command = parse_command(command_buffer);

            if (command == NULL) {
                should_quit = true;
                break;
            }

            char response[BUFFER_SIZE];

            if (!execute_command(
                db,
                command,
                response,
                sizeof(response),
                &should_quit
            )) {
                command_destroy(command);
                exit(EXIT_FAILURE);
            }

            if (!write_all(
                client_fd,
                response,
                strlen(response)
            )) {
                perror("write");
                command_destroy(command);
                return;
            }

            command_destroy(command);

            if (should_quit) {
                printf("Client disconnected\n");
                break;
            }

            size_t remaining = input_length - command_length;

            memmove(
                input_buffer,
                input_buffer + command_length,
                remaining
            );

            input_length = remaining;
        }
    }
}

static void client_task(void *arg) {
    struct client_context *context = arg;

    int client_fd = context->client_fd;
    struct database *db = context->db;
    struct client_manager *manager = context->manager;

    free(context);

    handle_client(client_fd, db);

    client_manager_remove(manager,client_fd);
    close(client_fd);
}

int server_run(int port) {
    struct sigaction sigpipe_action;
    memset(&sigpipe_action, 0, sizeof(sigpipe_action));

    sigpipe_action.sa_handler = SIG_IGN;
    sigemptyset(&sigpipe_action.sa_mask);

    if (sigaction(SIGPIPE, &sigpipe_action, NULL) == -1) {
        perror("sigaction SIGPIPE");
        return -1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, LISTEN_BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    struct database *db = db_create();

    if (db == NULL) {
        close(server_fd);
        return -1;
    }

    struct thread_pool pool;

    if (thread_pool_init(&pool, THREAD_COUNT, QUEUE_CAPACITY) != 0) {
        db_destroy(db);
        close(server_fd);
        return -1;
    }

    struct client_manager manager;

    if (client_manager_init(&manager) != 0) {
        thread_pool_destroy(&pool);
        db_destroy(db);
        close(server_fd);
        return -1;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));

    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("sigaction");
        client_manager_destroy(&manager);
        thread_pool_destroy(&pool);
        db_destroy(db);
        close(server_fd);
        return -1;
    }

    printf("Listening on port %d...\n", port);

    while (!shutdown_requested) {
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd == -1) {
            if (errno == EINTR && shutdown_requested) {
                break;
            }

            perror("accept");
            break;
        }

        if (!set_client_receive_timeout(client_fd)) {
            close(client_fd);
            continue;
        }

        if (client_manager_add(&manager, client_fd) != 0) {
            close(client_fd);
            continue;
        }

        printf("Client connected\n");
    
        struct client_context *context = malloc(sizeof(*context));

        if (context == NULL) {
            fprintf(stderr, "Client rejected: malloc fail\n");
            client_manager_remove(&manager, client_fd);
            close(client_fd);
            continue;
        }

        context->client_fd = client_fd;
        context->db = db;
        context->manager = &manager;

        if (thread_pool_submit(&pool, client_task, context) != 0) {
            fprintf(stderr, "Client rejected: thread pool queue full\n");
            free(context);
            client_manager_remove(&manager, client_fd);
            close(client_fd);
            continue;
        }
    }

    client_manager_shutdown_all(&manager);
    thread_pool_destroy(&pool);
    client_manager_destroy(&manager);
    db_destroy(db);
    close(server_fd);

    return 0;
}
