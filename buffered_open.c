#include "buffered_open.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

// Function to open a file with buffered I/O
buffered_file_t *buffered_open(const char *pathname, int flags, ...) {
    va_list args;
    mode_t mode = 0;

    // Extract mode argument only if O_CREAT is specified
    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    // Allocate memory for buffered_file_t structure
    buffered_file_t *bf = malloc(sizeof(buffered_file_t));
    if (!bf) {
        perror("Error allocating memory for buffered_file_t");
        return NULL;
    }

    // Allocate memory for write buffer
    bf->write_buffer = malloc(BUFFER_SIZE);
    if (!bf->write_buffer) {
        free(bf);
        perror("Error allocating memory for buffer");
        return NULL;
    }

    // Allocate memory for read buffer
    bf->read_buffer = malloc(BUFFER_SIZE);
    if (!bf->read_buffer) {
        free(bf->write_buffer);
        free(bf);
        perror("Error allocating memory for buffer");
        return NULL;
    }

    // Check if O_PREAPPEND flag is set
    if(flags & O_PREAPPEND){
        bf->preappend = 1;
    } else {
        bf->preappend = 0;
    }

    // Remove O_PREAPPEND flag before calling open
    bf->flags = flags & ~O_PREAPPEND;
    
    // Open the file with the appropriate flags and mode
    if (flags & O_CREAT) {
        bf->fd = open(pathname, bf->flags, mode);
    } else {
        bf->fd = open(pathname, bf->flags);
    }
    
    if (bf->fd == -1) {
        perror("Error opening file");
        free(bf->read_buffer);
        free(bf->write_buffer);
        free(bf);
        return NULL;
    }

    bf->read_buffer_size = BUFFER_SIZE;
    bf->write_buffer_size = BUFFER_SIZE;
    bf->read_buffer_pos = 0;
    bf->write_buffer_pos = 0;

    return bf;
}

// Function to write data to the buffered file
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count) {
    const char *ptr = buf;
    size_t remaining = count;

    while (remaining > 0) {
        size_t space = BUFFER_SIZE - bf->write_buffer_pos;
        size_t to_copy = remaining < space ? remaining : space;

        // Copy data to buffer
        memcpy(bf->write_buffer + bf->write_buffer_pos, ptr, to_copy);
        bf->write_buffer_pos += to_copy;
        ptr += to_copy;
        remaining -= to_copy;

        // Flush buffer if full
        if (bf->write_buffer_pos == BUFFER_SIZE) {
            if (buffered_flush(bf) == -1) {
                return -1;
            }
        }
    }

    return count - remaining;
}

// Function to read data from the buffered file
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count) {
    char *ptr = buf;
    size_t remaining = count;
    size_t fd_size;
    size_t to_copy;

    // Flush the write buffer before reading
    if (buffered_flush(bf) == -1) {
        return -1;
    }
    ptr[0] = '\0';

    // Now read from the file descriptor if more data is needed
    while (remaining > 0) {

        // get the current position of fd
        size_t current_position =lseek(bf->fd, 0, SEEK_CUR);
        if (current_position == -1) {
            perror("Error getting current position in file");
            return -1;
        }
        if(bf->read_buffer_size < remaining){
            // Read data from the file descriptor
            fd_size = read(bf->fd, bf->read_buffer, bf->read_buffer_size);
            bf->read_buffer_pos = 0;
            if (fd_size == -1) {
                perror("Error reading from file");
                return -1;
            } else if (fd_size == 0) {// End of file
                break;
            }
        } else {
            // Read data from the file descriptor
            fd_size = read(bf->fd, bf->read_buffer, remaining);
            bf->read_buffer_pos = 0;
            if (fd_size == -1) {
                perror("Error reading from file");
                return -1;
            } else if (fd_size == 0) {// End of file

                break;
            }
        }

        // Copy data to the read buffer
        to_copy = remaining < fd_size ? remaining : fd_size;
        memcpy(ptr, bf->read_buffer, to_copy);

        // Null-terminate the buffer
        ptr[to_copy] = '\0';
        ptr += to_copy;
        remaining -= to_copy;
        bf->read_buffer_pos += to_copy;

        // set the file position to the current position after what i read
        current_position += to_copy;
        if (lseek(bf->fd, current_position, SEEK_SET) == -1) {
            perror("Error seeking to new position after reading");
            return -1;
        }
    }

    // return the read data number
    return count - remaining;
}

