#ifndef CUSTOM_LIBRETRO_PRIVATE_H
#define CUSTOM_LIBRETRO_PRIVATE_H

#include <stdbool.h>
#include "libretro.h"  /* brings in retro_memory_descriptor, retro_memory_map, retro_log_printf_t, etc. */

extern retro_log_printf_t log_cb;

/* RetroArch environment callback */
typedef bool (*retro_environment_t)(unsigned cmd, void *data);
extern retro_environment_t environ_cb;

/* RetroArch logging callback (already typedef'd in libretro.h) */
extern retro_log_printf_t log_cb;

#endif /* CUSTOM_LIBRETRO_PRIVATE_H */
