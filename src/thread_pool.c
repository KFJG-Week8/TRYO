#include "thread_pool.h"

#include <stdlib.h>
#include <string.h>

static void *worker_loop(void *arg)
{
    ThreadPool *pool = arg;

    while (1) {
        int client_fd;

        pthread_mutex_lock(&pool->mutex);
        while (!pool->stopping && pool->count == 0) {
            pthread_cond_wait(&pool->not_empty, &pool->mutex);
        }

        if (pool->stopping && pool->count == 0) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        client_fd = pool->fds[pool->head];
        pool->head = (pool->head + 1) % pool->capacity;
        pool->count--;
        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->mutex);

        pool->handler(client_fd, pool->context);
    }

    return NULL;
}

int thread_pool_init(ThreadPool *pool, size_t thread_count, size_t queue_capacity, TaskHandler handler, void *context)
{
    memset(pool, 0, sizeof(*pool));

    if (thread_count == 0 || queue_capacity == 0 || handler == NULL) {
        return 0;
    }

    pool->fds = malloc(sizeof(int) * queue_capacity);
    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    if (pool->fds == NULL || pool->threads == NULL) {
        free(pool->fds);
        free(pool->threads);
        return 0;
    }

    pool->capacity = queue_capacity;
    pool->handler = handler;
    pool->context = context;

    if (pthread_mutex_init(&pool->mutex, NULL) != 0 ||
        pthread_cond_init(&pool->not_empty, NULL) != 0 ||
        pthread_cond_init(&pool->not_full, NULL) != 0) {
        free(pool->fds);
        free(pool->threads);
        return 0;
    }

    for (size_t i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_loop, pool) != 0) {
            thread_pool_shutdown(pool);
            return 0;
        }
        pool->thread_count++;
    }

    return 1;
}

int thread_pool_submit(ThreadPool *pool, int client_fd)
{
    pthread_mutex_lock(&pool->mutex);

    while (!pool->stopping && pool->count == pool->capacity) {
        pthread_cond_wait(&pool->not_full, &pool->mutex);
    }

    if (pool->stopping) {
        pthread_mutex_unlock(&pool->mutex);
        return 0;
    }

    pool->fds[pool->tail] = client_fd;
    pool->tail = (pool->tail + 1) % pool->capacity;
    pool->count++;
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
    return 1;
}

void thread_pool_shutdown(ThreadPool *pool)
{
    if (pool->fds == NULL && pool->threads == NULL) {
        return;
    }

    pthread_mutex_lock(&pool->mutex);
    pool->stopping = 1;
    pthread_cond_broadcast(&pool->not_empty);
    pthread_cond_broadcast(&pool->not_full);
    pthread_mutex_unlock(&pool->mutex);

    for (size_t i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);
    pthread_mutex_destroy(&pool->mutex);
    free(pool->fds);
    free(pool->threads);
    memset(pool, 0, sizeof(*pool));
}
