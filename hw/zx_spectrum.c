/*
 * QEMU ZX Spectrum Emulator
 *
 * Copyright (c) 2007-2009 Stuart Brady <stuart.brady@gmail.com>
 * Copyright (c) 2007 Ulrich Hecht
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
#include "zx_video.h"
#include "boards.h"

#ifdef CONFIG_LIBSPECTRUM
#include <libspectrum.h>
#endif

#define ROM_FILENAME "zx-rom.bin"

//#define DEBUG_ZX_SPECTRUM

#ifdef DEBUG_ZX_SPECTRUM
#define DPRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

static int keystate[8];

static uint32_t io_keyboard_read(void *opaque, uint32_t addr)
{
    int r = 0;
    uint8_t colbits = 0xff;

    uint32_t rowbits = ((addr >> 8) & 0xff);

    for (r = 0; r < 8; r++) {
        if (!(rowbits & (1 << r))) {
            colbits &= keystate[r];
        }
    }
    return colbits;
}

static uint32_t io_spectrum_read(void *opaque, uint32_t addr)
{
    if (addr & 1) {
        return 0xff;
    }

    return io_keyboard_read(opaque, addr);
}

static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;
    cpu_reset(env);
}

static QEMUTimer *zx_ula_timer;

static void zx_50hz_timer(void *opaque)
{
    int64_t next_time;

    CPUState *env = opaque;
    cpu_interrupt(env, CPU_INTERRUPT_HARD);

    /* FIXME: not exactly 50 Hz */
    next_time = qemu_get_clock(vm_clock) + muldiv64(1, ticks_per_sec, 50);
    qemu_mod_timer(zx_ula_timer, next_time);

    zx_video_do_retrace();
}

static CPUState *zx_env;

static void zx_timer_init(void)
{
    int64_t t = qemu_get_clock(vm_clock);
    zx_ula_timer = qemu_new_timer(vm_clock, zx_50hz_timer, zx_env);
    qemu_mod_timer(zx_ula_timer, t);
}

typedef struct {
    int row;
    int column;
} ZXKeypos;

#define DEF_ZX_KEY(name, row, column) ZX_KEY_ ## name,
enum zx_keys {
#include "zx_key_template.h"
ZX_MAX_KEYS
};

#define DEF_ZX_KEY(name, row, column) [ZX_KEY_ ## name] = {row, column},
static const ZXKeypos keypos[ZX_MAX_KEYS] = {
#include "zx_key_template.h"
};

static int zx_keypressed[ZX_MAX_KEYS];
static int qemu_keypressed[0x100];

