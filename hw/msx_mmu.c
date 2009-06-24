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

typedef struct MMUMegaCart MMUMegaCart;

typedef CPUWriteMemoryFunc **(*msx_mapper_fn_t)(MMUMegaCart *s, int cart_page);

typedef struct {
    int cart_pagenum;
    int phys_addr;
    int io_index;
    MMUMegaCart *megacart;
} MMUMegaCartMapper;

struct MMUMegaCart {
    int pagecount;
    ram_addr_t *pages;
    msx_mapper_fn_t mapper_fn;
    MMUMegaCartMapper mapper[CART_NUMPAGES];
};

typedef struct {
    CPUState *cpu;
    struct {
        ram_addr_t page[SLOT_NUMPAGES];
        MMUMegaCart *megacart;
    } slot[NUMSLOTS];
    int slot_for_page[SLOT_NUMPAGES];
} MMUState;

static uint32_t msx_dummy_read(void *opaque, target_phys_addr_t addr)
{
    hw_error("%s: oops, should not have come here...\n", __FUNCTION__);
    return 0;
}

static CPUReadMemoryFunc *msx_dummy_read_ops[] = {
    msx_dummy_read,
    msx_dummy_read,
    msx_dummy_read,
};

static void msx_mmu_megacart_map(MMUMegaCart *s, int addr)
{
    int cpage = addr / CART_PAGESIZE;
    int cartpnum = s->mapper[cpage].cart_pagenum;
    TRACE("[0x%04x] -> cart page %d [0x%08x]", cpage * CART_PAGESIZE, cartpnum,
          (cpage >= 0 && cpage < s->pagecount)
          ? cartpnum * CART_PAGESIZE : -1);
    if (cartpnum >= 0 && cartpnum < s->pagecount) {
        ram_addr_t ptr = s->pages[cartpnum];
        CPUWriteMemoryFunc **wf = s->mapper_fn(s, cpage);
        if (wf) {
            if (s->mapper[cpage].io_index < 0) {
                int io_index = cpu_register_io_memory(0, msx_dummy_read_ops,
                                                      wf, &s->mapper[cpage]);
                if (io_index < 0) {
                    hw_error("%s: ran out of io regions\n", __FUNCTION__);
                }
                s->mapper[cpage].io_index = io_index;
            }
            ptr |= s->mapper[cpage].io_index | IO_MEM_ROMD;
        } else {
            ptr |= IO_MEM_ROM;
        }
        cpu_register_physical_memory(cpage * CART_PAGESIZE, CART_PAGESIZE,
                                     ptr);
    }
}

static inline void msx_mmu_megacart_remap(MMUMegaCartMapper *m, int pnum)
{
    if (pnum != m->cart_pagenum) {
        m->cart_pagenum = pnum;
        cpu_register_physical_memory(m->phys_addr, CART_PAGESIZE,
                                     IO_MEM_UNASSIGNED);
        msx_mmu_megacart_map(m->megacart, m->phys_addr);
    }
}

static void msx_mmu_remap(void *opaque, int addr, int slot)
{
    cpu_register_physical_memory(addr & ~(SLOT_PAGESIZE - 1),
                                 SLOT_PAGESIZE,
                                 IO_MEM_UNASSIGNED);
    MMUState *s = (MMUState *)opaque;
    MMUMegaCart *mc = s->slot[slot].megacart;
    TRACE("[0x%04x] -> slot%d (%s)", addr, slot,
          mc ? "megacart"
          : s->slot[slot].page[addr / SLOT_PAGESIZE] == IO_MEM_UNASSIGNED
          ? "unmapped" : "mapped");
    if (mc) {
        int i;
        for (i = 0; i < CARTPAGES_PER_SLOTPAGE; i++, addr += CART_PAGESIZE) {
            msx_mmu_megacart_map(mc, addr);
        }
    } else {
        cpu_register_physical_memory(addr & ~(SLOT_PAGESIZE - 1),
                                     SLOT_PAGESIZE,
                                     s->slot[slot].page[addr / SLOT_PAGESIZE]);
    }
}

