#include "queue.h"

#include <stddef.h>

void queue_init(struct job_queue *queue) {
    queue->head = NULL;
    queue->tail = NULL;
}

void enqueue(struct job_queue *queue, struct job *job) {
    job->next = NULL;

    if (queue->head == NULL) {
        queue->head = job;
        queue->tail = job;
        return;
    }

    queue->tail->next = job;
    queue->tail = job;
}

struct job *dequeue(struct job_queue *queue) {
    struct job *job = queue->head;

    if (job == NULL) {
        return NULL;
    }

    queue->head = job->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    job->next = NULL;

    return job;
}

int queue_empty(struct job_queue *queue) {
    return (queue->head == NULL);
}
