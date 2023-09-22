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
            prog->opts.output_assembly = true;
            prog->opts.output_executable = false;
            prog->opts.output_objfile = false;
            break;
        case 'c':
            prog->opts.output_assembly = false;
            prog->opts.output_objfile = true;
            prog->opts.output_executable = false;
            break;
        case 'f':
            prog->opts.read_stdin = false;
            prog->infile = optarg;
            break;
        case 'o':
            prog->outfile = optarg;
            prog->opts.has_outfile_name = true;
            break;
        default:
            fprintf(stderr, "Usage: %s [-c] [-S] [-f infile] [-o outfile]\n", argv[0]);
            return false;
        }
    }
    return true;
}
