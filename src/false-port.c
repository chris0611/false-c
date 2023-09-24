#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

#include "falseprog.h"
#include "util.h"
#include "cmdline.h"
#include "compile.h"

int main(int argc, char *argv[]) {
    false_program main_prog = {
        .opts = OUTPUT_EXECUTABLE | READ_STDIN
    };

    if (!read_cmdline_opts(&main_prog, argc, argv)) {
        return EXIT_FAILURE;
    }

    if (!(main_prog.opts & READ_STDIN)) {
        main_prog.source = map_file(main_prog.infile, &main_prog.source_len);
    } else {
        main_prog.source = read_file_buffered(stdin, &main_prog.source_len);
    }

    if (main_prog.source == NULL) {
        err_and_die("Failed to read source code");
    }

    compile_false_program(&main_prog);

    char *prefix = NULL;
    if (!(main_prog.opts & HAS_OUTFILE_NAME) && main_prog.infile != NULL) {
        prefix = find_filename_prefix(main_prog.infile);
    }

    switch ((main_prog.opts & OUTPUT_MASK)) {
    case OUTPUT_EXECUTABLE:
        if (!(main_prog.opts & HAS_OUTFILE_NAME))
            main_prog.outfile = (prefix) ? build_filename(prefix, "") : "false-prog";
        assemble_false_program(&main_prog);
        link_false_program(&main_prog);
        break;
    case OUTPUT_OBJFILE:
        if (!(main_prog.opts & HAS_OUTFILE_NAME))
            main_prog.outfile = (prefix) ? build_filename(prefix, "o") : "false-prog.o";
        assemble_false_program(&main_prog);
        rename_file(main_prog.obj_fname, main_prog.outfile);
        break;
    case OUTPUT_ASSEMBLY:
        if (!(main_prog.opts & HAS_OUTFILE_NAME))
            main_prog.outfile = (prefix) ? build_filename(prefix, "asm") : "false-prog.asm";
        rename_file(main_prog.asm_fname, main_prog.outfile);
        break;
    default:
        __builtin_unreachable();
        break;
    }

    // cleanup
    if (main_prog.opts & READ_STDIN) {
        free(main_prog.source);
    } else {
        unmap(main_prog.source, main_prog.source_len);
        if (!(main_prog.opts & HAS_OUTFILE_NAME)) {
            free(main_prog.outfile);
        }
        if (prefix != NULL) {
            free(prefix);
        }
    }

    return EXIT_SUCCESS;
}