int buffered_flush(buffered_file_t *bf) {
    if(bf->write_buffer_pos > 0) {
        // Handle O_TRUNC flag
        if (bf->flags & O_TRUNC) {
            if (ftruncate(bf->fd, 0) == -1) {
                perror("Error truncating file");
                return -1;
            }
            // Remove the O_TRUNC flag after truncation
            bf->flags &= ~O_TRUNC;
        }

        // Check if O_APPEND flag is set, so the offset has to change to the end of the file
        if (bf->flags & O_APPEND) {
            // Ensure the file pointer is at the end before writing if O_APPEND is set
            if (lseek(bf->fd, 0, SEEK_END) == -1) {
                perror("Error seeking to end of file");
                return -1;
            }
            // Remove the O_APPEND flag after handling
            bf->flags &= ~O_APPEND;
        }

        // If O_PREAPPEND flag is set, handle pre-append logic
        if (bf->preappend == 1) {

            // get the file size
            off_t file_size = lseek(bf->fd, 0, SEEK_END);
            if (file_size == -1) {
                perror("Error seeking to end of file");
                return -1;
            }

            char *temp_buffer = malloc(file_size);
            if (!temp_buffer) {
                perror("Error allocating memory for temporary buffer");
                return -1;
            }

            // set the file position to the start of the file.
            lseek(bf->fd, 0, SEEK_SET);
            // Move the current file content to a temporary buffer
            if (read(bf->fd, temp_buffer, file_size) != file_size) {
                perror("Error reading file content into temporary buffer");
                free(temp_buffer);
                return -1;
            }

            // Reset file position to the beginning
            lseek(bf->fd, 0, SEEK_SET);
            if (ftruncate(bf->fd, 0) == -1) {
                perror("Error truncating file");
                free(temp_buffer);
                return -1;
            }

            // Write new data
            if (write(bf->fd, bf->write_buffer, bf->write_buffer_pos) != bf->write_buffer_pos) {
                perror("Error writing new data to file");
                free(temp_buffer);
                return -1;
            }

            // Save the position after writing new data
            off_t new_data_end = lseek(bf->fd, 0, SEEK_CUR);
            if (new_data_end == -1) {
                perror("Error seeking to current position in file");
                free(temp_buffer);
                return -1;
            }

            // Append the original content
            if (write(bf->fd, temp_buffer, file_size) != file_size) {
                perror("Error appending original content to file");
                free(temp_buffer);
                return -1;
            }

            // Reset position to the end of the new data
            if (lseek(bf->fd, new_data_end, SEEK_SET) == -1) {
                perror("Error resetting position to end of new data");
                free(temp_buffer);
                return -1;
            }

            free(temp_buffer);
            
            // Remove the O_PREAPPEND flag after handling by put 2 to indicated that was preappend
            bf->preappend = 2;
        } else {
            if (bf->preappend == 2) {
                // Save the position of previous data
                off_t previous_data_end = lseek(bf->fd, 0, SEEK_CUR);
                if (previous_data_end == -1) {
                    perror("Error seeking to current position in file");
                    return -1;
                }

                // get the fd size 
                off_t file_size = lseek(bf->fd, 0, SEEK_END);
                if (file_size == -1) {
                    perror("Error seeking to end of file");
                    return -1;
                }

                // Calculate the remaining size after the previous data
                off_t remaining_size = file_size - previous_data_end;

                // Allocate memory for the remaining data
                char *second_buffer = malloc(remaining_size);
                if (!second_buffer) {
                    perror("Error allocating memory for second buffer");
                    return -1;
                }

                // read the remaining data after the new data
                lseek(bf->fd, previous_data_end, SEEK_SET);
                if (read(bf->fd, second_buffer, remaining_size) != remaining_size) {
                    perror("Error reading file content into second buffer");
                    free(second_buffer);
                    return -1;
                }

                // Reset position to the end of the new data
                lseek(bf->fd, previous_data_end, SEEK_SET);

                // Write new data
                ssize_t written = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
                if (written == -1) {
                    perror("Error writing buffer to file");
                    free(second_buffer);
                    return -1;
                }

                // set the file position to the end of the new data
                if (lseek(bf->fd, previous_data_end + written, SEEK_SET) == -1) {
                    perror("Error seeking to new position after writing");
                    free(second_buffer);
                    return -1;
                }

                // Write the remaining data after the new data
                if (write(bf->fd, second_buffer, remaining_size) != remaining_size) {
                    perror("Error writing second buffer to file");
                    free(second_buffer);
                    return -1;
                }
                
                // set the file position to the end of the new data
                if (lseek(bf->fd, previous_data_end + written, SEEK_SET) == -1) {
                    perror("Error seeking to new position after writing");
                    free(second_buffer);
                    return -1;
                }

                free(second_buffer);
            } else {
                // Write the buffer to the file
                ssize_t written = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
                if (written == -1) {
                    perror("Error writing buffer to file");
                    return -1;
                }
            }
        }
        // Reset the buffer position after writing
        bf->write_buffer_pos = 0;
    } else {
        // Check if O_APPEND flag is set, so the offset has to change to the end of the file
        if (bf->flags & O_APPEND) {
            // Ensure the file pointer is at the end before writing if O_APPEND is set
            if (lseek(bf->fd, 0, SEEK_END) == -1) {
                perror("Error seeking to end of file");
                return -1;
            }
            // Remove the O_APPEND flag after handling
            bf->flags &= ~O_APPEND;
        }
    }
    return 0;
}

// Function to close the buffered file
int buffered_close(buffered_file_t *bf) {
    // Check if the file was opened in write or read/write mode before flushing
    if ((bf->flags & O_ACCMODE) != O_RDONLY) {
        // Flush the buffer before closing
        if (buffered_flush(bf) == -1) {
            close(bf->fd);
            free(bf->read_buffer);
            free(bf->write_buffer);
            free(bf);
            return -1;
        }
    }

    // Close the file descriptor
    if (close(bf->fd) == -1) {
        perror("Error closing file");
        free(bf->read_buffer);
        free(bf->write_buffer);
        free(bf);
        return -1;
    }

    // Free allocated memory
    free(bf->write_buffer);
    free(bf->read_buffer);
    free(bf);

    return 0;
}
