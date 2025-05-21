#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

int connection_queue_init(connection_queue_t *queue) {
    // TODO Not yet implemented
    int result;

    // initialize queue metadata
    queue->length = 0;
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->shutdown = 0;

    // initialize mutex
    result = pthread_mutex_init(&queue->lock, NULL);
    if (result) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(result));
        return -1;
    }

    // initialize condition variables
    result = pthread_cond_init(&queue->full, NULL);
    if (result) {
        fprintf(stderr, "pthread_cond_init (full): %s\n", strerror(result));
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }

    result = pthread_cond_init(&queue->empty, NULL);
    if (result) {
        fprintf(stderr, "pthread_cond_init (empty): %s\n", strerror(result));
        pthread_mutex_destroy(&queue->lock);
        pthread_cond_destroy(&queue->full);
        return -1;
    }

    return 0;
}

int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    // TODO Not yet implemented
    int result;

    // lock the queue
    result = pthread_mutex_lock(&queue->lock);
    if (result) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }

    while (queue->length == CAPACITY) {
        // exit if queue is shutting down
        if (queue->shutdown) {
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }

        // wait for space to enqueue
        result = pthread_cond_wait(&queue->full, &queue->lock);
        if (result) {
            fprintf(stderr, "pthread_cond_wait (full): %s\n", strerror(result));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    // enqueue connection
    queue->client_fds[queue->write_idx] = connection_fd;
    queue->length++;
    if (++queue->write_idx == CAPACITY)
        queue->write_idx = 0;

    // notify waiting threads
    result = pthread_cond_signal(&queue->empty);
    if (result) {
        fprintf(stderr, "pthread_cond_signal (empty): %s\n", strerror(result));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    // unlock the queue
    result = pthread_mutex_unlock(&queue->lock);
    if (result) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int connection_queue_dequeue(connection_queue_t *queue) {
    // TODO Not yet implemented
    int result;

    // lock the queue
    result = pthread_mutex_lock(&queue->lock);
    if (result) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }

    while (queue->length == 0) {
        // exit if shutting down
        if (queue->shutdown) {
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }

        // wait for available item
        result = pthread_cond_wait(&queue->empty, &queue->lock);
        if (result) {
            fprintf(stderr, "pthread_cond_wait (empty): %s\n", strerror(result));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    // dequeue connection
    int fd = queue->client_fds[queue->read_idx];
    queue->length--;
    if (++queue->read_idx == CAPACITY)
        queue->read_idx = 0;

    // notify waiting threads
    result = pthread_cond_signal(&queue->full);
    if (result) {
        fprintf(stderr, "pthread_cond_signal (full): %s\n", strerror(result));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    // unlock the queue
    result = pthread_mutex_unlock(&queue->lock);
    if (result) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }

    return fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    // TODO Not yet implemented
    int ret_val = 0;
    int result;

    queue->shutdown = 1;

    // notify all waiting threads
    result = pthread_cond_broadcast(&queue->full);
    if (result) {
        fprintf(stderr, "pthread_cond_broadcast (full): %s\n", strerror(result));
        ret_val = -1;
    }

    result = pthread_cond_broadcast(&queue->empty);
    if (result) {
        fprintf(stderr, "pthread_cond_broadcast (empty): %s\n", strerror(result));
        ret_val = -1;
    }

    return ret_val;
}

int connection_queue_free(connection_queue_t *queue) {
    // TODO Not yet implemented
    int ret_val = 0;
    int result;

    // destroy mutex and condition variables
    result = pthread_mutex_destroy(&queue->lock);
    if (result) {
        fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(result));
        ret_val = -1;
    }

    result = pthread_cond_destroy(&queue->full);
    if (result) {
        fprintf(stderr, "pthread_cond_destroy (full): %s\n", strerror(result));
        ret_val = -1;
    }

    result = pthread_cond_destroy(&queue->empty);
    if (result) {
        fprintf(stderr, "pthread_cond_destroy (empty): %s\n", strerror(result));
        ret_val = -1;
    }

    return ret_val;
}