static const int map[0x100][2] = {
    [0 ... 0xff] = {-1, -1}, /* Unmapped by default */

    [0x01] = {ZX_KEY_CAPSSHIFT, ZX_KEY_SPACE}, /* Escape */

    [0x02] = {ZX_KEY_1, -1},
    [0x03] = {ZX_KEY_2, -1},
    [0x04] = {ZX_KEY_3, -1},
    [0x05] = {ZX_KEY_4, -1},
    [0x06] = {ZX_KEY_5, -1},
    [0x07] = {ZX_KEY_6, -1},
    [0x08] = {ZX_KEY_7, -1},
    [0x09] = {ZX_KEY_8, -1},
    [0x0a] = {ZX_KEY_9, -1},
    [0x0b] = {ZX_KEY_0, -1},

    [0x0c] = {ZX_KEY_SYMBSHIFT, ZX_KEY_J}, /* Minus */

    [0x0e] = {ZX_KEY_CAPSSHIFT, ZX_KEY_0}, /* Backspace */

    [0x10] = {ZX_KEY_Q, -1},
    [0x11] = {ZX_KEY_W, -1},
    [0x12] = {ZX_KEY_E, -1},
    [0x13] = {ZX_KEY_R, -1},
    [0x14] = {ZX_KEY_T, -1},
    [0x15] = {ZX_KEY_Y, -1},
    [0x16] = {ZX_KEY_U, -1},
    [0x17] = {ZX_KEY_I, -1},
    [0x18] = {ZX_KEY_O, -1},
    [0x19] = {ZX_KEY_P, -1},

    [0x0d] = {ZX_KEY_SYMBSHIFT, ZX_KEY_L}, /* Equals */
    [0x0f] = {ZX_KEY_CAPSSHIFT, ZX_KEY_1}, /* Tab */

    [0x1c] = {ZX_KEY_ENTER,     -1}, /* Enter */

    [0x1d] = {ZX_KEY_SYMBSHIFT, -1}, /* Left Control */

    [0x1e] = {ZX_KEY_A, -1},
    [0x1f] = {ZX_KEY_S, -1},
    [0x20] = {ZX_KEY_D, -1},
    [0x21] = {ZX_KEY_F, -1},
    [0x22] = {ZX_KEY_G, -1},
    [0x23] = {ZX_KEY_H, -1},
    [0x24] = {ZX_KEY_J, -1},
    [0x25] = {ZX_KEY_K, -1},
    [0x26] = {ZX_KEY_L, -1},

    [0x27] = {ZX_KEY_SYMBSHIFT, ZX_KEY_O}, /* Semicolon */
    [0x28] = {ZX_KEY_SYMBSHIFT, ZX_KEY_7}, /* Apostrophe */

    [0x2a] = {ZX_KEY_CAPSSHIFT, -1}, /* Left Shift */

    [0x2b] = {ZX_KEY_SYMBSHIFT, ZX_KEY_3}, /* Hash */

    [0x2c] = {ZX_KEY_Z, -1},
    [0x2d] = {ZX_KEY_X, -1},
    [0x2e] = {ZX_KEY_C, -1},
    [0x2f] = {ZX_KEY_V, -1},
    [0x30] = {ZX_KEY_B, -1},
    [0x31] = {ZX_KEY_N, -1},
    [0x32] = {ZX_KEY_M, -1},

    [0x33] = {ZX_KEY_SYMBSHIFT, ZX_KEY_N}, /* Period */
    [0x34] = {ZX_KEY_SYMBSHIFT, ZX_KEY_M}, /* Comma */
    [0x35] = {ZX_KEY_SYMBSHIFT, ZX_KEY_V}, /* Slash */

    [0x36] = {ZX_KEY_CAPSSHIFT, -1}, /* Right Shift */
    [0x37] = {ZX_KEY_SYMBSHIFT, ZX_KEY_B}, /* * (Numpad) */
    [0x38] = {ZX_KEY_SYMBSHIFT, -1}, /* Left Alt */
    [0x39] = {ZX_KEY_SPACE,     -1}, /* Space Bar */

    [0x47] = {ZX_KEY_7,         -1}, /* 7 (Numpad) */
    [0x48] = {ZX_KEY_8,         -1}, /* 8 (Numpad) */
    [0x49] = {ZX_KEY_9,         -1}, /* 9 (Numpad) */
    [0x4a] = {ZX_KEY_SYMBSHIFT, ZX_KEY_J}, /* Minus (Numpad) */
    [0x4b] = {ZX_KEY_4,         -1}, /* 4 (Numpad) */
    [0x4c] = {ZX_KEY_5,         -1}, /* 5 (Numpad) */
    [0x4d] = {ZX_KEY_6,         -1}, /* 6 (Numpad) */
    [0x4e] = {ZX_KEY_SYMBSHIFT, ZX_KEY_K}, /* Plus (Numpad) */
    [0x4f] = {ZX_KEY_1,         -1}, /* 1 (Numpad) */
    [0x50] = {ZX_KEY_2,         -1}, /* 2 (Numpad) */
    [0x51] = {ZX_KEY_3,         -1}, /* 3 (Numpad) */
    [0x52] = {ZX_KEY_0,         -1}, /* 0 (Numpad) */
    [0x53] = {ZX_KEY_SYMBSHIFT, ZX_KEY_M}, /* Period (Numpad) */

    [0x9c] = {ZX_KEY_SYMBSHIFT, -1}, /* Enter (Numpad) */
    [0x9d] = {ZX_KEY_SYMBSHIFT, -1}, /* Right Control */
    [0xb5] = {ZX_KEY_SYMBSHIFT, ZX_KEY_V}, /* Slash (Numpad) */
    [0xb8] = {ZX_KEY_SYMBSHIFT, -1}, /* Right Alt */

    [0xc8] = {ZX_KEY_CAPSSHIFT, ZX_KEY_7}, /* Up Arrow */
    [0xcb] = {ZX_KEY_CAPSSHIFT, ZX_KEY_5}, /* Left Arrow */
    [0xcd] = {ZX_KEY_CAPSSHIFT, ZX_KEY_8}, /* Right Arrow */
    [0xd0] = {ZX_KEY_CAPSSHIFT, ZX_KEY_6}, /* Down Arrow */

    [0xdb] = {ZX_KEY_CAPSSHIFT, ZX_KEY_SYMBSHIFT}, /* Left Meta */
    [0xdc] = {ZX_KEY_CAPSSHIFT, ZX_KEY_SYMBSHIFT}, /* Menu */
    [0xdd] = {ZX_KEY_CAPSSHIFT, ZX_KEY_SYMBSHIFT}, /* Right Meta */
};

