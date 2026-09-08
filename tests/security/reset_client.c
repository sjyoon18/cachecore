#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 6379
#define SERVER_ADDRESS "127.0.0.1"
#define REQUEST_COUNT 100

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

        if (written == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (written == 0) {
            return -1;
        }

        total_written += (size_t)written;
    }

    return 0;
}

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    if (connect(
        fd,
        (struct sockaddr *)&address,
        sizeof(address)
    ) == -1) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }

    struct linger linger_option = {
        .l_onoff = 1,
        .l_linger = 0
    };

    if (setsockopt(
        fd,
        SOL_SOCKET,
        SO_LINGER,
        &linger_option,
        sizeof(linger_option)
    ) == -1) {
        perror("setsockopt");
        close(fd);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < REQUEST_COUNT; i++) {
        if (write_all(fd, "PING\n", 5) != 0) {
            fprintf(stderr, "write_all failed\n");
            close(fd);
            return EXIT_FAILURE;
        }
    }

    if (close(fd) == -1) {
        perror("close");
        return EXIT_FAILURE;
    }

    printf("Sent %d PING requests and reset the connection.\n", REQUEST_COUNT);

    return EXIT_SUCCESS;
}
