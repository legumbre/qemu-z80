/*
 * SAM Coupé Keyboard Emulation
 *
 * Copyright (c) 2007-2009 Stuart Brady <stuart.brady@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "hw.h"
#include "qemu-timer.h"
#include "console.h"
#include "isa.h"
#include "sysemu.h"
#include "sam_keyboard.h"
#include "boards.h"

//#define DEBUG_SAM_KEYBOARD

#ifdef DEBUG_SAM_KEYBOARD
#define DPRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

static int keystate[9];

uint32_t sam_keyboard_read(void *opaque, uint32_t addr)
{
    int r = 0;
    uint8_t colbits = 0xff;
    uint32_t rowbits = ((addr >> 8) & 0xff);

    if (rowbits == 0xff) {
        colbits &= keystate[8];
    } else {
        for (r = 0; r < 8; r++) {
            if (!(rowbits & (1 << r))) {
                colbits &= keystate[r];
            }
        }
    }
    return colbits;
}

typedef struct {
    int row;
    int column;
} SAMKeypos;

#define DEF_SAM_KEY(name, row, column) SAM_KEY_ ## name,
enum sam_keys {
#include "sam_key_template.h"
SAM_MAX_KEYS
};

#define DEF_SAM_KEY(name, row, column) [SAM_KEY_ ## name] = {row, column},
static const SAMKeypos keypos[SAM_MAX_KEYS] = {
#include "sam_key_template.h"
};

static int sam_keypressed[SAM_MAX_KEYS];
static int qemu_keypressed[0x100];

static const int map[0x100][2] = {
    [0 ... 0xff] = {-1, -1}, /* Unmapped by default */

    [0x01] = {SAM_KEY_ESCAPE,    -1},

    [0x02] = {SAM_KEY_1, -1},
    [0x03] = {SAM_KEY_2, -1},
    [0x04] = {SAM_KEY_3, -1},
    [0x05] = {SAM_KEY_4, -1},
    [0x06] = {SAM_KEY_5, -1},
    [0x07] = {SAM_KEY_6, -1},
    [0x08] = {SAM_KEY_7, -1},
    [0x09] = {SAM_KEY_8, -1},
    [0x0a] = {SAM_KEY_9, -1},
    [0x0b] = {SAM_KEY_0, -1},

    [0x0c] = {SAM_KEY_MINUS,     -1},

    [0x0e] = {SAM_KEY_DELETE,    -1}, /* Backspace */

    [0x10] = {SAM_KEY_Q, -1},
    [0x11] = {SAM_KEY_W, -1},
    [0x12] = {SAM_KEY_E, -1},
    [0x13] = {SAM_KEY_R, -1},
    [0x14] = {SAM_KEY_T, -1},
    [0x15] = {SAM_KEY_Y, -1},
    [0x16] = {SAM_KEY_U, -1},
    [0x17] = {SAM_KEY_I, -1},
    [0x18] = {SAM_KEY_O, -1},
    [0x19] = {SAM_KEY_P, -1},

    [0x0d] = {SAM_KEY_PLUS,      -1},
    [0x0f] = {SAM_KEY_TAB,       -1},

    [0x1a] = {SAM_KEY_EQUALS,    -1},
    [0x1b] = {SAM_KEY_DQUOTE,    -1},

    [0x1c] = {SAM_KEY_ENTER,     -1},

    [0x1d] = {SAM_KEY_CONTROL,   -1}, /* Left Control */

    [0x1e] = {SAM_KEY_A, -1},
    [0x1f] = {SAM_KEY_S, -1},
    [0x20] = {SAM_KEY_D, -1},
    [0x21] = {SAM_KEY_F, -1},
    [0x22] = {SAM_KEY_G, -1},
    [0x23] = {SAM_KEY_H, -1},
    [0x24] = {SAM_KEY_J, -1},
    [0x25] = {SAM_KEY_K, -1},
    [0x26] = {SAM_KEY_L, -1},

    [0x27] = {SAM_KEY_SEMICOLON, -1}, /* Semicolon */
    [0x28] = {SAM_KEY_SYMBSHIFT, SAM_KEY_7}, /* Apostrophe */

    [0x2a] = {SAM_KEY_CAPSSHIFT, -1}, /* Left Shift */

    [0x2b] = {SAM_KEY_SYMBSHIFT, SAM_KEY_3}, /* Hash */

    [0x2c] = {SAM_KEY_Z, -1},
    [0x2d] = {SAM_KEY_X, -1},
    [0x2e] = {SAM_KEY_C, -1},
    [0x2f] = {SAM_KEY_V, -1},
    [0x30] = {SAM_KEY_B, -1},
    [0x31] = {SAM_KEY_N, -1},
    [0x32] = {SAM_KEY_M, -1},

    [0x33] = {SAM_KEY_COMMA,     -1},
    [0x34] = {SAM_KEY_PERIOD,    -1},
    [0x35] = {SAM_KEY_SYMBSHIFT, -1}, /* Slash */

    [0x36] = {SAM_KEY_CAPSSHIFT, -1}, /* Right Shift */
    [0x37] = {SAM_KEY_SYMBSHIFT, SAM_KEY_B}, /* * (Numpad) */
    [0x38] = {SAM_KEY_SYMBSHIFT, -1}, /* Left Alt */
    [0x39] = {SAM_KEY_SPACE,     -1}, /* Space Bar */

    [0x47] = {SAM_KEY_F7,        -1}, /* 7 (Numpad) */
    [0x48] = {SAM_KEY_F8,        -1}, /* 8 (Numpad) */
    [0x49] = {SAM_KEY_F9,        -1}, /* 9 (Numpad) */
    [0x4a] = {SAM_KEY_MINUS,     -1}, /* Minus (Numpad) */
    [0x4b] = {SAM_KEY_F4,        -1}, /* 4 (Numpad) */
    [0x4c] = {SAM_KEY_F5,        -1}, /* 5 (Numpad) */
    [0x4d] = {SAM_KEY_F6,        -1}, /* 6 (Numpad) */
    [0x4e] = {SAM_KEY_PLUS,      -1}, /* Plus (Numpad) */
    [0x4f] = {SAM_KEY_F1,        -1}, /* 1 (Numpad) */
    [0x50] = {SAM_KEY_F2,        -1}, /* 2 (Numpad) */
    [0x51] = {SAM_KEY_F3,        -1}, /* 3 (Numpad) */
    [0x52] = {SAM_KEY_F0,        -1}, /* 0 (Numpad) */
    [0x53] = {SAM_KEY_PERIOD,    -1}, /* Period (Numpad) */

    [0x9c] = {SAM_KEY_ENTER,     -1}, /* Enter (Numpad) */
    [0x9d] = {SAM_KEY_CONTROL,   -1}, /* Right Control */
    [0xb5] = {SAM_KEY_SYMBSHIFT, SAM_KEY_V}, /* Slash (Numpad) */
    [0xb8] = {SAM_KEY_EDIT,      -1}, /* Right Alt */

    [0xc8] = {SAM_KEY_UP,        SAM_KEY_7}, /* Up Arrow */
    [0xcb] = {SAM_KEY_LEFT,      SAM_KEY_5}, /* Left Arrow */
    [0xcd] = {SAM_KEY_RIGHT,     SAM_KEY_8}, /* Right Arrow */
    [0xd0] = {SAM_KEY_DOWN,      SAM_KEY_6}, /* Down Arrow */

    [0xdb] = {SAM_KEY_CONTROL,   SAM_KEY_SYMBSHIFT}, /* Left Meta */
    [0xdc] = {SAM_KEY_EDIT,      SAM_KEY_SYMBSHIFT}, /* Menu */
    [0xdd] = {SAM_KEY_CONTROL,   SAM_KEY_SYMBSHIFT}, /* Right Meta */
};

