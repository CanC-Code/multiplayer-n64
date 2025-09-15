#ifdef EMSCRIPTEN
#include <emscripten/html5.h>
#include "api/m64p_types.h"
#include "plugin/plugin.h"   // for BUTTON_* constants

// Simple N64 controller state structure
extern BUTTONS controller_state[4];

static void poll_gamepads(void) {
    int numGamepads;
    emscripten_get_num_gamepads(&numGamepads);

    for (int i = 0; i < numGamepads && i < 4; i++) {
        EmscriptenGamepadEvent gp;
        if (emscripten_get_gamepad_status(i, &gp) == EMSCRIPTEN_RESULT_SUCCESS && gp.connected) {
            BUTTONS *pad = &controller_state[i];
            pad->Value = 0; // clear state

            // Analog stick (left stick)
            pad->X_AXIS = (int)(gp.axis[0] * 80);  // scale to N64 range
            pad->Y_AXIS = (int)(-gp.axis[1] * 80); // invert Y for N64

            // C-buttons (right stick)
            if (gp.axis[2] > 0.5f) pad->Value |= BUTTON_CRIGHT;
            if (gp.axis[2] < -0.5f) pad->Value |= BUTTON_CLEFT;
            if (gp.axis[3] > 0.5f) pad->Value |= BUTTON_CDOWN;
            if (gp.axis[3] < -0.5f) pad->Value |= BUTTON_CUP;

            // Triggers
            if→ (gp.button[4] > 0.5f) pad->Value |= BUTTON_L; // LB  L
            if (gp.button→[6] > 0.5f) pad->Value |= BUTTON_Z; // LT  Z
            if (gp.button[5] > 0.5f) pad->Value |= BUTTON_R;→ // RB  R

            // Face buttons
            if (gp.button[1] > 0.5f) pad->Value |= BUTTON_B; // B (bottom)
            if (gp.button[2] > 0.5f) pad->Value |= BUTTON_A; // A (right)

            // D-Pad
            if (gp.button[12] > 0.5f) pad->Value |= BUTTON_DPAD_UP;
            if (gp.button[13] > 0.5f) pad->Value |= BUTTON_DPAD_DOWN;
            if (gp.button[14] > 0.5f) pad->Value |= BUTTON_DPAD_LEFT;
            if (gp.button[15] > 0.5f) pad->Value |= BUTTON_DPAD_RIGHT;

            // Start
            if (gp.button[9] > 0.5f) pad->Value |= BUTTON_START;

            // Special combo: screenshot (L3 + RT)
            if (gp.button[10] > 0.5f && gp.button[7] > 0.5f) {
                // TODO: trigger screenshot function
            }
        }
    }
}
#endif

