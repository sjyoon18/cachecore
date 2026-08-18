#include "client_manager.h"

#include <stdlib.h>
#include <sys/socket.h>

struct client_node {
    int client_fd;
    struct client_node *next;
};

int client_manager_init(struct client_manager *manager) {
    if (manager == NULL) {
        return -1;
    }

    manager->head = NULL;

    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        return -1;
    }

    return 0;
}

int client_manager_add(
    struct client_manager *manager,
    int client_fd
) {
    if (manager == NULL) {
        return -1;
    }

    struct client_node *node = malloc(sizeof(*node));

    if (node == NULL) {
        return -1;
    }

    node->client_fd = client_fd;

    pthread_mutex_lock(&manager->mutex);

    node->next = manager->head;
    manager->head = node;

    pthread_mutex_unlock(&manager->mutex);

    return 0;
}

void client_manager_remove(
    struct client_manager *manager,
    int client_fd
) {
    if (manager == NULL) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    struct client_node *previous = NULL;
    struct client_node *current = manager->head;

    while (current != NULL) {
        if (current->client_fd == client_fd) {
            if (previous == NULL) {
                manager->head = current->next;
            } else {
                previous->next = current->next;
            }

            free(current);
            pthread_mutex_unlock(&manager->mutex);
            return;
        }

        previous = current;
        current = current->next;
    }

    pthread_mutex_unlock(&manager->mutex);
}

void client_manager_shutdown_all(struct client_manager *manager) {
    if (manager == NULL) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    struct client_node *current = manager->head;

    while (current != NULL) {
        shutdown(current->client_fd, SHUT_RDWR);
        current = current->next;
    }

    pthread_mutex_unlock(&manager->mutex);
}

void client_manager_destroy(struct client_manager *manager) {
    if (manager == NULL) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    struct client_node *current = manager->head;

    while (current != NULL) {
        struct client_node *next = current->next;
        free(current);
        current = next;
    }

    manager->head = NULL;

    pthread_mutex_unlock(&manager->mutex);

    pthread_mutex_destroy(&manager->mutex);
}
