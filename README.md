# FALSE Compiler

This is a compiler for the [FALSE](https://esolangs.org/wiki/FALSE) language, written in C.
FALSE is a stack-based language, similar to Forth.
Currently only supports x86_64 Linux, with glibc.

## Features

- Compile to assembly, ELF object-file or ELF executable
- Support for inline x86_64 machine code (Inspired by the [original implementation](https://strlen.com/false-language/)'s inline m68k assembler)

## Dependencies

- Somewhat recent version of [nasm](https://nasm.us/index.php) and `ld`
- GNU libc

## Quickstart

Compiling the project is done with the usual cmake commands, from the project root directory:

`cmake -S. -Bbuild && cmake --build build`

Then, to start using the compiler, you can either pipe in the program you want to compile via stdin, or specify a file with `-f <file>`.

Examples:

`echo -n "2 4+." | ./build/false-c` (produces an executable named "false-prog" by default)

`./build/false-c -f program.fls -o program` (produces an executable named "program")

## FALSE syntax

### Literals

- Any number, like `42`, will be pushed onto the stack (Consecutive numbers need to be separated by whitespace)
- Characters: `'c` puts the character code for 'c' onto the stack

### Stack manipulation

- `$` Duplicates the top element
- `%` Drops (deletes) the element on top
- `\` Swaps the top two elements
- `@` Left-rotate top three elements (e.g. 1 2 3 -> 2 3 1)
- `ø` Pick element at the given index (e.g. 1 2 3 0 ø -> 1 2 3 3)

### Arithmetic

- `+` Addition
- `-` Subtraction
- `*` Multiplication
- `/` Division
- `_` Negate (e.g. 3_ -> -3)
- `&` Bitwise AND
- `|` Bitwise OR
- `~` Bitwise NOT

### Comparison

False is 0 and true is -1 (all bits set)

- `>` Greater than
- `=` Equals

### Lambdas and control flow

- `[...]` Defines a lambda, and puts it on the stack
- `!` Executes a lambda
- `?` Conditionally execute lambda `condition[...]?` (checks if second element on stack is non-zero)
- `#` While-loop, takes two lambdas as operands: `[condition][body]#` (also checks for non-zero)

### Variables

- Use `a-z` to put a reference to a one of the 26 variables onto the stack
- `:` Stores next item on stack into variable
- `;` Loads from variable

### Input/Output

- `^` Read a character from stdin (EOF = -1)
- `,` Write a character to stdout
- `"string"` Write string to stdout
- `.` Write top of stack as a decimal integer
- `ß` Flush buffered input/output (does nothing in this implementation)

### Other

- `{...}` Comment
- `` ` `` Compile integer as x86 machine code

## How to use inline x86

My recommendation is to first write an assembly language snippet with the code you want to embed.
Then you can assemble this to a flat binary, and embed 4 and 4 bytes of it at a time.
Note that instructions might not cleanly map into multiples of 4 bytes, so pad with no-ops where necessary.

__Important__: The generated assembly uses `rcx` as a pointer to the stack, and `rbx` as a pointer to the variables, and thus care must be taken to preserve these registers, to avoid corrupting the program.

Small example (using nasm):

```nasm
BITS 64
; snippet for printing "hi" to stdout
xor eax, eax
inc eax         ; set eax to 1 (write syscall)
xor edi, edi
inc edi         ; set edi to 1 (stdout)
mov word [rcx-2], 'hi'
lea rsi, [rcx-2]
mov rdx, 2      ; length 2
push rcx        ; save onto stack as syscalls may clobber rcx
syscall
pop rcx
nop             ; padding
```

Having saved this to a file called `hi.asm`, assemble it to a flat binary with nasm like so: `nasm -fbin hi.asm -o hi.bin`.
We can check the contents of the binary with `xxd`:

```shell
$ xxd hi.bin
00000000: 31c0 ffc0 31ff ffc7 66c7 41fe 6869 488d  1...1...f.A.hiH.
00000010: 71fe ba02 0000 0051 0f05 5990            q......Q..Y.
```

(Here you would want to confirm that the binary is a multiple of 32-bits, and if not, pad with no-ops as mentioned.)

We can now use the included python script `hex2int.py` to convert xxd output to a FALSE inline-x86 statement. Pipe it into the compiler to create an executable like so:

```shell
$ xxd -ps hi.bin | ./hex2int.py | ./build/false-c-port -o hi && ./hi
hi
```

## Why make this?

For fun and profit :^)
