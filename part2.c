#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

// Function to write a message to stdout a specified number of times with random delays
void write_message(const char *message, int count) {
    for (int i = 0; i < count; i++) {
        printf("%s\n", message);

        // Random delay between 0 and 99 milliseconds and handle error
        if (usleep((rand() % 100) * 1000) < 0) {
            perror("usleep");
            exit(1);
        }
    }
}

// Function to acquire a lock
void acquire_lock() {
    while (open("lockfile.lock", O_CREAT | O_EXCL, 0644) < 0) {
        
        // Wait and retry and handle error
        if (usleep(1000) < 0) {
            perror("usleep");
            exit(1);
        }
    }
}

// Function to release a lock
void release_lock() {
    if (remove("lockfile.lock") < 0) {
        perror("remove");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc <= 4) {
        fprintf(stderr, "Usage: %s <message1> <message2> ... <count>\n", argv[0]);
        return 1;
    }

    // The last argument is the count
    int count = atoi(argv[argc - 1]);

    // The number of messages is the number of arguments minus 2
    int num_messages = argc - 2;

    for (int i = 0; i < num_messages; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        } else if (pid == 0) {
            // Acquire the lock before writing
            acquire_lock();
            write_message(argv[i + 1], count);

            // Release the lock after writing
            release_lock();
            exit(0);
        }
        // Wait for the current child process to finish before continuing
        if (wait(NULL) < 0) {
            perror("wait");
            return 1;
        }
    }

    return 0;
}
