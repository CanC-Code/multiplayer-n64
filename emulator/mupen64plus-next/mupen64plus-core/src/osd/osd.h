/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - osd.h                                                   *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2008 Nmn, Ebenblues                                     *
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

#ifndef __OSD_H__
#define __OSD_H__

#include "main/list.h"
#include "osal/preproc.h"

#if defined(__GNUC__)
#define ATTR_FMT(fmtpos, attrpos) __attribute__ ((format (printf, fmtpos, attrpos)))
#else
#define ATTR_FMT(fmtpos, attrpos)
#endif

/******************************************************************
   osd_corner
   0    1    2 |
    \ __|__/   | Offset always effects the same:
     |     |   |  +X = Leftward   +Y = Upward
   3-|  4  |-5 |  With no offset, the text will touch the border.
     |_____|   |
    /   |   \  |
   6    7    8 |
*******************************************************************/
enum osd_corner {
    OSD_TOP_LEFT,       // 0
    OSD_TOP_CENTER,     // 1
    OSD_TOP_RIGHT,      // 2

    OSD_MIDDLE_LEFT,    // 3
    OSD_MIDDLE_CENTER,  // 4
    OSD_MIDDLE_RIGHT,   // 5

    OSD_BOTTOM_LEFT,    // 6
    OSD_BOTTOM_CENTER,  // 7
    OSD_BOTTOM_RIGHT,   // 8

    OSD_NUM_CORNERS
};

enum osd_message_state {
    OSD_APPEAR,
    OSD_DISPLAY,
    OSD_DISAPPEAR,

    OSD_NUM_STATES
};

enum osd_animation_type {
    OSD_NONE,
    OSD_FADE,

    OSD_NUM_ANIM_TYPES
};

typedef struct {
    char *text;        // message text
    enum osd_corner corner;
    float xoffset;
    float yoffset;
    float color[3];
    float sizebox[4];  // bounding box
    int state;
    enum osd_animation_type animation[OSD_NUM_STATES];
    unsigned int timeout[OSD_NUM_STATES];
#define OSD_INFINITE_TIMEOUT 0xffffffff
    unsigned int frames;
    int user_managed;
    struct list_head list;
} osd_message_t;

enum { R, G, B }; // color indices

/* Function prototypes (implemented in osd.c) */
void osd_init(int width, int height);
void osd_exit(void);
void osd_render(void);
osd_message_t * osd_new_message(enum osd_corner, const char *, ...) ATTR_FMT(2, 3);
void osd_update_message(osd_message_t *, const char *, ...) ATTR_FMT(2, 3);
void osd_delete_message(osd_message_t *);
void osd_message_set_static(osd_message_t *);
void osd_message_set_user_managed(osd_message_t *);

#endif // __OSD_H__
