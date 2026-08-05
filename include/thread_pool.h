#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "queue.h"

#include <pthread.h>

struct thread_pool {
    pthread_t *workers;
    int num_threads;

    struct job_queue queue;

    pthread_mutex_t mutex;
    pthread_cond_t condition;

    int shutdown;
};

int thread_pool_init(struct thread_pool *pool, int num_threads);

int thread_pool_submit(
    struct thread_pool *pool,
    void (*function)(void *),
    void *arg
);

void thread_pool_destroy(struct thread_pool *pool);

#endif
