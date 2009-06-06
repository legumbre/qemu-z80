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
#include "zx_keyboard.h"
#include "boards.h"

#ifdef CONFIG_LIBSPECTRUM
#include <libspectrum.h>
#endif

#define ROM_FILENAME "zx-rom.bin"

static uint32_t io_spectrum_read(void *opaque, uint32_t addr)
{
    if ((addr & 1) == 0) {
        return zx_keyboard_read(opaque, addr);
    } else {
        return 0xff;
    }
}

static void io_spectrum_write(void *opaque, uint32_t addr, uint32_t data)
{
    if ((addr & 1) == 0) {
        zx_video_set_border(data & 0x7);
    }
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
    ret = load_image_targphys(filename, 0x0000, rom_size);
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
    register_ioport_write(0, 0x10000, 1, io_spectrum_write, NULL);

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
        int i, p;
        const int pages_48k[3] = { 5, 2, 0 };

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
        for (p = 0; p < 3; p++) {
            page = libspectrum_snap_pages(snap, pages_48k[p]);
            for (i = 0x0000; i < 0x4000; i++) {
                stb_phys(i + ((p + 1) << 14), page[i]);
            }
        }

        zx_video_set_border(libspectrum_snap_out_ula(snap) & 0x7);

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
