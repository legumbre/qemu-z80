/*
 * QEMU SAM Coupé Spectrum Emulator
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
#include "sam_video.h"
#include "sam_keyboard.h"
#include "boards.h"

#ifdef CONFIG_LIBSPECTRUM
#include <libspectrum.h>
#endif

#define ROM_FILENAME "sam-rom.bin"

#ifdef DEBUG_SAM_COUPE
#define DPRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

static CPUState *sam_env;

static int page_tab[4];

static int lmpr = 0x00;
static int hmpr = 0x02;
static int vmpr = 0x00;

static target_ulong sam_mapaddr(target_ulong addr) {
    return (page_tab[addr >> 14] << 14) | (addr & ~(~0 << 14));
}

static void map_memory(void)
{
    if (!(lmpr & 0x20)) {
        page_tab[0] = 0x20; /* ROM 0 */
    } else {
        page_tab[0] = lmpr & 0x1f;
    }
    page_tab[1] = (lmpr + 1) & 0x1f;

    page_tab[2] = hmpr & 0x1f;
    if (lmpr & 0x40) {
        page_tab[3] = 0x21; /* ROM 1 */
    } else {
        page_tab[3] = (hmpr + 1) & 0x1f;
    }

    /* TODO: if lmpr bit 7 is set, page 0 is write-protected */

    cpu_interrupt(first_cpu, CPU_INTERRUPT_EXITTB);
    tlb_flush(first_cpu, 1);
}

static uint32_t io_pen_read(void *opaque, uint32_t addr)
{
    DPRINTF("read from %s\n", __func__);
    return 0xff; /* not implemented */
}

static void io_clut_write(void *opaque, uint32_t addr, uint32_t data)
{
    DPRINTF("write %02x to %s\n", data, __func__);
    /* not implemented */
}

static uint32_t io_status_read(void *opaque, uint32_t addr)
{
    DPRINTF("read from %s\n", __func__);
    return 0xf7 & (sam_keyboard_read(opaque, addr) | ~0xe0);
}

static void io_lineirq_write(void *opaque, uint32_t addr, uint32_t data)
{
    DPRINTF("write %02x to %s\n", data, __func__);
    /* not implemented */
}

static uint32_t io_lmpr_read(void *opaque, uint32_t addr)
{
    DPRINTF("read from %s\n", __func__);
    return lmpr;
}

static void io_lmpr_write(void *opaque, uint32_t addr, uint32_t data)
{
    DPRINTF("write %02x to %s\n", data, __func__);
    lmpr = data;
    map_memory();
}

static uint32_t io_hmpr_read(void *opaque, uint32_t addr)
{
    DPRINTF("read from %s\n", __func__);
    return hmpr;
}

static void io_hmpr_write(void *opaque, uint32_t addr, uint32_t data)
{
    DPRINTF("write %02x to %s\n", data, __func__);
    hmpr = data;
    map_memory();
}

static uint32_t io_vmpr_read(void *opaque, uint32_t addr)
{
    DPRINTF("read from %s\n", __func__);
    return vmpr;
}

static void io_vmpr_write(void *opaque, uint32_t addr, uint32_t data)
{
    DPRINTF("write %02x to %s\n", data, __func__);
    vmpr = data;
    /* not implemented */
}

static uint32_t io_midi_read(void *opaque, uint32_t addr)
{
    DPRINTF("read from %s\n", __func__);
    return 0xff; /* not implemented */
}

static void io_midi_write(void *opaque, uint32_t addr, uint32_t data)
{
    DPRINTF("write %02x to %s\n", data, __func__);
    /* not implemented */
}

static uint32_t io_attr_read(void *opaque, uint32_t addr)
{
    DPRINTF("read from %s\n", __func__);
    return 0xff; /* not implemented */
}

static void io_sound_write(void *opaque, uint32_t addr, uint32_t data)
{
    DPRINTF("write %02x to %s\n", data, __func__);
    /* not implemented */
}

static uint32_t io_ula_read(void *opaque, uint32_t addr)
{
    return sam_keyboard_read(opaque, addr) | ~0x1f;
}

