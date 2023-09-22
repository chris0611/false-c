#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "falseprog.h"
#include "compile.h"

#define CODEGEN_SIZE 65536

static const char *codegen_prolouge =
    "section .text\n"
    "global _start\n"
    "_start:\n"
    "push rbp\n"
    "mov rbp, rsp\n"
    "sub rsp, 26\n"     // allocate space for variables on stack
    "mov rbx, rsp\n"
    "mov rax, 9\n"      // mmap syscall number
    "mov rdi, 0\n"      // mapping dest (OS chooses)
    "mov rsi, 65536\n"  // size of mapping
    "mov rdx, 3\n"      // PROT_READ | PROT_WRITE
    "mov r10, 0x22\n"   // MAP_PRIVATE | MAP_ANONYMOUS
    "mov r8, -1\n"      // fd (-1)
    "xor r9d, r9d\n"    // offset
    "syscall\n"         // let the kernel do its thing
    "test rax, rax\n"   // check for errors
    "js false_exit\n"
    "mov rcx, rax\n"
    "add rcx, 65536\n"
    "false_main:\n";

static const char *codegen_epilouge =
    "false_exit:\n"
    "add rsp, 26\n"
    "mov eax, 60\n"
    "xor edi, edi\n"
    "syscall\n\n";

static const char *codegen_int_to_str =
    "int_to_str:\n"         // Assumes number to convert is in [rcx]
    "mov r12d, [rcx]\n"
    "mov edi, r12d\n"
    "neg edi\n"             // Check if negative number
    "cmovs edi, r12d\n"
    "add rcx, 4\n"
    "sub rsp, 12\n"         // Make buffer on stack
    "mov byte [rsp+11], 0\n"
    "lea rax, [rsp+10]\n"
    "mov r8d, 3435973837\n" // constant to avoid division
    "lea r9, [rel .L.str]\n"
    ".loop:\n"              // loop
    "mov r10d, edi\n"
    "imul r10, r8\n"
    "shr r10, 35\n"
    "lea r11d, [r10 + r10]\n"
    "lea r11d, [r11+4*r11]\n"
    "mov edx, edi\n"
    "sub edx, r11d\n"
    "movzx r11d, byte [rdx + r9]\n"
    "mov byte [rax], r11b\n"
    "dec rax\n"
    "cmp edi, 9\n"
    "mov edi, r10d\n"
    "ja .loop\n"
    "test r12d, r12d\n"
    "js .append_minus\n"
    "inc rax\n"
    "jmp .exit\n"
    ".append_minus:\n"
    "mov byte [rax], 45\n"
    ".exit:\n"
    "mov rsi, rsp\n"
    "sub rsi, rax\n"
    "add rsi, 11\n"
    "mov rdx, rax\n"    // rdx contains pointer to string
    "mov rax, rsi\n"    // rax contains number of bytes in string
    "add rsp, 12\n"
    "ret\n"
    "section .rodata\n"
    ".L.str: db '0123456789',0\n";

typedef struct symbol_codegen {
    int64_t bytes;
    const char *code;
} symbol_codegen;

/*
    Conventions:
    rbx = pointer to variables
    rcx = pointer to stack
*/

#define CODEGEN_STR(str) {sizeof(str), str}

