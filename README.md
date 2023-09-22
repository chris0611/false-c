# FALSE Compiler

This is a compiler for the [FALSE](https://esolangs.org/wiki/FALSE) language, written in C.
Currently  only compiles for x64 Linux, with glibc.

## Features

- Outputs assembly, ELF object or ELF executable.
- Support for inline x64 machine code (the original implementation had inline m68k assembler)

## Dependencies

- Somewhat recent version of `nasm` and `gcc`
- [Capstone](http://www.capstone-engine.org/) disassembly framework/library

## Quickstart

From the project root directory:
`cmake -S. -Bbuild && cmake --build build`
