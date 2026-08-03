#include <stdio.h>
#include <stdlib.h>

#include "server.h"

int main(void) {
    const int port = 6379;

    printf("CacheCore starting on port %d...\n", port);

    if (server_run(port) != 0) {
        fprintf(stderr, "CacheCore failed to start.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
