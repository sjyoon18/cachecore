#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <pthread.h>

struct client_node;

struct client_manager {
    struct client_node *head;
    pthread_mutex_t mutex;
};

int client_manager_init(struct client_manager *manager);

int client_manager_add(
    struct client_manager *manager,
    int client_fd
);

void client_manager_remove(
    struct client_manager *manager,
    int client_fd
);

void client_manager_shutdown_all(
    struct client_manager *manager
);

void client_manager_destroy(
    struct client_manager *manager
);

#endif
