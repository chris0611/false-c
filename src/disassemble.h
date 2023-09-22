#ifndef FALSE_C_DISASSEMBLE_H
#define FALSE_C_DISASSEMBLE_H

#include <stdint.h>

char *machinecode_to_assembly(const uint8_t *code, int64_t len);

#endif /* FALSE_C_DISASSEMBLE_H */
