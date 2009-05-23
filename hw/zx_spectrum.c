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
#include "zx_ula.h"
#include "boards.h"

#ifdef CONFIG_LIBSPECTRUM
#include <libspectrum.h>
#endif

/* output Bochs bios info messages */
//#define DEBUG_BIOS

#define ROM_FILENAME "zx-rom.bin"

int keystate[8];

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
    if (addr & 1)
        return 0xff;

    return io_keyboard_read(opaque, addr);
}

static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;
    cpu_reset(env);
}

QEMUTimer *zx_ulatimer;

void zx_50hz_timer(void *opaque)
{
//    printf("zx_irq_timer()\n");
    int64_t next_time;

    CPUState *env = opaque;
    cpu_interrupt(env, CPU_INTERRUPT_HARD);

    /* FIXME: 50 Hz */
    next_time = qemu_get_clock(vm_clock) + muldiv64(1, ticks_per_sec, 50);
    qemu_mod_timer(zx_ulatimer, next_time);

    zx_ula_do_retrace();
}

CPUState *zx_env;

void zx_timer_init(DisplayState *ds) {
    /* FIXME */

    int64_t t = qemu_get_clock(vm_clock);
    zx_ulatimer = qemu_new_timer(vm_clock, zx_50hz_timer, zx_env);
    qemu_mod_timer(zx_ulatimer, t);
}

static const uint8_t keycodes[128] = {
    0x00, 0x00, 0x31, 0x32, 0x33, 0x34, 0x35, 0x45,
      0x44, 0x43, 0x42, 0x41, 0x00, 0x00, 0x00, 0x00,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x55, 0x54, 0x53,
      0x52, 0x51, 0x00, 0x00, 0x61, 0x72, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x65, 0x64, 0x63, 0x62, 0x00,
      0x00, 0x00, 0x01, 0x00, 0x02, 0x03, 0x04, 0x05,
    0x75, 0x74, 0x73, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void zx_put_keycode(void *opaque, int keycode) {
    int release = keycode & 0x80;
    keycode &= 0x7f;

    //printf("Keycode %d (%s)\n", keycode, release? "release" : "press");

    keycode = keycodes[keycode];

    if (keycode) {
        int row = keycode >> 4;
        int col = 1 << ((keycode & 0xf) - 1);
        if (release) {
            keystate[row] |= col;
        } else {
            keystate[row] &= ~col;
        }
    }
}

static void zx_keyboard_init()
{
    int i;
    for (i=0; i<8; i++) {
        keystate[i] = 0xff;
    }
    qemu_add_kbd_event_handler(zx_put_keycode, NULL);
}

/* ZX Spectrum initialisation */
static void zx_init1(int ram_size, int vga_ram_size,
                     const char *boot_device, DisplayState *ds,
                     const char *kernel_filename, const char *kernel_cmdline,
                     const char *initrd_filename, const char *cpu_model)
{
    char buf[1024];
    int ret;
    ram_addr_t ram_addr, vga_ram_addr, rom_offset;
    int rom_size;
    CPUState *env;

    /* init CPUs */
    if (!cpu_model)
        cpu_model = "z80";
    env = cpu_init(cpu_model);
    zx_env = env; // XXX
    register_savevm("cpu", 0, 4, cpu_save, cpu_load, env);
    qemu_register_reset(main_cpu_reset, env);

    /* allocate RAM */
    cpu_register_physical_memory(0x4000, 0x10000 - 0x4000, IO_MEM_RAM);

    /* ROM load */
    snprintf(buf, sizeof(buf), "%s/%s", bios_dir, ROM_FILENAME);
    rom_size = get_image_size(buf);
    if (rom_size <= 0 ||
        (rom_size % 16384) != 0) {
        goto rom_error;
    }
//    rom_offset = qemu_ram_alloc(rom_size);
    rom_offset = 0x10000;
    ret = load_image(buf, phys_ram_base + rom_offset);

    if (ret != rom_size) {
    rom_error:
        fprintf(stderr, "qemu: could not load ZX Spectrum ROM '%s'\n", buf);
        exit(1);
    }

    /* hack from xz80 adding HALT to the keyboard input loop to save CPU */
    static unsigned char oldip[]={253,203,1,110,200,58,8,92,253,203,1,174};
    static unsigned char newip[]={33,59,92,118,203,110,200,58,8,92,203,174};
    if(!memcmp(phys_ram_base + rom_offset + 0x10b0,oldip,12))
        memcpy(phys_ram_base + rom_offset + 0x10b0,newip,12);

    cpu_register_physical_memory(0x0000, 0x4000, rom_offset | IO_MEM_ROM);

    /* map entire I/O space */
    register_ioport_read(0, 0x10000, 1, io_spectrum_read, NULL);

    zx_ula_init(ds, phys_ram_base + ram_size, ram_size);

    zx_keyboard_init();
    zx_timer_init(ds);

#ifdef CONFIG_LIBSPECTRUM
    /* load a snapshot */
    if(kernel_filename) {
        libspectrum_id_t type;
        libspectrum_class_t cls;
        libspectrum_snap* snap;
        uint8_t* snapmem;
        libspectrum_byte* page;
        int length;
        int i;
        if(libspectrum_init() != LIBSPECTRUM_ERROR_NONE ||
           libspectrum_identify_file(&type, kernel_filename, NULL, 0) != LIBSPECTRUM_ERROR_NONE ||
           libspectrum_identify_class(&cls, type) != LIBSPECTRUM_ERROR_NONE ||
           libspectrum_snap_alloc(&snap) != LIBSPECTRUM_ERROR_NONE) {
            fprintf(stderr, "%s: libspectrum error\n", __FUNCTION__);
            exit(1);
        }
        if(cls != LIBSPECTRUM_CLASS_SNAPSHOT) {
            fprintf(stderr, "%s: %s is not a snapshot\n", __FUNCTION__, kernel_filename);
            exit(1);
        }
        snapmem = malloc(65536);
        length = load_image(kernel_filename, snapmem);
        //printf("loaded %d bytes from %s\n",length, kernel_filename);
        if(libspectrum_snap_read(snap, snapmem, length, type, NULL) != LIBSPECTRUM_ERROR_NONE) {
            fprintf(stderr, "%s: failed to load snapshot %s\n", __FUNCTION__, kernel_filename);
            exit(1);
        }
        //printf("snap pc = %d\n",libspectrum_snap_pc(snap));

        /* fill memory */
        page = libspectrum_snap_pages(snap, 5);
        for(i = 0x4000; i < 0x8000; i++) {
            //printf("storing 0x%x in 0x%x\n",page[i-0x4000],i);
            stb_phys(i, page[i - 0x4000]);
        }
        page = libspectrum_snap_pages(snap, 2);
        for(i = 0x8000; i < 0xc000; i++)
            stb_phys(i, page[i - 0x8000]);
        page = libspectrum_snap_pages(snap, 0);
        for(i = 0xc000; i < 0x10000; i++)
            stb_phys(i, page[i - 0xc000]);

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

        free(snapmem);
    }
#endif
}

static void zx_spectrum_init(int ram_size, int vga_ram_size,
                             const char *boot_device, DisplayState *ds,
                             const char *kernel_filename,
                             const char *kernel_cmdline,
                             const char *initrd_filename,
                             const char *cpu_model)
{
    zx_init1(ram_size, vga_ram_size, boot_device, ds,
             kernel_filename, kernel_cmdline,
             initrd_filename, cpu_model);
}

QEMUMachine zxspec_machine = {
    "zxspec",
    "ZX Spectrum",
    zx_spectrum_init,
};
