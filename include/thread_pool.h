#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stddef.h>

typedef void (*TaskHandler)(int client_fd, void *context);

typedef struct {
    int *fds;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_t *threads;
    size_t thread_count;
    int stopping;
    TaskHandler handler;
    void *context;
} ThreadPool;

int thread_pool_init(ThreadPool *pool, size_t thread_count, size_t queue_capacity, TaskHandler handler, void *context);
int thread_pool_submit(ThreadPool *pool, int client_fd);
void thread_pool_shutdown(ThreadPool *pool);

#endif
