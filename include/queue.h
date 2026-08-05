#ifndef QUEUE_H
#define QUEUE_H

struct job {
    void (*function)(void *);
    void *arg;
    struct job *next;
};

struct job_queue {
    struct job *head;
    struct job *tail;
};

void queue_init(struct job_queue *queue);

void enqueue(struct job_queue *queue, struct job *job);

struct job *dequeue(struct job_queue *queue);

int queue_empty(struct job_queue *queue);

#endif
