#ifndef SAVE_MEMORY_COMPAT_H
#define SAVE_MEMORY_COMPAT_H

#define EEPROM_MAX_SIZE   0x800   /* 2KB */
#define SRAM_MAX_SIZE     0x8000  /* 32KB */
#define FLASHRAM_MAX_SIZE 0x20000 /* 128KB */
#define MEMPACK_MAX_SIZE  0x8000  /* 32KB */

#include "save_memory_compat.h"
struct save_memory_t saved_memory;

struct save_memory_t {
    unsigned char eeprom[EEPROM_MAX_SIZE];
    unsigned char sram[SRAM_MAX_SIZE];
    unsigned char flashram[FLASHRAM_MAX_SIZE];
    unsigned char mempack[MEMPACK_MAX_SIZE];
};

extern struct save_memory_t saved_memory;

#endif
