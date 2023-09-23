#ifndef FALSE_C_PROG_H
#define FALSE_C_PROG_H

#include <stdbool.h>
#include <stdint.h>

// fixme: convert these to bit-flags
typedef struct compiler_opts {
    bool output_assembly;
    bool output_objfile;
    bool output_executable;
    bool read_stdin;
    bool has_outfile_name;
} compiler_opts;

typedef struct false_program {
    compiler_opts opts;
    const char *infile; // name of input file
    char *source;       // pointer to source
    int64_t source_len; // length of source input
    char *outfile;      // name of output file
    char *code;         // generated assembly
    int64_t code_len;   // length of the generated assembly
    char asm_fname[32];
    char obj_fname[32];
    int64_t last_codegen_len;
} false_program;

#endif /* FALSE_C_PROG_H */