/* FIXME:
 *   Need to mappings from stepping on each other...
 *   or at least make them step on one another in a consistent manner?
 *   Could use separate state arrays for surpressing/adding keys
 *   and allow only one change to the modifier keys at a time...
 *
 * Also need to implement shifted mappings.
 */

static void zx_put_keycode(void *opaque, int keycode)
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

        DPRINTF("Keycode 0x%02x (%s)\n", keycode, release ? "release" : "press");

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
                        if (--zx_keypressed[key] <= 0) {
                            DPRINTF("Releasing 0x%02x\n", key);
                            zx_keypressed[key] = 0;
                            keystate[row] |= 1 << col;
                        }
                    } else {
                        DPRINTF("Pressing 0x%02x\n", key);
                        zx_keypressed[key]++;
                        keystate[row] &= ~(1 << col);
                    }
                }
            }
        }
    }
}

static void zx_keyboard_init(void)
{
    int i;
    for (i=0; i<8; i++) {
        keystate[i] = 0xff;
    }
    memset(zx_keypressed, 0, sizeof(zx_keypressed));
    memset(qemu_keypressed, 0, sizeof(qemu_keypressed));
    qemu_add_kbd_event_handler(zx_put_keycode, NULL);
}

static const uint8_t halthack_oldip[16] =
    {253, 203, 1,110, 200, 58, 8, 92, 253, 203, 1, 174};
static const uint8_t halthack_newip[16] =
    {33, 59, 92, 118, 203, 110, 200, 58, 8, 92, 203, 174};

