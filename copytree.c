#include "copytree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>

// Helper function to create directories recursively with default permissions
void create_directories(const char *dir_path) {
    // Create a temporary buffer to hold the directory path
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%s", dir_path);

    // Get the length of the directory path
    size_t path_length = strlen(buffer);

    // Remove trailing slash from the directory path if it exists
    if (buffer[path_length - 1] == '/') {
        buffer[path_length - 1] = 0;
    }

    // Iterate over the directory path
    for (char *ptr = buffer + 1; *ptr; ptr++) {
        // If a slash is found, replace it with a null character
        if (*ptr == '/') {
            *ptr = 0;

            // Try to create the directory up to this point in the path
            if (mkdir(buffer, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
                perror("Error creating directory");
                exit(EXIT_FAILURE);
            }

            // Replace the null character with a slash
            *ptr = '/';
        }
    }

    // Try to create the final directory in the path
    if (mkdir(buffer, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
        perror("Error creating directory");
        exit(EXIT_FAILURE);
    }
}

// Function to copy a file
void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    // Get the status of the source file
    struct stat statbuf;
    if (lstat(src, &statbuf) == -1) {
        perror("lstat failed");
        return;
    }

    // Check if the source file is a symbolic link
    if (S_ISLNK(statbuf.st_mode) && copy_symlinks) {
        char link_des[1024];
        // Read the target of the symbolic link
        ssize_t size = readlink(src, link_des, sizeof(link_des) - 1);
        if (size == -1) {
            perror("readlink failed");
            return;
        }
        link_des[size] = '\0';
        
        // Remove the existing symbolic link if it exists
        if (remove(dest) == -1 && errno != ENOENT) {
            perror("remove failed");
            return;
        }

        // Create a new symbolic link
        if (symlink(link_des, dest) == -1) {
            perror("symlink failed");
            return;
        }
    } else {
        // Open the source file
        int src_file_descriptor = open(src, O_RDONLY);
        if (src_file_descriptor == -1) {
            perror("open source file failed");
            return;
        }

        // Determine the permissions for the destination file
        mode_t permissions_mode = copy_permissions ? statbuf.st_mode : (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        // Open the destination file
        int dest_file_descriptor = open(dest, O_WRONLY | O_CREAT | O_TRUNC, permissions_mode);
        if (dest_file_descriptor == -1) {
            perror("open destination file failed");
            close(src_file_descriptor);
            return;
        }

        // Buffer for file copying
        char buf[8192];
        ssize_t n;
        // Copy the file
        while ((n = read(src_file_descriptor, buf, sizeof(buf))) > 0) {
            if (write(dest_file_descriptor, buf, n) != n) {
                perror("write failed");
                close(src_file_descriptor);
                close(dest_file_descriptor);
                return;
            }
        }

        // Check for read errors
        if (n == -1) {
            perror("read failed");
        }

        // Close the source and destination files
        close(src_file_descriptor);
        close(dest_file_descriptor);

        // Copy the file permissions if required
        if (copy_permissions) {
            if (chmod(dest, statbuf.st_mode) == -1) {
                perror("chmod failed");
            }
        }
    }
}
// Function to recursively copy a directory
void copy_directory(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    // Open the source directory
    DIR *source_dir = opendir(src);
    if (source_dir == NULL) {
        perror("Failed to open source directory");
        return;
    }

    // Create the destination directory
    create_directories(dest);

    // Entry for directory reading
    struct dirent *dir_entry;
    while ((dir_entry = readdir(source_dir)) != NULL) {
        // Skip the current directory and parent directory
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }

        // Prepare the source and destination paths
        char source_path[512];
        char destination_path[512];
        snprintf(source_path, sizeof(source_path), "%s/%s", src, dir_entry->d_name);
        snprintf(destination_path, sizeof(destination_path), "%s/%s", dest, dir_entry->d_name);

        // Get the status of the source path
        struct stat status_buffer;
        if (lstat(source_path, &status_buffer) == -1) {
            perror("Failed to get status of source path");
            continue;
        }

        // Check if the source path is a directory
        if (S_ISDIR(status_buffer.st_mode)) {
            // Recursively copy the directory
            copy_directory(source_path, destination_path, copy_symlinks, copy_permissions);
            // Copy permissions if required
            if (copy_permissions) {
                if (chmod(destination_path, status_buffer.st_mode) == -1) {
                    perror("Failed to copy permissions");
                }
            }
        } else {
            // Copy the file
            copy_file(source_path, destination_path, copy_symlinks, copy_permissions);
        }
    }

    // Close the source directory
    if (closedir(source_dir) == -1) {
        perror("Failed to close source directory");
    }
}
