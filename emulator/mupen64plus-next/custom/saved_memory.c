#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "saved_memory.h"

/* --------------------------------------------------------------------------
   Internal pointers and sizes
   These will be registered by saved_memory_init() from emulator memory.
   -------------------------------------------------------------------------- */

static uint8_t *g_mempak_ptr   = NULL;
static size_t   g_mempak_size  = 0;

static uint8_t *g_sram_ptr     = NULL;
static size_t   g_sram_size    = 0;

static uint8_t *g_flashram_ptr = NULL;
static size_t   g_flashram_size= 0;

static uint8_t *g_eeprom_ptr   = NULL;
static size_t   g_eeprom_size  = 0;

/* --------------------------------------------------------------------------
   File helpers
   -------------------------------------------------------------------------- */

static int load_file(const char *path, uint8_t *ptr, size_t size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    size_t n = fread(ptr, 1, size, f);
    fclose(f);

    return (n == size) ? 0 : -1;
}

static int save_file(const char *path, const uint8_t *ptr, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;

    size_t n = fwrite(ptr, 1, size, f);
    fclose(f);

    return (n == size) ? 0 : -1;
}

/* --------------------------------------------------------------------------
   API Implementation
   -------------------------------------------------------------------------- */

void saved_memory_init(void)
{
    /* In a full integration, emulator would call this with real pointers.
       For now these are just left NULL until wired up from memory.c */
}

int saved_memory_load_all(void)
{
    if (g_mempak_ptr   && g_mempak_size)   saved_memory_load_mempak();
    if (g_sram_ptr     && g_sram_size)     saved_memory_load_sram();
    if (g_flashram_ptr && g_flashram_size) saved_memory_load_flashram();
    if (g_eeprom_ptr   && g_eeprom_size)   saved_memory_load_eeprom();
    return 0;
}

int saved_memory_save_all(void)
{
    if (g_mempak_ptr   && g_mempak_size)   saved_memory_save_mempak();
    if (g_sram_ptr     && g_sram_size)     saved_memory_save_sram();
    if (g_flashram_ptr && g_flashram_size) saved_memory_save_flashram();
    if (g_eeprom_ptr   && g_eeprom_size)   saved_memory_save_eeprom();
    return 0;
}

/* --------------------------------------------------------------------------
   Individual operations
   -------------------------------------------------------------------------- */

int saved_memory_load_mempak(void)
{
    return load_file("saves/mempak.bin", g_mempak_ptr, g_mempak_size);
}

int saved_memory_save_mempak(void)
{
    return save_file("saves/mempak.bin", g_mempak_ptr, g_mempak_size);
}

int saved_memory_load_sram(void)
{
    return load_file("saves/sram.bin", g_sram_ptr, g_sram_size);
}

int saved_memory_save_sram(void)
{
    return save_file("saves/sram.bin", g_sram_ptr, g_sram_size);
}

int saved_memory_load_flashram(void)
{
    return load_file("saves/flashram.bin", g_flashram_ptr, g_flashram_size);
}

int saved_memory_save_flashram(void)
{
    return save_file("saves/flashram.bin", g_flashram_ptr, g_flashram_size);
}

int saved_memory_load_eeprom(void)
{
    return load_file("saves/eeprom.bin", g_eeprom_ptr, g_eeprom_size);
}

int saved_memory_save_eeprom(void)
{
    return save_file("saves/eeprom.bin", g_eeprom_ptr, g_eeprom_size);
}

/* --------------------------------------------------------------------------
   Query functions
   -------------------------------------------------------------------------- */

uint8_t *saved_memory_mempak_ptr(void)   { return g_mempak_ptr; }
size_t   saved_memory_mempak_size(void)  { return g_mempak_size; }

uint8_t *saved_memory_sram_ptr(void)     { return g_sram_ptr; }
size_t   saved_memory_sram_size(void)    { return g_sram_size; }

uint8_t *saved_memory_flashram_ptr(void) { return g_flashram_ptr; }
size_t   saved_memory_flashram_size(void){ return g_flashram_size; }

uint8_t *saved_memory_eeprom_ptr(void)   { return g_eeprom_ptr; }
size_t   saved_memory_eeprom_size(void)  { return g_eeprom_size; }

/* --------------------------------------------------------------------------
   Unload/Cleanup
   -------------------------------------------------------------------------- */

void saved_memory_unload_all(void)
{
    /* Ensure everything is written */
    saved_memory_save_all();

    /* Nothing else to free because we don?t own emulator memory.
       If in future malloc() is used for buffers, free them here. */
}
