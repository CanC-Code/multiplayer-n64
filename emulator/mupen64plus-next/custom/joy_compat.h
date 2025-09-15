#ifndef JOY_COMPAT_H
#define JOY_COMPAT_H

// Replacement for eJoyCommand and CMD_* constants
typedef enum {
    CMD_AXIS   = 0,
    CMD_HAT    = 1,
    CMD_BUTTON = 2
} eJoyCommand;

#endif // JOY_COMPAT_H
