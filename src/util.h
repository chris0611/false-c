#ifndef FALSE_C_UTIL_H
#define FALSE_C_UTIL_H

#include <stdio.h>
#include <stdint.h>

char *map_file(const char *filepath, int64_t *filesize);

void unmap(char *mapped, int64_t size);

void rename_file(const char *old_name, const char *new_name);

void remove_file(const char *filename);

// caller has to free return value
char *read_file_buffered(FILE *source, int64_t *flen);

// caller has to free return value
char *build_filename(const char *prefix, const char *suffix);

// fixme: add note about basename!
/*  caller has to free return value.
    gets the name of the file before the last '.' */
char *find_filename_prefix(const char *filename);

#endif /* FALSE_C_UTIL_H */
