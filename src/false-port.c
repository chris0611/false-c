#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "disassemble.h"
#include "falseprog.h"
#include "util.h"
#include "cmdline.h"
#include "compile.h"

#define M68K_CODE "\x22\x08\x4e\xae\xfc\x4c"

int main(int argc, char *argv[]) {
    false_program main_prog = {
        .opts = { false, false, true, true, false },
        .infile = NULL,
        .outfile = NULL,
        .code = NULL,
        .code_len = 0,
        .source = NULL,
        .source_len = 0,
    };

    if (!read_cmdline_opts(&main_prog, argc, argv)) {
        return EXIT_FAILURE;
    }

    machinecode_to_assembly((const uint8_t*)M68K_CODE, sizeof(M68K_CODE)-1);

    if (!main_prog.opts.read_stdin) {
        main_prog.source = map_file(main_prog.infile, &main_prog.source_len);
    } else {
        main_prog.source = read_file_buffered(stdin, &main_prog.source_len);
    }

    if (main_prog.source == NULL) {
        fprintf(stderr, "Fatal error: Quitting...\n");
        return EXIT_FAILURE;
    }

    compile_false_program(&main_prog);

    if (main_prog.opts.output_objfile || main_prog.opts.output_executable) {
        assemble_false_program(&main_prog);
    }

    char *prefix = NULL;
    if (!main_prog.opts.has_outfile_name && main_prog.infile != NULL) {
        prefix = find_filename_prefix(main_prog.infile);
    }

    if (main_prog.opts.output_executable) {
        if (prefix != NULL) {
            main_prog.outfile = build_filename(prefix, "");
        } else if (main_prog.outfile == NULL) {
            main_prog.outfile = "false-prog";
        }

        link_false_program(&main_prog);
    } else if (main_prog.opts.output_objfile) {
        if (prefix != NULL) {
            main_prog.outfile = build_filename(prefix, "o");
        } else if (main_prog.outfile == NULL) {
            main_prog.outfile = "false-prog.o";
        }

        rename_file(main_prog.obj_fname, main_prog.outfile);
    } else if (main_prog.opts.output_assembly) {
        if (prefix != NULL) {
            main_prog.outfile = build_filename(prefix, "asm");
        } else if (main_prog.outfile == NULL) {
            main_prog.outfile = "false-prog.asm";
        }
        rename_file(main_prog.asm_fname, main_prog.outfile);
    }

    // cleanup
    if (main_prog.opts.read_stdin) {
        free(main_prog.source);
    } else {
        unmap(main_prog.source, main_prog.source_len);
        if (!main_prog.opts.has_outfile_name) {
            free(main_prog.outfile);
        }
        if (prefix != NULL) {
            free(prefix);
        }
    }

    return EXIT_SUCCESS;
}
