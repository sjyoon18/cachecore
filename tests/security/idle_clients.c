#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 6379
#define SERVER_ADDRESS "127.0.0.1"
#define IDLE_CLIENT_COUNT 4

static int connect_to_server(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("socket");
        return -1;
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
        return -1;
    }

    if (connect(
        fd,
        (struct sockaddr *)&address,
        sizeof(address)
    ) == -1) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

int main(void) {
    int client_fds[IDLE_CLIENT_COUNT];

    for (int i = 0; i < IDLE_CLIENT_COUNT; i++) {
        client_fds[i] = connect_to_server();
        if (client_fds[i] == -1) {
            for (int j = 0; j < i; j++) {
                close(client_fds[j]);
            }

            return EXIT_FAILURE;
        }
    }

    printf(
        "Opened %d idle connections. Press Enter to close them.\n",
        IDLE_CLIENT_COUNT
    );
    fflush(stdout);

    int exit_status = EXIT_SUCCESS;

    if (getchar() == EOF) {
        fprintf(stderr, "Failed to wait for input\n");
        exit_status = EXIT_FAILURE;
    }

    for (int i = 0; i < IDLE_CLIENT_COUNT; i++) {
        if (close(client_fds[i]) == -1) {
            perror("close");
            exit_status = EXIT_FAILURE;
        }
    }

    return exit_status;
}
