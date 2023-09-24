# FALSE Compiler

This is a compiler for the [FALSE](https://esolangs.org/wiki/FALSE) language, written in C.
Currently only compiles for x64 Linux, with glibc.

## Features

- Compile to assembly, ELF object-file or ELF executable.
- Support for inline x86_64 machine code (Inspired by the [original implementation](https://strlen.com/false-language/)'s inline m68k assembler)

## Dependencies

- Somewhat recent version of [nasm](https://nasm.us/index.php) and `ld`.

## Quickstart

How to compile the program, from the project root directory:
`cmake -S. -Bbuild && cmake --build build`
Then, you can either pipe in the program you want to compile, or specify a file with `-f <file>`.

Examples:
`echo -n "2 4+." | ./build/false-c-port` (generates an executable named "false-prog" by default)

`./build/false-c-port -f program.fls -o program` (generates an executable named "program")

## How to use inline machine code

My recommendation is to assemble the code you want with your favorite assembler, and get a hexdump of the output.
Then, you can convert 4 and 4 bytes of those into decimal. This will be your input.
Note that instructions might not cleanly map into multiples of 4 bytes, so pad with no-ops where necessary.

## Why?

For fun and profit :^)
