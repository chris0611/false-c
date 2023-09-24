#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "cmdline.h"

bool read_cmdline_opts(false_program *prog, int argc, char *argv[]) {
    int opt;

    // parse command line options
    while ((opt = getopt(argc, argv, "cSf:o:")) != -1) {
        switch (opt) {
        case 'S':
            prog->opts &= ~OUTPUT_MASK;
            prog->opts |= OUTPUT_ASSEMBLY;
            break;
        case 'c':
            prog->opts &= ~OUTPUT_MASK;
            prog->opts |= OUTPUT_OBJFILE;
            break;
        case 'f':
            prog->opts &= ~READ_STDIN;
            prog->infile = optarg;
            break;
        case 'o':
            prog->outfile = optarg;
            prog->opts |= HAS_OUTFILE_NAME;
            break;
        default:
            fprintf(stderr, "Usage: %s [-c] [-S] [-f infile] [-o outfile]\n", argv[0]);
            return false;
        }
    }
    return true;
}