static void msx_konami_noscc_write(void *opaque,
                                   target_phys_addr_t addr,
                                   uint32_t value)
{
    if (!addr) {
        MMUMegaCartMapper *s = opaque;
        int new_pagenum = value & 0x1f;
        if (new_pagenum >= s->megacart->pagecount) {
            new_pagenum = -1;
        }
        msx_mmu_megacart_remap(s, new_pagenum);
    }
}

static CPUWriteMemoryFunc *msx_konami_noscc_write_ops[] = {
    msx_konami_noscc_write,
    msx_konami_noscc_write,
    msx_konami_noscc_write,
};

static CPUWriteMemoryFunc **msx_mapper_konami_noscc(MMUMegaCart *s,
                                                    int cart_page)
{
    return (cart_page > 2 && cart_page < 6) /* 0x6000-0xbfff */
        ? msx_konami_noscc_write_ops : NULL;
}

static void msx_konami_scc_write(void *opaque,
                                 target_phys_addr_t addr,
                                 uint32_t value)
{
    if (addr == 0x1000) {
        MMUMegaCartMapper *s = opaque;
        msx_mmu_megacart_remap(s, value % s->megacart->pagecount);
    }
}

static CPUWriteMemoryFunc *msx_konami_scc_write_ops[] = {
    msx_konami_scc_write,
    msx_konami_scc_write,
    msx_konami_scc_write,
};

static CPUWriteMemoryFunc **msx_mapper_konami_scc(MMUMegaCart *s,
                                                  int cart_page)
{
    return (cart_page > 1 && cart_page < 6) /* 0x4000-0xbfff */
        ? msx_konami_scc_write_ops : NULL;
}

static void msx_ascii_8k_write(void *opaque,
                               target_phys_addr_t addr,
                               uint32_t value)
{
    MMUMegaCartMapper *s = opaque;
    s = &s->megacart->mapper[((addr >> 11) & 3) + 2];
    msx_mmu_megacart_remap(s, value % s->megacart->pagecount);
}

static CPUWriteMemoryFunc *msx_ascii_8k_write_ops[] = {
    msx_ascii_8k_write,
    msx_ascii_8k_write,
    msx_ascii_8k_write,
};

static CPUWriteMemoryFunc **msx_mapper_ascii_8k(MMUMegaCart *s,
                                                int cart_page)
{
    return (cart_page == 3) /* 0x6000-0x7fff */
        ? msx_ascii_8k_write_ops : NULL;
}

static void msx_ascii_16k_write(void *opaque,
                                target_phys_addr_t addr,
                                uint32_t value)
{
    MMUMegaCartMapper *s = opaque;
    if (!(addr & 0x0800)) {
        int startp = 2 + ((addr >> 11) & 2);
        value = (value << 1) % s->megacart->pagecount;
        s = &s->megacart->mapper[startp];
        msx_mmu_megacart_remap(s, value);
        s = &s->megacart->mapper[startp + 1];
        msx_mmu_megacart_remap(s, value + 1);
    }
}

static CPUWriteMemoryFunc *msx_ascii_16k_write_ops[] = {
    msx_ascii_16k_write,
    msx_ascii_16k_write,
    msx_ascii_16k_write,
};

static CPUWriteMemoryFunc **msx_mapper_ascii_16k(MMUMegaCart *s,
                                                 int cart_page)
{
    return (cart_page == 3) /* 0x6000-0x7fff */
        ? msx_ascii_16k_write_ops : NULL;
}

#define MSX_NB_MAPPERS 4
static const msx_mapper_fn_t msx_mapper_fn_table[MSX_NB_MAPPERS] = {
    msx_mapper_konami_noscc,
    msx_mapper_konami_scc,
    msx_mapper_ascii_8k,
    msx_mapper_ascii_16k,
};

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
        msx_mmu_remap(s, i, slot);
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
        msx_mmu_remap(s, i * SLOT_PAGESIZE, slot);
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

