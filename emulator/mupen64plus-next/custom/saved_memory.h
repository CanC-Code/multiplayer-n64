#ifndef CUSTOM_SAVED_MEMORY_H
#define CUSTOM_SAVED_MEMORY_H

#include <stddef.h>
#include <stdint.h>

/* High-level saved memory accessors used by main.c and file_storage/backends.
 * This exposes the in-memory buffers (pointers + sizes) for each save type.
 *
 * Usage:
 *   saved_memory_init();              // after core memory is allocated
 *   saved_memory_load_all();          // load from disk (or IDBFS when running in Emscripten)
 *   ... emulator runs ...
 *   saved_memory_save_all();          // persist to disk
 *   saved_memory_unload_all();        // flush + cleanup before shutdown
 *
 * The implementation uses simple binary files under "saves/" directory:
 *   saves/mempak.bin
 *   saves/sram.bin
 *   saves/flashram.bin
 *   saves/eeprom.bin
 *
 * For web builds you must mount an emscripten filesystem (IDBFS) at "/saves"
 * from the HTML/JS BEFORE calling into the module, and sync it after saves.
 *
 * The backing buffers used here are *views* into emulator memory; saved_memory
 * does NOT allocate the big buffers itself except for optional small local copy.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* initialize saved memory subsystem
 * Must be called once after emulator memory structures exist (so pointers are valid).
 * It will register internal pointers (no huge allocation).
 */
void saved_memory_init(void);

/* Load all save files from disk into emulator memory. */
int saved_memory_load_all(void);

/* Save all memory areas into disk files. */
int saved_memory_save_all(void);

/* Unload (flush + cleanup) all save data. */
void saved_memory_unload_all(void);

/* Individual save operations (return 0 on success) */
int saved_memory_load_mempak(void);
int saved_memory_save_mempak(void);

int saved_memory_load_sram(void);
int saved_memory_save_sram(void);

int saved_memory_load_flashram(void);
int saved_memory_save_flashram(void);

int saved_memory_load_eeprom(void);
int saved_memory_save_eeprom(void);

/* Query functions - pointers may be NULL if emulator didn't allocate them */
uint8_t *saved_memory_mempak_ptr(void);
size_t saved_memory_mempak_size(void);

uint8_t *saved_memory_sram_ptr(void);
size_t saved_memory_sram_size(void);

uint8_t *saved_memory_flashram_ptr(void);
size_t saved_memory_flashram_size(void);

uint8_t *saved_memory_eeprom_ptr(void);
size_t saved_memory_eeprom_size(void);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_SAVED_MEMORY_H */
