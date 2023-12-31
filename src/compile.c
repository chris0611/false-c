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
#include <endian.h>

#include "falseprog.h"
#include "compile.h"
#include "util.h"

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
    // fixme: somehow make sure that addresses are < UINT32_MAX, since we just load 32-bits
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
        "mov eax, [rcx]\n"      // lambda
        "mov edx, [rcx+4]\n"    // condition
        "add rcx, 8\n"          // pop stack
        "test edx, edx\n"       // check for zero
        "je short 3\n"          // if zero, don't exec lambda
        "call rax\n"            // if not, exec
        //".f:\n"
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
        "mov [rcx+4], eax\n"
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
        "add rcx, 4\n"
        "mov [rcx], eax\n"
    ),
    // Bitwise or
    ['|'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "or eax, [rcx+4]\n"
        "add rcx, 4\n"
        "mov [rcx], eax\n"
    ),
    // Bitwise not
    ['~']  = CODEGEN_STR(
        "not dword [rcx]\n"
    ),
    // Compare greater than
    ['>'] = CODEGEN_STR(
        "mov eax, [rcx]\n"
        "cmp [rcx+4], eax\n"
        "setg al\n"
        "add rcx, 4\n"
        "movzx eax, al\n"
        "neg eax\n"
        "mov [rcx], eax\n"
    ),
    // While loop (takes two lambdas as operands)
    ['#'] = CODEGEN_STR(
        "mov eax, [rcx]\n"      // lambda (while-loop body)
        "mov edx, [rcx+4]\n"    // lambda (conditional)
        "add rcx, 8\n"          // pop off calc-stack
        "push rax\n"            // push onto (normal) stack
        "push rdx\n"
        // loop start
        "mov eax, [rsp]\n"      // get conditional lambda
        "call rax\n"
        "mov eax, [rcx]\n"      // result from stack
        "add rcx, 4\n"          // pop it
        "test eax, eax\n"
        "je short 10\n"         // exit loop
        "mov rax, [rsp+8]\n"
        "call rax\n"            // execute "loop body"
        "jmp short -23\n"       // start loop again
        // loop exit
        "add rsp, 16\n"         // pop lambda addrs from stack
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
        "mov [rcx], eax\n"
        "xor edi, edi\n"  // stdin
        "mov rsi, rcx\n"
        "mov rdx, 1\n"
        "push rcx\n"
        "syscall\n"
        "pop rcx\n"
        "mov eax, [rcx]\n"
        "test eax, eax\n"
        "jnz short 3\n"
        "not dword [rcx]\n"
    ),
    // Pick from stack (0xF8 is the ISO 8859-1 encoding for 'ø')
    [(uint8_t)'\xF8'] = CODEGEN_STR(
        "movsxd rax, dword [rcx]\n"
        "mov eax, dword [rcx + 4*rax + 4]\n"
        "mov dword [rcx], eax\n"
    )
};

#undef CODEGEN_STR

static void append_code(false_program *restrict prog, const char *code,
                        int64_t code_len) {
    if (prog->code_len + code_len > CODEGEN_SIZE) {
        err_and_die("Maximum code size reached\n");
    }
    memcpy(&prog->code[prog->code_len], code, code_len);
    prog->code_len += code_len;
    prog->last_codegen_len = code_len;
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

static const char *string_codegen_fmt =
    "jmp .print_str%ld\n"
    "section .rodata\n"
    ".L.str%ld: "
    "db `%s`\n"
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
    static int64_t string_constants = 0;
    const int64_t len = (intptr_t)str_end - (intptr_t)str_start;
    // fixme: arbitrary sizes. Consider using dynamic allocation?
    char str_buf[512] = {0};
    char fmt_buf[1024] = {0};

    // fixme: check length
    int64_t buf_idx = 0;
    for (int64_t i = 0; i < len; i++) {
        if (str_start[i] != '\n') {
            str_buf[buf_idx++] = str_start[i];
            continue;
        }
        str_buf[buf_idx++] = '\\';
        str_buf[buf_idx++] = 'n';
    }

    int64_t snum = string_constants++;
    int64_t written = snprintf(fmt_buf, 1024, string_codegen_fmt, snum, snum,
                               str_buf, snum, snum, len);
    append_code(prog, fmt_buf, written);
}

static const char *inline_x86_codegen_fmt =
    "dd 0x%08x ; INLINE ASSEMBLER\n";

// fixme: this is cursed?
static void inline_assembler_codegen(false_program *prog) {
    char *current_pos = &prog->code[prog->code_len];

    // find last number pushed onto stack
    char ch;
    int64_t offset = 0;
    while ((ch = current_pos[offset]) != ' ') {
        offset--;
    }

    // get number
    const int32_t num = strtol(&current_pos[offset], NULL, 10);
    char buffer[256] = {0};

    const int64_t written = snprintf(buffer, 256, inline_x86_codegen_fmt, be32toh(num));

    prog->code_len -= prog->last_codegen_len;
    append_code(prog, buffer, written);
}

