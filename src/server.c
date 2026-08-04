#include "server.h"
#include "command.h"
#include "database.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

static void handle_client(int client_fd, struct database *db) {
    char read_buffer[BUFFER_SIZE];
    char input_buffer[BUFFER_SIZE];

    size_t input_length = 0;
    bool should_quit = false;

    while(!should_quit) {
        ssize_t bytes_read = read(client_fd, read_buffer, sizeof(read_buffer));

        if (bytes_read == -1) {
            perror("read");
            break;
        }

        if (bytes_read == 0) {
            printf("Client disconnected.\n");
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

        switch (command->type) {
            case COMMAND_PING:
                snprintf(response, sizeof(response), "PONG\n");
                break;

            case COMMAND_SET:
                snprintf(
                    response,
                    sizeof(response),
                    db_set(db, command->key, command->value) ? "OK\n" : "ERROR\n"
                );
                break;

            case COMMAND_GET: {
                const char *value = db_get(db, command->key);

                if (value == NULL) {
                    snprintf(response, sizeof(response), "NOT_FOUND\n");
                } else {
                    snprintf(response, sizeof(response), "%s\n", value);
                }
                break;
            }

            case COMMAND_DEL:
                snprintf(
                    response,
                    sizeof(response),
                    db_del(db, command->key) ? "OK\n" : "NOT_FOUND\n"
                );
                break;

            case COMMAND_QUIT:
                snprintf(response, sizeof(response), "BYE\n");
                should_quit = true;
                break;

            case COMMAND_INVALID:
            default:
                snprintf(response, sizeof(response), "ERROR\n");
                break;
        }

        if (write(client_fd, response, strlen(response)) == -1) {
            perror("write");
        }

        command_destroy(command);

        if (should_quit) {
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

int server_run(int port) {
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

    if (listen(server_fd, 10) == -1) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    struct database *db = db_create();

    if (db == NULL) {
        close(server_fd);
        return -1;
    }

    printf("Listening on port %d...\n", port);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd == -1) {
            perror("accept");
            break;
        }

        printf("Client connected\n");
    
        handle_client(client_fd, db);

        close(client_fd);

        printf("Client disconnected\n");
    }

    db_destroy(db);
    close(server_fd);

    return 0;
}
