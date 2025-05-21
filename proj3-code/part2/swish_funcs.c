#include "swish_funcs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "string_vector.h"

#define MAX_ARGS 10

/*
 * Helper function to run a single command within a pipeline. You should make
 * make use of the provided 'run_command' function here.
 * tokens: String vector containing the tokens representing the command to be
 * executed, possible redirection, and the command's arguments.
 * pipes: An array of pipe file descriptors.
 * n_pipes: Length of the 'pipes' array
 * in_idx: Index of the file descriptor in the array from which the program
 *         should read its input, or -1 if input should not be read from a pipe.
 * out_idx: Index of the file descriptor in the array to which the program
 *          should write its output, or -1 if output should not be written to
 *          a pipe.
 * Returns 0 on success or -1 on error.
 */
int run_piped_command(strvec_t *tokens, int *pipes, int n_pipes, int in_idx, int out_idx) {
    if (in_idx >= 0 && dup2(pipes[in_idx], STDIN_FILENO) == -1) {
        perror("dup2");
        return -1;
    }

    if (out_idx >= 0 && dup2(pipes[out_idx], STDOUT_FILENO) == -1) {
        perror("dup2");
        return -1;
    }

    if (run_command(tokens) == -1) {
        return -1;
    }

    return 0;
}

int run_pipelined_commands(strvec_t *tokens) {
    // intialize
    int n_pipes = strvec_num_occurrences(tokens, "|");
    int n_cmds = n_pipes + 1;
    int n_fds = n_pipes * 2;
    int *pipe_fds = malloc(sizeof(int) * n_fds);
    if (!pipe_fds) {
        perror("malloc");
        return -1;
    }

    // pipe creation
    for (int i = 0; i < n_pipes; i++) {
        if (pipe(&pipe_fds[2 * i]) == -1) {
            perror("pipe");
            for (int j = 0; j < 2 * i; j++)
                close(pipe_fds[j]);
            free(pipe_fds);
            return -1;
        }
    }

    strvec_t *tokens_left = tokens;

    for (int i = n_cmds - 1; i >= 0; i--) {
        strvec_t cmd_tokens;
        int pipe_idx = strvec_find_last(tokens_left, "|");
        strvec_slice(tokens_left, &cmd_tokens, pipe_idx + 1, tokens_left->length);
        strvec_take(tokens_left, pipe_idx);

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            for (int j = 0; j < n_fds; j++)
                close(pipe_fds[j]);
            free(pipe_fds);
            strvec_clear(&cmd_tokens);
            return -1;
        }

        if (pid == 0) {
            int in_idx = (i > 0) ? (2 * (i - 1)) : -1;
            int out_idx = (i < n_pipes) ? (2 * i + 1) : -1;

            // Close all pipe FDs except in_idx and out_idx
            for (int j = 0; j < n_fds; j++) {
                if (j != in_idx && j != out_idx)
                    close(pipe_fds[j]);
            }

            if (run_piped_command(&cmd_tokens, pipe_fds, n_fds, in_idx, out_idx) == -1) {
                if (in_idx >= 0)
                    close(pipe_fds[in_idx]);
                if (out_idx >= 0)
                    close(pipe_fds[out_idx]);
                free(pipe_fds);
                strvec_clear(&cmd_tokens);
                exit(1);
            }

            if (in_idx >= 0)
                close(pipe_fds[in_idx]);
            if (out_idx >= 0)
                close(pipe_fds[out_idx]);
            free(pipe_fds);
            strvec_clear(&cmd_tokens);
            exit(0);
        }

        strvec_clear(&cmd_tokens);
    }

    int ret_val = 0;
    for (int i = 0; i < n_fds; i++) {
        if (close(pipe_fds[i]) == -1) {
            perror("close");
            ret_val = -1;
        }
    }

    // Wait for all child processes
    for (int i = 0; i < n_cmds; i++) {
        if (wait(NULL) == -1) {
            perror("wait");
            ret_val = -1;
        }
    }

    free(pipe_fds);
    return ret_val;
}