static const symbol_codegen codegen_lookup[256] = {
    ['+'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "add eax, [rcx+4]\n"
        "add rcx, 4\n"
        "mov [rcx], eax\n"),
    ['-'] = CODEGEN_STR(
        "mov eax, [rcx+4]\n"
        "sub eax, [rcx]\n"
        "add rcx, 4\n"
        "mov [rcx], eax\n"),
    ['*'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "imul dword [rcx+4]\n"
        "add rcx, 4\n"
        "mov [rcx], eax\n"
    ),
    ['/'] = CODEGEN_STR(
        "mov eax, [rcx+4]\n"
        "cdq\n"
        "idiv dword [rcx]\n"
        "add rcx, 4\n"
        "mov [rcx], eax\n"
    ),
    // Print integer
    ['.'] = CODEGEN_STR(
        "call int_to_str\n"
        "mov rsi, rdx\n"    // pointer
        "mov edx, eax\n"    // length
        "mov eax, 1\n"
        "mov edi, 1\n"      // stdout
        "push rcx\n"
        "syscall\n"
        "pop rcx\n"
    ),
    // Execute lambda
    ['!'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "add rcx, 4\n"
        "call rax\n"
    ),
    // Store into variable
    [':'] = CODEGEN_STR(
        "mov eax, [rcx]\n"      // variable
        "mov edx, [rcx+4]\n"    // value
        "mov dword [rbx+rax], edx\n"
        "add rcx, 8\n"
    ),
    // Load from variable
    [';'] = CODEGEN_STR(
        "mov eax, [rcx]\n"  // variable
        "mov edx, dword [rbx+rax]\n"
        "mov dword [rcx], edx\n"
    ),
    // Compare equal
    ['='] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "cmp [rcx+4], eax\n"
        "sete al\n"
        "add rcx, 4\n"
        "movzx eax, al\n"
        "neg eax\n"
        "mov [rcx], eax\n"
    ),
    // Conditionally execute
    ['?'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "mov rdx, [rcx+4]\n"
        "add ecx, 2\n"
        "test eax, eax\n"
        "je .f\n"
        "call rdx\n"
        ".f:\n"
    ),
    // Duplicate top of stack
    ['$'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "sub rcx, 4\n"
        "mov [rcx], eax\n"
    ),
    // Drop top of stack
    ['%'] = CODEGEN_STR(
        "add rcx, 4\n"
    ),
    // Swap top two elements
    ['\\'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "mov edx, [rcx+4]\n"
        "mov [rcx], edx\n"
        "mov [rcx+1], eax\n"
    ),
    // Rotate top three elements
    ['@'] = CODEGEN_STR(
        "vmovd xmm0, [rcx+8]\n"
        "mov eax, [rcx+4]\n"
        "vpinsrd xmm0, xmm0, [rcx], 1\n"
        "mov [rcx+8], eax\n"
        "vmovq [rcx], xmm0\n"
    ),
    // Negate
    ['_'] = CODEGEN_STR(
        "neg dword [rcx]\n"
    ),
    // Bitwise and
    ['&'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "and eax, [rcx+4]\n"
        "add ecx, 4\n"
        "mov [rcx], eax\n"
    ),
    // Bitwise or
    ['|'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "or eax, [rcx+4]\n"
        "add ecx, 4\n"
        "mov [rcx], eax\n"
    ),
    // Bitwise not
    ['~']  = CODEGEN_STR(
        "not dword [rcx]\n"
    ),
    // Compare greater than
    // TODO: Check that comparison is done in right order...
    ['>'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "cmp [rcx+4], eax\n"
        "setg al\n"
        "add ecx, 4\n"
        "movzx eax, al\n"
        "neg eax\n"
        "mov [rcx], eax\n"
    ),
    // While loop (takes two lambdas as operands)
    ['#'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "mov edx, [rcx+4]\n"
        "add ecx, 8\n"
        "push rdx\n"
        "push rax\n"
        ".s:\n"
        "mov eax, [esp+4]\n" // fixme: this is probably wrong
        "call rax\n"
        "test eax, eax\n"
        "je .e\n"
        "mov rax, [esp]\n"
        "call rax\n"
        "jmp .s\n"
        ".e:\n"
        "add esp, 8\n"
    ),
    // putchar
    [','] = CODEGEN_STR(
        "mov eax, 1\n"
        "mov edi, 1\n"  // stdout
        "mov rsi, rcx\n"
        "mov rdx, 1\n"
        "push rcx\n"
        "syscall\n"
        "pop rcx\n"
        "add rcx, 4\n"
    ),
    // getchar
    ['^'] = CODEGEN_STR(
        "sub rcx, 4\n"
        "xor eax, eax\n"
        "xor edi, edi\n"  // stdin
        "mov rsi, rcx\n"
        "mov rdx, 1\n"
        "push rcx\n"
        "syscall\n"
        "pop rcx\n"
    )
};

#undef CODEGEN_STR

static void append_code(false_program *restrict prog, const char *code,
                        int64_t code_len) {
    if (prog->code_len + code_len > CODEGEN_SIZE) {
        fprintf(stderr, "Error: Maximum size for generated code reached\n");
        fprintf(stderr, "Quitting...\n");
        abort();
    }
    memcpy(&prog->code[prog->code_len], code, code_len);
    prog->code_len += code_len;
}

static const char *number_codegen_fmt =
    "sub rcx, 4\n"
    "mov dword [rcx], %d\n";

static void number_codegen(false_program *prog, const int num) {
    char buffer[64] = {0};
    const int64_t len = snprintf(buffer, 64, number_codegen_fmt, num);
    append_code(prog, buffer, len);
}

static int parse_number(const char *src, const int64_t src_len, int64_t *idx) {
    char buffer[32] = {0};
    int64_t bufidx = 0;
    char ch = src[*idx];

    do {
        buffer[bufidx++] = ch;
        if (++(*idx) == src_len) break;
        if (bufidx == 31) break;
    } while (isdigit(ch = src[*idx]));

    (*idx)--; // since loop counter will increase by one
    return strtol(buffer, NULL, 10);
}

static int64_t string_constants = 0;

static const char *string_codegen_fmt =
    "jmp .print_str%ld\n"
    "section .rodata\n"
    ".L.str%ld: "
    "db '%s'\n"
    "section .text\n"
    ".print_str%ld:\n"
    "xor eax, eax\n"
    "inc eax\n"
    "xor edi, edi\n"
    "inc edi\n"
    "lea rsi, [rel .L.str%ld]\n"
    "mov rdx, %ld\n"
    "push rcx\n"
    "syscall\n"
    "pop rcx\n";

