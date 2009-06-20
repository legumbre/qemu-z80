/*
 * MSX memory mappers emulation
 *
 * Copyright (c) 2009 Juha Riihimäki
 *
 * This code is licensed under the GPL version 2
 */
#include "sysemu.h"
#include "msx.h"

#define ADDRSPACE 0x10000
#define SLOT_PAGESIZE 0x4000
#define CART_PAGESIZE 0x2000
#define SLOT_NUMPAGES (ADDRSPACE / SLOT_PAGESIZE)
#define CART_NUMPAGES (ADDRSPACE / CART_PAGESIZE)
#define CARTPAGES_PER_SLOTPAGE (SLOT_PAGESIZE / CART_PAGESIZE)
#define NUMSLOTS 4

typedef struct {
    CPUState *cpu;
    struct {
        ram_addr_t page[SLOT_NUMPAGES];
    } slot[NUMSLOTS];
    int slot_for_page[SLOT_NUMPAGES];
} MMUState;

static void msx_mmu_map(void *opaque, int addr, int slot)
{
    MMUState *s = (MMUState *)opaque;
    cpu_register_physical_memory(addr, SLOT_PAGESIZE,
                                 s->slot[slot].page[addr / SLOT_PAGESIZE]);
}

static inline ram_addr_t msx_mmu_alloc_page(uint32_t size)
{
    ram_addr_t page = qemu_ram_alloc(size);
    //memset(qemu_get_ram_ptr(page), 0xff, size);
    return page;
}

void msx_mmu_load_rom(void *opaque, int addr, int slot,
                      const char *filename, int rom_size)
{
    char *path = qemu_find_file(QEMU_FILE_TYPE_BIOS, filename);
    if (!path) {
        hw_error("%s: unable to locate MSX ROM '%s'\n", __FUNCTION__,
                 filename);
    }
    if (rom_size > 0) {
        if (get_image_size(path) != rom_size) {
            hw_error("%s: invalid size for MSX ROM '%s'\n", __FUNCTION__,
                     path);
        }
    } else {
        rom_size = get_image_size(path);
    }
    addr &= ~(SLOT_PAGESIZE - 1);
    TRACE("loading '%s' in slot %d, starting at 0x%04x (size %d)",
          path, slot, addr, rom_size);
    MMUState *s = (MMUState *)opaque;
    int page = addr / SLOT_PAGESIZE;
    int i;
    for (i = 0; i < rom_size; i += SLOT_PAGESIZE, page++) {
        if (s->slot[slot].page[page] != IO_MEM_UNASSIGNED) {
            hw_error("%s: page %d in slot %d is already assigned\n",
                     __FUNCTION__, page, slot);
        }
        s->slot[slot].page[page] = msx_mmu_alloc_page(SLOT_PAGESIZE)
                                   | IO_MEM_ROM;
        msx_mmu_map(s, i, slot);
    }
    if (load_image_targphys(path, addr, rom_size) != rom_size) {
        hw_error("%s: unable to load MSX ROM '%s'\n", __FUNCTION__, path);
    }
    qemu_free(path);
}