/* FIXME:
 *   Need to mappings from stepping on each other...
 *   or at least make them step on one another in a consistent manner?
 *   Could use separate state arrays for surpressing/adding keys
 *   and allow only one change to the modifier keys at a time...
 *
 * Also need to implement shifted mappings.
 */

static void sam_put_keycode(void *opaque, int keycode)
{
    int release = keycode & 0x80;
    int key, row, col;
    static int ext_keycode = 0;
    int i;
    int valid;

    if (keycode == 0xe0) {
        ext_keycode = 1;
    } else {
        if (ext_keycode) {
            keycode |= 0x80;
        } else {
            keycode &= 0x7f;
        }
        ext_keycode = 0;

        DPRINTF("Keycode 0x%02x (%s)\n", keycode,
                release ? "release" : "press");

        if (release && qemu_keypressed[keycode]) {
            valid = 1;
            qemu_keypressed[keycode] = 0;
        } else if (!release && !qemu_keypressed[keycode]) {
            valid = 1;
            qemu_keypressed[keycode] = 1;
        } else {
            valid = 0;
        }

        if (valid) {
            for (i = 0; i < 2; i++) {
                key = map[keycode][i];
                if (key != -1) {
                    row = keypos[key].row;
                    col = keypos[key].column;
                    if (release) {
                        if (--sam_keypressed[key] <= 0) {
                            DPRINTF("Releasing 0x%02x\n", key);
                            sam_keypressed[key] = 0;
                            keystate[row] |= 1 << col;
                        }
                    } else {
                        DPRINTF("Pressing 0x%02x\n", key);
                        sam_keypressed[key]++;
                        keystate[row] &= ~(1 << col);
                    }
                }
            }
        }
    }
}

void sam_keyboard_init(void)
{
    int i;
    for (i=0; i<9; i++) {
        keystate[i] = 0xff;
    }
    memset(sam_keypressed, 0, sizeof(sam_keypressed));
    memset(qemu_keypressed, 0, sizeof(qemu_keypressed));
    qemu_add_kbd_event_handler(sam_put_keycode, NULL);
}
