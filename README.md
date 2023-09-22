# FALSE Compiler

This is a compiler for the [FALSE](https://esolangs.org/wiki/FALSE) language, written in C.
Currently only compiles for x64 Linux, with glibc.

## Features

- Compile to assembly, ELF object-file or ELF executable.
- Support for inline x64 machine code (Inspired by the [original implementation](https://strlen.com/false-language/)'s inline m68k assembler)

## Dependencies

- Somewhat recent version of `nasm`, `gcc` and `ld`.
- [Capstone](http://www.capstone-engine.org/) disassembly framework/library

## Quickstart

From the project root directory:
`cmake -S. -Bbuild && cmake --build build`