static void io_ula_write(void *opaque, uint32_t addr, uint32_t data)
{
    sam_video_set_border(data & 0x7);
}

static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;
    cpu_reset(env);

    page_tab[0] = 0x20 + 0; /* ROM 0 */
    page_tab[1] = 0x01;     /* RAM 1 */

    page_tab[2] = 0x02;     /* RAM 2 */
    page_tab[3] = 0x03;     /* RAM 3 */
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

    sam_video_do_retrace();
}

static void zx_timer_init(void)
{
    int64_t t = qemu_get_clock(vm_clock);
    zx_ula_timer = qemu_new_timer(vm_clock, zx_50hz_timer, sam_env);
    qemu_mod_timer(zx_ula_timer, t);
}

/* ZX Spectrum initialisation */
static void sam_coupe_init(ram_addr_t ram_size,
                           const char *boot_device,
                           const char *kernel_filename,
                           const char *kernel_cmdline,
                           const char *initrd_filename,
                           const char *cpu_model)
{
    char *filename;
    int ret;
    ram_addr_t ram_offset, rom_offset;
    int rom_size;
    int rom_base;
    CPUState *env;
    int i;

    /* init CPUs */
    if (!cpu_model) {
        cpu_model = "z80";
    }
    env = cpu_init(cpu_model);
    sam_env = env; // XXX
    register_savevm("cpu", 0, 4, cpu_save, cpu_load, env);
    qemu_register_reset(main_cpu_reset, 0, env);
    main_cpu_reset(env);

    env->mapaddr = sam_mapaddr;

    ram_size = 0x80000; // 512 K

    /* allocate RAM */
    ram_offset = qemu_ram_alloc(ram_size);
    cpu_register_physical_memory(0, ram_size, ram_offset | IO_MEM_RAM);

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
    rom_base = ram_size;
    rom_offset = qemu_ram_alloc(rom_size);
    cpu_register_physical_memory(rom_base, rom_size, rom_offset | IO_MEM_ROM);
    ret = load_image_targphys(filename, rom_base, rom_size);
    if (ret != rom_size) {
    rom_error:
        fprintf(stderr, "qemu: could not load SAM Coupe ROM '%s'\n",
                        bios_name);
        exit(1);
    }
    if (filename) {
        qemu_free(filename);
    }

    /* map entire I/O space */
    for (i = 0; i < 0x10000; i += 0x100) {
        register_ioport_read(i + 0xf8, 1, 1, io_pen_read, NULL);
        register_ioport_read(i + 0xf9, 1, 1, io_status_read, NULL);
        register_ioport_read(i + 0xfa, 1, 1, io_lmpr_read, NULL);
        register_ioport_read(i + 0xfb, 1, 1, io_hmpr_read, NULL);
        register_ioport_read(i + 0xfc, 1, 1, io_vmpr_read, NULL);
        register_ioport_read(i + 0xfd, 1, 1, io_midi_read, NULL);
        register_ioport_read(i + 0xfe, 1, 1, io_ula_read, NULL);
        register_ioport_read(i + 0xff, 1, 1, io_attr_read, NULL);
        register_ioport_write(i + 0xf8, 1, 1, io_clut_write, NULL);
        register_ioport_write(i + 0xf9, 1, 1, io_lineirq_write, NULL);
        register_ioport_write(i + 0xfa, 1, 1, io_lmpr_write, NULL);
        register_ioport_write(i + 0xfb, 1, 1, io_hmpr_write, NULL);
        register_ioport_write(i + 0xfc, 1, 1, io_vmpr_write, NULL);
        register_ioport_write(i + 0xfd, 1, 1, io_midi_write, NULL);
        register_ioport_write(i + 0xfe, 1, 1, io_ula_write, NULL);
        register_ioport_write(i + 0xff, 1, 1, io_sound_write, NULL);
    }

    sam_video_init(ram_offset);

    sam_keyboard_init();
    zx_timer_init();
}

static QEMUMachine sam_machine = {
    .name = "sam",
    .desc = "SAM Coupe",
    .init = sam_coupe_init,
    .is_default = 0,
};

static void sam_machine_init(void) {
    qemu_register_machine(&sam_machine);
}

machine_init(sam_machine_init);
