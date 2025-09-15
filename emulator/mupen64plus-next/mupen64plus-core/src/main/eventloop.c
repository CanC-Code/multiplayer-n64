/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - eventloop.c                                             *
 *   Handle core event loop and SDL input events                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>              /* <-- Added to fix SDL_Event, SDL_Joystick, etc */
#include "eventloop.h"
#include "main.h"
#include "plugin.h"
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/m64p_plugin.h"

#include "custom/joy_compat.h"

/* -------------------------------------------------------------------------
   Global joystick instance ID mapping
   SDL2 identifies joysticks with instance IDs, not device indices.
   We store the mapping here.
   ------------------------------------------------------------------------- */
#define MAX_JOYSTICKS 16
static int l_iJoyInstanceID[MAX_JOYSTICKS];

/* -------------------------------------------------------------------------
   Function: MatchJoyCommand
   Match joystick events to configured joycommands
   ------------------------------------------------------------------------- */
static int MatchJoyCommand(const SDL_Event *event, eJoyCommand *outCmd)
{
    if (!event || !outCmd)
        return 0;

    switch (event->type)
    {
        case SDL_JOYAXISMOTION:
            /* Example: map joystick axis */
            *outCmd = CMD_AXIS;
            return 1;

        case SDL_JOYHATMOTION:
            *outCmd = CMD_HAT;
            return 1;

        case SDL_JOYBUTTONDOWN:
            *outCmd = CMD_BUTTON;
            return 1;

        case SDL_JOYBUTTONUP:
            *outCmd = CMD_BUTTON;
            return 1;

        default:
            return 0;
    }
}

/* -------------------------------------------------------------------------
   SDL event filter for joystick events
   ------------------------------------------------------------------------- */
static int SDLCALL event_sdl_filter(void *userdata, SDL_Event *event)
{
    (void)userdata;
    eJoyCommand joyCmd;

    if (MatchJoyCommand(event, &joyCmd))
    {
        DebugMessage(M64MSG_VERBOSE, "Joystick command matched: %d", joyCmd);
        return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
   Initialize joystick subsystem
   ------------------------------------------------------------------------- */
void InitJoystick(void)
{
    if (!SDL_WasInit(SDL_INIT_JOYSTICK))
        SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    int numJoysticks = SDL_NumJoysticks();
    DebugMessage(M64MSG_INFO, "Detected %d joystick(s)", numJoysticks);

    for (int device = 0; device < numJoysticks && device < MAX_JOYSTICKS; device++)
    {
        SDL_Joystick *thisJoy = SDL_JoystickOpen(device);
        if (thisJoy)
        {
            l_iJoyInstanceID[device] = SDL_JoystickInstanceID(thisJoy);
            DebugMessage(M64MSG_INFO, "Opened joystick %d, instance ID %d", device, l_iJoyInstanceID[device]);
        }
        else
        {
            l_iJoyInstanceID[device] = -1;
            DebugMessage(M64MSG_WARNING, "Failed to open joystick %d", device);
        }
    }

    SDL_SetEventFilter(event_sdl_filter, NULL);
}

/* -------------------------------------------------------------------------
   Shutdown joystick subsystem
   ------------------------------------------------------------------------- */
void ShutdownJoystick(void)
{
    for (int device = 0; device < MAX_JOYSTICKS; device++)
    {
        if (l_iJoyInstanceID[device] != -1)
        {
            SDL_Joystick *joy = SDL_JoystickFromInstanceID(l_iJoyInstanceID[device]);
            if (joy)
                SDL_JoystickClose(joy);
            l_iJoyInstanceID[device] = -1;
        }
    }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}
