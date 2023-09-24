#ifndef FALSE_C_PROG_H
#define FALSE_C_PROG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum false_option : uint64_t {
    OUTPUT_EXECUTABLE = UINT64_C(1) << 0,
    OUTPUT_ASSEMBLY   = UINT64_C(1) << 1,
    OUTPUT_OBJFILE    = UINT64_C(1) << 2,
    OUTPUT_MASK       = UINT64_C(0x7),
    READ_STDIN        = UINT64_C(1) << 3,
    HAS_OUTFILE_NAME  = UINT64_C(1) << 4
} false_option;

typedef struct false_program {
    false_option opts;
    const char *infile; // name of input file
    char *source;       // pointer to source
    int64_t source_len; // length of source input
    char *outfile;      // name of output file
    char *code;         // generated assembly
    int64_t code_len;   // length of the generated assembly
    int64_t last_codegen_len;
    char asm_fname[32];
    char obj_fname[32];
} false_program;

#endif /* FALSE_C_PROG_H */
