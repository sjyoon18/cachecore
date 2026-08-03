#include "server.h"
#include "command.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

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

    printf("Listening on port %d...\n", port);

    int client_fd = accept(server_fd, NULL, NULL);

    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        return -1;
    }

    char buffer[BUFFER_SIZE];

    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read == -1) {
        perror("read");
        close(client_fd);
        close(server_fd);
        return -1;
    }

    buffer[bytes_read] = '\0';

    printf("Received: %s", buffer);

    struct command *command = parse_command(buffer);

    if (command == NULL) {
        close(client_fd);
        close(server_fd);
        return -1;
    }

    const char *response;

    switch (command->type) {
        case COMMAND_PING:
            response = "PONG\n";
            break;
        case COMMAND_SET:
            response = "SET command received\n";
            break;
        case COMMAND_GET:
            response = "GET command received\n";
            break;
        case COMMAND_DEL:
            response = "DEL command received\n";
            break;
        case COMMAND_QUIT:
            response = "QUIT command received\n";
            break;
        case COMMAND_INVALID:
        default:
            response = "ERROR\n";
            break;
    }

    if (write(client_fd, response, strlen(response)) == -1) {
        perror("write");
    }

    command_destroy(command);

    close(client_fd);
    close(server_fd);

    return 0;
}
