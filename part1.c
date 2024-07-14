#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Function to write a message to a file a specified number of times
void write_to_file(FILE *file, const char *message, int count) {
    for (int i = 0; i < count; i++) {
        fprintf(file, "%s", message);
    }
    fflush(file); // Ensure all output is written to the file
}

int main(int argc, char *argv[]) {
    // Check if the correct number of arguments are provided
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <parent_message> <child1_message> <child2_message> <count>\n", argv[0]);
        return 1;
    }

    // Get the messages and the count from the command-line arguments
    const char *parent_message = argv[1];
    const char *child1_message = argv[2];
    const char *child2_message = argv[3];
    int count = atoi(argv[4]);
    

    // Fork the first child process
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork1");
        return 1;
    } else if (pid1 == 0) {
        // In the first child process, open the file, write the message, and then exit
        FILE *file = fopen("output.txt", "a");
        if (!file) {
            perror("fopen");
            exit(1);
        }
        write_to_file(file, child1_message, count);
        fclose(file);
        exit(0);
    }

    // Fork the second child process
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork2");
        return 1;
    } else if (pid2 == 0) {        
        // In the second child process, open the file, write the message, and then exit
        waitpid(pid1, NULL, 0); // Wait for the first child to finish
        sleep(10);

        FILE *file = fopen("output.txt", "a");
        if (!file) {
            perror("fopen");
            exit(1);
        }
        write_to_file(file, child2_message, count);
        fclose(file);
        exit(0);
    }

    // In the parent process, wait for both child processes to finish
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    // After both child processes have finished, open the file and write the parent's message
    FILE *file = fopen("output.txt", "a");
    if (!file) {
        perror("fopen");
        return 1;
    }
    write_to_file(file, parent_message, count);
    fclose(file);

    return 0;
}
