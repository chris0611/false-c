#include <capstone/capstone.h>

#include "disassemble.h"

char *machinecode_to_assembly(const uint8_t *code, int64_t len) {
    csh handle;
    cs_insn *insn;
    size_t count;

    if (cs_open(CS_ARCH_M68K, CS_MODE_M68K_060, &handle) != CS_ERR_OK) {
        fprintf(stderr, "Failed to initialize disassembler\n");
        return NULL;
    }

    count = cs_disasm(handle, code, len, 0x0000, 0, &insn);
    if (count == 0) {
        cs_err err = cs_errno(handle);
        fprintf(stderr, "Failed to disassemble given code: %s\n", cs_strerror(err));
        return NULL;
    }

    // print instructions
    for (size_t i = 0; i < count; i++) {
        printf("0x%"PRIx64":\t", insn[i].address);
        for (size_t j = 0; j < insn[i].size; j++) {
            printf("%02hhx ", insn[i].bytes[j]);
        }
        printf("\t%s\t%s\n", insn[i].mnemonic, insn[i].op_str);
    }

    cs_free(insn, count);
    cs_close(&handle);
    return NULL;
}
