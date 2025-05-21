#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) {
    keep_going = 0;
}

// thread function
void *thread_func(void *arg) {
    connection_queue_t *queue = (connection_queue_t *) arg;

    while (1) {
        int client_fd = connection_queue_dequeue(queue);
        if (client_fd == -1) {
            if (queue->shutdown)
                return NULL;
            continue;
        }

        // handle request
        char res_name[BUFSIZE];
        if (read_http_request(client_fd, res_name) == -1) {
            close(client_fd);
            continue;
        }

        // path to resource
        char res_path[BUFSIZE];
        snprintf(res_path, BUFSIZE, "%s%s", serve_dir, res_name);

        // respond to client
        write_http_response(client_fd, res_path);

        // clean up connection
        if (close(client_fd) == -1)
            perror("close");
    }

    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }

    serve_dir = argv[1];
    const char *port = argv[2];

    // handler
    struct sigaction sigact = {.sa_handler = handle_sigint, .sa_flags = 0};
    sigfillset(&sigact.sa_mask);

    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // socket address info
    struct addrinfo hints = {0}, *server;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int result = getaddrinfo(NULL, port, &hints, &server);
    if (result != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        return 1;
    }

    int sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        return 1;
    }

    if (bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        close(sock_fd);
        freeaddrinfo(server);
        return 1;
    }

    freeaddrinfo(server);

    if (listen(sock_fd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(sock_fd);
        return 1;
    }

    // Block signals in worker threads
    sigset_t newset, oldset;
    sigfillset(&newset);
    sigprocmask(SIG_SETMASK, &newset, &oldset);

    // Initialize queue
    connection_queue_t queue;
    if (connection_queue_init(&queue) == -1) {
        close(sock_fd);
        return 1;
    }

    // ceate worker threads
    pthread_t threads[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) {
        result = pthread_create(&threads[i], NULL, thread_func, &queue);
        if (result) {
            fprintf(stderr, "pthread_create: %s\n", strerror(result));
            connection_queue_shutdown(&queue);
            for (int j = 0; j < i; j++)
                pthread_join(threads[j], NULL);
            connection_queue_free(&queue);
            close(sock_fd);
            return 1;
        }
    }

    sigprocmask(SIG_SETMASK, &oldset, NULL);

    // cccept loop
    while (keep_going) {
        int client_fd = accept(sock_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR)
                break;
            perror("accept");
            break;
        }
        connection_queue_enqueue(&queue, client_fd);
    }

    // shutdown and cleanup
    int ret_val = 0;
    if (connection_queue_shutdown(&queue) == -1)
        ret_val = 1;

    if (close(sock_fd) == -1) {
        perror("close");
        ret_val = 1;
    }

    for (int i = 0; i < N_THREADS; i++) {
        result = pthread_join(threads[i], NULL);
        if (result) {
            fprintf(stderr, "pthread_join: %s\n", strerror(result));
            ret_val = 1;
        }
    }

    if (connection_queue_free(&queue) == -1)
        ret_val = 1;

    return ret_val;
}