static const char *lambda_entry_codegen_fmt =
    "lea rax, [rel .lambda%1$d]\n"
    "sub rcx, 4\n"
    "mov [rcx], eax\n"      // this is not ideal, the address of a lambda cannot be > UINT_MAX
    "jmp .lambda_end%1$d\n" // (a solution could be to store an index into an array of addresses instead)
    ".lambda%1$d:\n";

static const char *lambda_exit_codegen_fmt =
    "ret\n"
    ".lambda_end%d:\n";


static void lambda_entry_codegen(false_program *prog, int lambda) {
    char buf[128] = {0};
    const int64_t written = snprintf(buf, 128, lambda_entry_codegen_fmt, lambda);
    append_code(prog, buf, written);
}

static void lambda_exit_codegen(false_program *prog, int lambda) {
    char buf[128] = {0};
    const int64_t written = snprintf(buf, 128, lambda_exit_codegen_fmt, lambda);
    append_code(prog, buf, written);
}

void compile_false_program(false_program *prog) {
    char *code = mmap(NULL, CODEGEN_SIZE, PROT_READ |
                      PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
    assert(code != MAP_FAILED && "mmap() failed!");

    prog->code = code;
    prog->code_len = 0;
    append_code(prog, codegen_prolouge, strlen(codegen_prolouge));

    int num_lambdas = 0;
    int lambda_stack[128] = {0};
    int64_t stack_top = 0;

    // main loop
    for (int64_t i = 0; i < prog->source_len; i++) {
        const char ch = prog->source[i];

        const symbol_codegen *gen = &codegen_lookup[(uint8_t)ch];
        if (gen->code != NULL) {
            append_code(prog, gen->code, gen->bytes-1);
            continue;
        }

        // Number constant
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

        // Start of UTF-8 encoding for both 'ø' and 'ß'
        if (ch == '\xC3') {
            if ((i + 1) == prog->source_len) break;
            if (prog->source[i + 1] == '\x9F') {
                // just skip ß (no buffered i/o)
                i++;
            } else if (prog->source[i + 1] == '\xB8') {
                const symbol_codegen *gen = &codegen_lookup[(uint8_t)'\xF8'];
                append_code(prog, gen->code, gen->bytes-1);
                i++;
            }
            continue;
        }

        // skip comments
        if (ch == '{') {
            char sym;
            int64_t idx = i;
            do {
                idx++;
                if (idx == prog->source_len) {
                    err_and_die("Unterminated comment!\nA closing '}' is required");
                }
            } while ((sym = prog->source[idx]) != '}');
            i = idx;
            continue;
        }

        // Character literal
        if (ch == '\'') {
            if ((i + 1) == prog->source_len) {
                err_and_die("Character literal is missing character!");
            }
            number_codegen(prog, prog->source[++i]);
            continue;
        }

        // String literals
        if (ch == '"') {
            char sym;
            int64_t idx = i;
            char *str_start = &prog->source[idx + 1];
            do {
                idx++;

                if (idx == prog->source_len) {
                    err_and_die("Unterminated string constant!");
                }
            } while ((sym = prog->source[idx]) != '"');

            string_codegen(prog, str_start, &prog->source[idx]);
            i = idx;
            continue;
        }

        if (ch == '`') {
            inline_assembler_codegen(prog);
            continue;
        }

        if (ch == '[') {
            lambda_entry_codegen(prog, num_lambdas);
            lambda_stack[stack_top++] = num_lambdas++;
            continue;
        }

        if (ch == ']') {
            const int lambda = lambda_stack[--stack_top];
            lambda_exit_codegen(prog, lambda);
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
    strcpy(prog->obj_fname, "false-tmpXXXXXX.o");
    int objfd = mkstemps(prog->obj_fname, 2);
    assert(objfd != -1 && "Couldn't make a temporary file! :(");

    pid_t cpid = vfork();
    if (cpid < 0) {
        err_and_die("vfork() failed");
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
            close(objfd);
            unlink(prog->asm_fname);
            unlink(prog->obj_fname);
            err_and_die("Failed to invoke nasm");
        }

        close(objfd);
        // don't need assembly file anymore
        unlink(prog->asm_fname);
    }
}

void link_false_program(false_program *prog) {
    pid_t cpid = vfork();
    if (cpid < 0) {
        err_and_die("vfork() failed");
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
            unlink(prog->obj_fname);
            err_and_die("Failed to invoke linker");
        }
        // don't need object file anymore
        unlink(prog->obj_fname);
    }
}
