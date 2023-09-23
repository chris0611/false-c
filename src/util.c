#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "util.h"

#define READBUF_SIZE 4096

char *map_file(const char *filepath, int64_t *filesize) {
    const int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        return NULL;
    }

    struct stat file_stats;
    int success = fstat(fd, &file_stats);
    if (success == -1) {
        fprintf(stderr, "Error getting file stats: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }

    char *file_bytes = mmap(NULL, (size_t)file_stats.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (file_bytes == MAP_FAILED) {
        fprintf(stderr, "Error memory-mapping file: %s\n", strerror(errno));
        file_bytes = NULL;
    }

    *filesize = file_stats.st_size;
    close(fd);
    return file_bytes;
}

void unmap(char *mapped, int64_t size) {
    munmap(mapped, size);
}

void rename_file(const char *old_name, const char *new_name) {
    rename(old_name, new_name);
}

void remove_file(const char *filename) {
    unlink(filename);
}

char *read_file_buffered(FILE *source_file, int64_t *flen) {
    char *source = NULL;
    int64_t source_len = 0;
    char buffer[READBUF_SIZE];
    int64_t buf_len = 0;

    char ch;
    while ((ch = fgetc(source_file)) != EOF) {
        if (buf_len == READBUF_SIZE) {
            if (source == NULL) {
                source = malloc(buf_len);
                assert(source != NULL && "Allocation Failed!");
                memcpy(source, buffer, buf_len);
            } else {
                char *tmp = realloc(source, source_len + buf_len);
                assert(tmp != NULL && "Allocation Failed!");
                source = tmp;
                memcpy(&source[source_len], buffer, buf_len);
            }
            buf_len = 0;
            source_len += READBUF_SIZE;
        }

        buffer[buf_len++] = ch;
    }

    // copy what's left of buffer
    if (buf_len > 0) {
        if (source == NULL) {
            source = malloc(buf_len);
            assert(source != NULL && "Allocation Failed!");
        } else {
            source = realloc(source, source_len + buf_len);
            assert(source != NULL && "Allocation Failed!");
        }
        memcpy(&source[source_len], buffer, buf_len);
        source_len += buf_len;
    }

    *flen = source_len;
    return source;
}

char *build_filename(const char *prefix, const char *suffix) {
    assert((prefix != NULL && suffix != NULL) && "Arguments can't be NULL!");

    const size_t len = strlen(prefix) + strlen(suffix);
    char *filename = calloc(len + 2, sizeof(char));
    assert(filename != NULL && "the OS wouldn't give us any memory =(");

    snprintf(filename, len + 2, "%s.%s", prefix, suffix);
    return filename;
}

char *find_filename_prefix(const char *filename) {
    size_t prefix_length = 0;
    size_t tmp_length = 0;

    char *base = basename(filename);
    const char *offset = base;

    while ((tmp_length = strcspn(offset, ".")) != 0) {
        prefix_length += tmp_length;
        offset += tmp_length;
    }

    char *prefix = calloc(prefix_length + 1, sizeof(char));
    assert(prefix != NULL && "the OS wouldn't give us any memory =(");

    memcpy(prefix, base, prefix_length);
    return prefix;
}

#define ANSI_RED    "\033[0;31m"
#define ANSI_RESET  "\033[0m"

[[noreturn]]
void err_and_die(const char *errmsg) {
    fprintf(stderr, ANSI_RED "Error: " ANSI_RESET " %s\n", errmsg);
    exit(EXIT_FAILURE);
}
