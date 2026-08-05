#include "thread_pool.h"

#include <stdlib.h>

static void *worker_thread(void *arg) {
    struct thread_pool *pool = arg;

    while(1) {
        pthread_mutex_lock(&pool->mutex);

        while (queue_empty(&pool->queue) && !pool->shutdown) {
            pthread_cond_wait(&pool->condition, &pool->mutex);
        }

        if (pool->shutdown && queue_empty(&pool->queue)) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        struct job *job = dequeue(&pool->queue);
        pool->queue_size--;
        pthread_mutex_unlock(&pool->mutex);
        job->function(job->arg);

        free(job);
    }

    return NULL;
}

int thread_pool_init(
    struct thread_pool *pool,
    int num_threads,
    size_t queue_capacity
) {
    pool->num_threads = num_threads;
    pool->workers = malloc(sizeof(pthread_t) * num_threads);

    if (pool->workers == NULL) {
        return -1;
    }

    queue_init(&pool->queue);

    pool->queue_size = 0;
    pool->queue_capacity = queue_capacity;

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->condition, NULL);

    pool->shutdown = 0;

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->workers[i], NULL, worker_thread, pool) != 0) {
            return -1;
        }
    }

    return 0;
}

int thread_pool_submit(
    struct thread_pool *pool,
    void (*function)(void *),
    void *arg
) {
    struct job *job = malloc(sizeof(struct job));

    if (job == NULL) {
        return -1;
    }

    job->function = function;
    job->arg = arg;
    job->next = NULL;

    pthread_mutex_lock(&pool->mutex);

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutex);
        free(job);
        return -1;
    }

    if (pool->queue_size >= pool->queue_capacity) {
        pthread_mutex_unlock(&pool->mutex);
        free(job);
        return -1;
    }

    enqueue(&pool->queue, job);
    pool->queue_size++;
    
    pthread_cond_signal(&pool->condition);
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

void thread_pool_destroy(struct thread_pool *pool) {
    pthread_mutex_lock(&pool->mutex);

    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->condition);

    pthread_mutex_unlock(&pool->mutex);

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->workers[i], NULL);
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->condition);

    free(pool->workers);
}
