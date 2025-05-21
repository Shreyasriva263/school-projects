#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALPHABET_LEN 26

/*
 * Counts the number of occurrences of each letter (case insensitive) in a text
 * file and stores the results in an array.
 * file_name: The name of the text file in which to count letter occurrences
 * counts: An array of integers storing the number of occurrences of each letter.
 *     counts[0] is the number of 'a' or 'A' characters, counts [1] is the number
 *     of 'b' or 'B' characters, and so on.
 * Returns 0 on success or -1 on error.
 */
int count_letters(const char *file_name, int *counts) {
    FILE *fd = fopen(file_name, "r");
    if (fd == NULL) {
        perror("fopen");
        return -1;
    }
    // initialize count array
    for (int i = 0; i < ALPHABET_LEN; i++) {
        counts[i] = 0;
    }
    int ch;
    while ((ch = fgetc(fd)) != EOF) {
        if (isalpha(ch)) {
            ch = tolower(ch);
            counts[ch - 'a']++;
        }
    }
    fclose(fd);

    return 0;
}

/*
 * Processes a particular file(counting occurrences of each letter)
 *     and writes the results to a file descriptor.
 * This function should be called in child processes.
 * file_name: The name of the file to analyze.
 * out_fd: The file descriptor to which results are written
 * Returns 0 on success or -1 on error
 */
int process_file(const char *file_name, int out_fd) {
    int counts[ALPHABET_LEN];
    memset(counts, 0, sizeof(int) * ALPHABET_LEN);
    if (count_letters(file_name, counts)) {
        return -1;
    }
    if (write(out_fd, counts, sizeof(int) * ALPHABET_LEN) == -1) {
        perror("write");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        // No files to consume, return immediately
        return 0;
    }

    // TODO Create a pipe for child processes to write their results
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return -1;
    }
    // TODO Fork a child to analyze each specified file (names are argv[1], argv[2], ...)
    for (int i = 1; i < argc; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            return -1;
        }
        if (pid == 0) {
            close(pipe_fds[0]);
            if (process_file(argv[i], pipe_fds[1]) == -1) {
                close(pipe_fds[1]);
                exit(EXIT_FAILURE);
            }
            close(pipe_fds[1]);
            exit(EXIT_SUCCESS);
        }
    }
    close(pipe_fds[1]);

    // TODO Aggregate all the results together by reading from the pipe in the parent
    int totals[ALPHABET_LEN] = {0};
    int buffer[ALPHABET_LEN];
    int bytes_read;
    while ((bytes_read = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
        for (int i = 0; i < ALPHABET_LEN; i++) {
            totals[i] += buffer[i];
        }
    }

    if (bytes_read == -1) {
        perror("read");
        close(pipe_fds[0]);
        return -1;
    }

    close(pipe_fds[0]);
    // TODO Change this code to print out the total count of words of each length
    for (int i = 0; i < ALPHABET_LEN; i++) {
        printf("%c Count: %d\n", 'a' + i, totals[i]);
    }

    int ret_val = 0;
    for (int i = 1; i < argc; i++) {
        if (wait(NULL) == -1) {
            perror("wait");
            ret_val = -1;
        }
    }

    return ret_val;
}