static void msx_mmu_load_mega_cartridge(void *opaque, int slot, int fd,
                                        uint32_t rom_size, int req_mapper)
{
    if (req_mapper >= MSX_NB_MAPPERS) {
        req_mapper = 0;
    }
    MMUMegaCart *mc = qemu_mallocz(sizeof(*mc));
    mc->pagecount = (rom_size + CART_PAGESIZE - 1) / CART_PAGESIZE;
    mc->pages = qemu_mallocz(sizeof(ram_addr_t) * mc->pagecount);
    mc->mapper_fn = msx_mapper_fn_table[req_mapper];
    uint32_t i, j;
    int lastfirst = 1;
    for (i = 0, j = 0; i < rom_size; i += CART_PAGESIZE, j++) {
        cpu_register_physical_memory(0x4000, CART_PAGESIZE, IO_MEM_UNASSIGNED);
        ram_addr_t page = msx_mmu_alloc_page(CART_PAGESIZE);
        mc->pages[j] = page;
        cpu_register_physical_memory(0x4000, CART_PAGESIZE, page | IO_MEM_ROM);
        uint32_t count = (rom_size - i) > CART_PAGESIZE
                         ? CART_PAGESIZE : (rom_size - i);
        if (read_targphys(fd, 0x4000, count) != count) {
            hw_error("%s: error while reading cartridge file\n", __FUNCTION__);
        }
        if (!i) {
            uint8_t signature[2];
            cpu_physical_memory_read(0x4000, signature, 2);
            if (signature[0] == 'A' && signature[1] == 'B') {
                lastfirst = 0;
            }
        }
    }
    for (i = 0; i < CART_NUMPAGES; i++) {
        mc->mapper[i].cart_pagenum = -1;
        mc->mapper[i].phys_addr = i * CART_PAGESIZE;
        mc->mapper[i].io_index = -1;
        mc->mapper[i].megacart = mc;
    }
    if (lastfirst) {
        mc->mapper[2].cart_pagenum = mc->pagecount - 2;
        mc->mapper[3].cart_pagenum = mc->pagecount - 1;
        mc->mapper[4].cart_pagenum = mc->pagecount - 2;
        mc->mapper[5].cart_pagenum = mc->pagecount - 1;
    } else {
        mc->mapper[2].cart_pagenum = 0;
        mc->mapper[3].cart_pagenum = 1;
        mc->mapper[4].cart_pagenum = 2;
        mc->mapper[5].cart_pagenum = 3;
    }
    MMUState *s = (MMUState *)opaque;
    s->slot[slot].megacart = mc;
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
    if (s->slot[slot].megacart) {
        hw_error("%s: slot %d is already occupied\n",
                 __FUNCTION__, slot);
    }
    for (i = 0; i < SLOT_NUMPAGES; i++) {
        if (s->slot[slot].page[i] != IO_MEM_UNASSIGNED) {
            hw_error("%s: slot %d is already occupied\n",
                     __FUNCTION__, slot);
        }
    }
    const char *p = strchr(filename,',');
    int req_mapper = 0;
    if (p) {
        char *s = qemu_mallocz(p - filename + 1);
        strncpy(s, filename, p - filename);
        filename = s;
        req_mapper = atoi(p + 1);
    }
    uint32_t rom_size = get_image_size(filename);
    TRACE("loading '%s' in slot %d (size %d)", filename, slot, rom_size);
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        hw_error("%s: unable to open '%s' for reading\n", __FUNCTION__,
                 filename);
    }
    if (msx_is_mega_cartridge(fd, rom_size)) {
        msx_mmu_load_mega_cartridge(opaque, slot, fd, rom_size, req_mapper);
    } else {
        msx_mmu_load_plain_cartridge(opaque, slot, fd, rom_size);
    }
    close(fd);
    if (p) {
        qemu_free((void *)filename);
    }
}

void msx_mmu_reset(void *opaque)
{
    MMUState *mmu = (MMUState *)opaque;
    int page;
    for (page = 0; page < SLOT_NUMPAGES; page++) {
        msx_mmu_remap(mmu, page * SLOT_PAGESIZE, 0);
    }
}

void msx_mmu_slot_select(void *opaque, uint32_t value)
{
    MMUState *s = (MMUState *)opaque;
    int page;
    for (page = 0; page < SLOT_NUMPAGES; page++, value >>= 2) {
        int slot = value & 3;
        if (s->slot_for_page[page] != slot) {
            msx_mmu_remap(s, page * SLOT_PAGESIZE, slot);
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