static void string_codegen(false_program *prog, char *str_start, char *str_end) {
    const int64_t len = (intptr_t)str_end - (intptr_t)str_start;
    // fixme: arbitrary sizes. Consider using dynamic allocation?
    char str_buf[512] = {0};
    char fmt_buf[1024] = {0};

    assert(len != 511 && "String constant too long!\n");
    memcpy(str_buf, str_start, len);

    int64_t snum = string_constants++;
    int64_t written = snprintf(fmt_buf, 1024, string_codegen_fmt, snum, snum,
                               str_buf, snum, snum, len);
    append_code(prog, fmt_buf, written);
}

void compile_false_program(false_program *prog) {
    char *code = mmap(NULL, CODEGEN_SIZE, PROT_READ |
                      PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
    assert(code != MAP_FAILED && "mmap() failed!");

    prog->code = code;
    prog->code_len = 0;
    append_code(prog, codegen_prolouge, strlen(codegen_prolouge));

    // main loop
    for (int64_t i = 0; i < prog->source_len; i++) {
        const char ch = prog->source[i];

        const symbol_codegen *gen = &codegen_lookup[(uint8_t)ch];
        if (gen->code != NULL) {
            append_code(prog, gen->code, gen->bytes-1);
            continue;
        }

        // If we got here, the symbol is either an
        // alphanumeric character or some unknown symbol

        if (ch >= '0' && ch <= '9') {
            // Pass i as param because number might be several characters long
            const int num = parse_number(prog->source, prog->source_len, &i);
            number_codegen(prog, num);
            continue;
        }

        // Variable reference
        if (ch >= 'a' && ch <= 'z') {
            number_codegen(prog, (ch-'a')*4);
            continue;
        }

        // check for ø and ß

        // skip comments
        if (ch == '{') {
            char sym;
            int64_t idx = i;
            do {
                idx++;
                if (idx == prog->source_len) {
                    fprintf(stderr, "Error: No closing '}'\nQuitting...\n");
                    abort();
                }
            } while ((sym = prog->source[idx]) != '}');
            continue;
        }

        // handle string constants
        if (ch == '"') {
            char sym;
            int64_t idx = i;
            char *str_start = &prog->source[idx + 1];
            do {
                idx++;

                if (idx == prog->source_len) {
                    fprintf(stderr, "Error: No matching '\"'\nQuitting...\n");
                    abort();
                }
            } while ((sym = prog->source[idx]) != '"');

            string_codegen(prog, str_start, &prog->source[idx]);
            i = idx;
            continue;
        }
    }

    append_code(prog, codegen_epilouge, strlen(codegen_epilouge));
    append_code(prog, codegen_int_to_str, strlen(codegen_int_to_str));

    strcpy(prog->asm_fname, "false-tmpXXXXXX.asm");
    int asmfd = mkstemps(prog->asm_fname, 4);
    assert(asmfd != -1 && "Couldn't make a temporary file! :(");
    int res = write(asmfd, prog->code, prog->code_len);
    if (res < 0) {
        fprintf(stderr, "Error: Failed to write to file: %s\n", strerror(errno));
    }
    close(asmfd);
}

void assemble_false_program(false_program *prog) {
    // fixme: generate name based on input program name
    strcpy(prog->obj_fname, "false-tmpXXXXXX.o");
    int objfd = mkstemps(prog->obj_fname, 2);
    assert(objfd != -1 && "Couldn't make a temporary file! :(");

    pid_t cpid = vfork();
    if (cpid < 0) {
        fprintf(stderr, "Error: vfork() failed\nQuitting...\n");
        abort();
    }

    if (cpid == 0) {
        // we are the child
        execlp("nasm", "nasm", prog->asm_fname, "-o", prog->obj_fname,
            "-f", "elf64", NULL);
        _exit(EXIT_FAILURE);
    } else {
        int wstatus;
        wait(&wstatus);
        if (WIFEXITED(wstatus) && (WEXITSTATUS(wstatus) == EXIT_FAILURE)) {
            fprintf(stderr, "Error: Failed to invoke nasm\nQuitting...\n");
            close(objfd);
            unlink(prog->asm_fname);
            unlink(prog->obj_fname);
            abort();
        }

        close(objfd);
        // don't need assembly file anymore
        unlink(prog->asm_fname);
    }
}

void link_false_program(false_program *prog) {
    pid_t cpid = vfork();
    if (cpid < 0) {
        fprintf(stderr, "Error: vfork() failed\nQuitting...\n");
        abort();
    }

    if (cpid == 0) {
        // we are the child
        execlp("ld", "ld", prog->obj_fname, "-o",
            (prog->outfile != NULL) ? prog->outfile : "false_prog", NULL);
        _exit(EXIT_FAILURE);
    } else {
        int wstatus;
        wait(&wstatus);
        if (WIFEXITED(wstatus) && (WEXITSTATUS(wstatus) == EXIT_FAILURE)) {
            fprintf(stderr, "Error: Failed to invoke linker\nQuitting...\n");
            unlink(prog->obj_fname);
            abort();
        }
        // don't need object file anymore
        unlink(prog->obj_fname);
    }
}