/* ZX Spectrum initialisation */
static void zx_spectrum_init(ram_addr_t ram_size,
                             const char *boot_device,
                             const char *kernel_filename,
                             const char *kernel_cmdline,
                             const char *initrd_filename,
                             const char *cpu_model)
{
    char *filename;
    uint8_t halthack_curip[12];
    int ret;
    ram_addr_t ram_offset, rom_offset;
    int rom_size;
    CPUState *env;

    /* init CPUs */
    if (!cpu_model) {
        cpu_model = "z80";
    }
    env = cpu_init(cpu_model);
    zx_env = env; // XXX
    register_savevm("cpu", 0, 4, cpu_save, cpu_load, env);
    qemu_register_reset(main_cpu_reset, 0, env);
    main_cpu_reset(env);

    /* allocate RAM */
    ram_offset = qemu_ram_alloc(0xc000);
    cpu_register_physical_memory(0x4000, 0xc000, ram_offset | IO_MEM_RAM);

    /* ROM load */
    if (bios_name == NULL) {
        bios_name = ROM_FILENAME;
    }
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    if (filename) {
        rom_size = get_image_size(filename);
    } else {
        rom_size = -1;
    }
    if (rom_size <= 0 ||
        (rom_size % 0x4000) != 0) {
        goto rom_error;
    }
    rom_offset = qemu_ram_alloc(rom_size);
    cpu_register_physical_memory(0x0000, 0x4000, rom_offset | IO_MEM_ROM);
    ret = load_image_targphys(filename, 0, rom_size);
    if (ret != rom_size) {
    rom_error:
        fprintf(stderr, "qemu: could not load ZX Spectrum ROM '%s'\n",
                        bios_name);
        exit(1);
    }
    if (filename) {
        qemu_free(filename);
    }

    /* hack from xz80 adding HALT to the keyboard input loop to save CPU */
    cpu_physical_memory_read(0x10b0, halthack_curip, 12);
    if (!memcmp(halthack_curip, halthack_oldip, 12)) {
        cpu_physical_memory_write_rom(0x10b0, halthack_newip, 12);
    }

    /* map entire I/O space */
    register_ioport_read(0, 0x10000, 1, io_spectrum_read, NULL);

    zx_video_init(ram_offset);

    zx_keyboard_init();
    zx_timer_init();

#ifdef CONFIG_LIBSPECTRUM
    /* load a snapshot */
    if (kernel_filename) {
        libspectrum_id_t type;
        libspectrum_class_t cls;
        libspectrum_snap* snap;
        uint8_t* snapmem;
        libspectrum_byte* page;
        int length;
        int i;
        if (libspectrum_init() != LIBSPECTRUM_ERROR_NONE ||
            libspectrum_identify_file(&type, kernel_filename, NULL, 0) != LIBSPECTRUM_ERROR_NONE ||
            libspectrum_identify_class(&cls, type) != LIBSPECTRUM_ERROR_NONE) {
            fprintf(stderr, "%s: libspectrum error\n", __FUNCTION__);
            exit(1);
        }
        snap = libspectrum_snap_alloc();
        if (cls != LIBSPECTRUM_CLASS_SNAPSHOT) {
            fprintf(stderr, "%s: %s is not a snapshot\n", __FUNCTION__, kernel_filename);
            exit(1);
        }
        snapmem = qemu_mallocz(0x10000);
        length = load_image(kernel_filename, snapmem);
        //printf("loaded %d bytes from %s\n",length, kernel_filename);
        if (libspectrum_snap_read(snap, snapmem, length, type, NULL) != LIBSPECTRUM_ERROR_NONE) {
            fprintf(stderr, "%s: failed to load snapshot %s\n", __FUNCTION__, kernel_filename);
            exit(1);
        }
        //printf("snap pc = %d\n",libspectrum_snap_pc(snap));

        /* fill memory */
        page = libspectrum_snap_pages(snap, 5);
        for (i = 0x4000; i < 0x8000; i++) {
            //printf("storing 0x%x in 0x%x\n",page[i-0x4000],i);
            stb_phys(i, page[i - 0x4000]);
        }
        page = libspectrum_snap_pages(snap, 2);
        for (i = 0x8000; i < 0xc000; i++) {
            stb_phys(i, page[i - 0x8000]);
        }
        page = libspectrum_snap_pages(snap, 0);
        for (i = 0xc000; i < 0x10000; i++) {
            stb_phys(i, page[i - 0xc000]);
        }

        /* restore registers */
        env->regs[R_A] = libspectrum_snap_a(snap);
        env->regs[R_F] = libspectrum_snap_f(snap);
        env->regs[R_BC] = libspectrum_snap_bc(snap);
        env->regs[R_DE] = libspectrum_snap_de(snap);
        env->regs[R_HL] = libspectrum_snap_hl(snap);
        env->regs[R_AX] = libspectrum_snap_a_(snap);
        env->regs[R_FX] = libspectrum_snap_f_(snap);
        env->regs[R_BCX] = libspectrum_snap_bc_(snap);
        env->regs[R_DEX] = libspectrum_snap_de_(snap);
        env->regs[R_HLX] = libspectrum_snap_hl_(snap);
        env->regs[R_IX] = libspectrum_snap_ix(snap);
        env->regs[R_IY] = libspectrum_snap_iy(snap);
        env->regs[R_I] = libspectrum_snap_i(snap);
        env->regs[R_R] = libspectrum_snap_r(snap);
        env->regs[R_SP] = libspectrum_snap_sp(snap);
        env->pc = libspectrum_snap_pc(snap);
        env->iff1 = libspectrum_snap_iff1(snap);
        env->iff2 = libspectrum_snap_iff2(snap);
        env->imode = libspectrum_snap_im(snap);

        qemu_free(snapmem);
    }
#endif
}

static QEMUMachine zxspec_machine = {
    .name = "zxspec",
    .desc = "ZX Spectrum",
    .init = zx_spectrum_init,
    .is_default = 1,
};

static void zxspec_machine_init(void) {
    qemu_register_machine(&zxspec_machine);
}

machine_init(zxspec_machine_init);