static void msx_mmu_load_plain_cartridge(void *opaque, int slot, int fd,
                                         uint32_t rom_size)
{
    ram_addr_t page[SLOT_NUMPAGES];
    MMUState *s = (MMUState *)opaque;
    switch (rom_size / CART_PAGESIZE) {
        case 0 ... 2:
            page[0] = msx_mmu_alloc_page(SLOT_PAGESIZE) | IO_MEM_ROM;
            page[1] = page[0];
            page[2] = page[0];
            page[3] = page[0];
            break;
        case 3 ... 4:
            page[0] = msx_mmu_alloc_page(SLOT_PAGESIZE) | IO_MEM_ROM;
            page[1] = page[0];
            page[2] = msx_mmu_alloc_page(SLOT_PAGESIZE) | IO_MEM_ROM;
            page[3] = page[2];
            break;
        case 5 ... 7:
            page[0] = msx_mmu_alloc_page(SLOT_PAGESIZE) | IO_MEM_ROM;
            page[1] = msx_mmu_alloc_page(SLOT_PAGESIZE) | IO_MEM_ROM;
            page[2] = msx_mmu_alloc_page(SLOT_PAGESIZE) | IO_MEM_ROM;
            page[3] = msx_mmu_alloc_page(SLOT_PAGESIZE) | IO_MEM_ROM;
            break;
    }
    int i;
    for (i = 0; i < SLOT_NUMPAGES; i++) {
        s->slot[slot].page[i] = page[i];
        msx_mmu_map(s, i * SLOT_PAGESIZE, slot);
    }
    int loadcount = 0;
    switch (rom_size / CART_PAGESIZE) {
        case 0 ... 2:
        case 5 ... 7:
            loadcount = read_targphys(fd, 0, rom_size);
            break;
        case 3 ... 4:
            loadcount = read_targphys(fd, SLOT_PAGESIZE, rom_size);
            break;
    }
    if (loadcount != rom_size) {
        hw_error("%s: could not read %d bytes\n", __FUNCTION__, rom_size);
    }
    if (rom_size / CART_PAGESIZE < CARTPAGES_PER_SLOTPAGE) {
        /* mirror sub-slotpagesize cart */
        void *src = qemu_get_ram_ptr(page[0]);
        void *dst = src + CART_PAGESIZE;
        int i = CARTPAGES_PER_SLOTPAGE - 1;
        for (; i--; dst += CART_PAGESIZE) {
            memcpy(dst, src, CART_PAGESIZE);
        }
    }
}

static int msx_is_mega_cartridge(int fd, uint32_t size)
{
    int result = 0;
    volatile char signature[2];
    if (size >= 0x10000 && lseek(fd, 0, SEEK_SET) >=0 &&
        read(fd, (char *)signature, 2) == 2 &&
        ((signature[0] == 'A' && signature[1] == 'B') ||
         (lseek(fd, size - 0x4000, SEEK_SET) >= 0 &&
          read(fd, (char *)signature, 2) == 2 &&
          signature[0] == 'A' && signature[1] == 'B'))) {
        result = 1;
    }
    lseek(fd, 0, SEEK_SET);
    return result;
}

void msx_mmu_load_cartridge(void *opaque, int slot, const char *filename)
{
    MMUState *s = (MMUState *)opaque;
    int i;
    for (i = 0; i < SLOT_NUMPAGES; i++) {
        if (s->slot[slot].page[i] != IO_MEM_UNASSIGNED) {
            hw_error("%s: slot %d is already occupied\n",
                     __FUNCTION__, slot);
        }
    }
    uint32_t rom_size = get_image_size(filename);
    TRACE("loading '%s' in slot %d (size %d)", filename, slot, rom_size);
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        hw_error("%s: unable to open '%s' for reading\n", __FUNCTION__,
                 filename);
    }
    if (msx_is_mega_cartridge(fd, rom_size)) {
        hw_error("%s: megarom cartridges are not supported\n", __FUNCTION__);
    } else {
        msx_mmu_load_plain_cartridge(opaque, slot, fd, rom_size);
    }
    close(fd);
}

void msx_mmu_reset(void *opaque)
{
    MMUState *mmu = (MMUState *)opaque;
    int page;
    for (page = 0; page < SLOT_NUMPAGES; page++) {
        msx_mmu_map(mmu, page * SLOT_PAGESIZE, 0);
    }
}

void msx_mmu_slot_select(void *opaque, uint32_t value)
{
    MMUState *s = (MMUState *)opaque;
    int page;
    for (page = 0; page < SLOT_NUMPAGES; page++, value >>= 2) {
        int slot = value & 3;
        if (s->slot_for_page[page] != slot) {
            msx_mmu_map(s, page * SLOT_PAGESIZE, slot);
            s->slot_for_page[page] = slot;
        }
    }
}

void *msx_mmu_init(CPUState *cpu, int ramslot)
{
    MMUState *s = qemu_mallocz(sizeof(*s));
    s->cpu = cpu;
    int page, slot;
    for (slot = 0; slot < NUMSLOTS; slot++) {
        for (page = 0; page < SLOT_NUMPAGES; page++) {
            s->slot[slot].page[page] = (slot == ramslot)
            ? (msx_mmu_alloc_page(SLOT_PAGESIZE) | IO_MEM_RAM)
            : IO_MEM_UNASSIGNED;
        }
    }
    msx_mmu_reset(s);
    return s;
}
