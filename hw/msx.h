/*
 * QEMU MSX Emulator
 *
 * Copyright (c) 2009 Juha Riihimäki
 *
 * This code is licensed under the GPL version 2
 */

//#define DEBUG_MSX

#ifdef DEBUG_MSX
#define TRACE(fmt, ...) fprintf(stderr, "%s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#else
#define TRACE(...)
#endif

/* msx_mmu.c */
void *msx_mmu_init(CPUState *cpu, int ramslot);
void msx_mmu_reset(void *opaque);
void msx_mmu_load_rom(void *opaque, int addr, int slot,
                      const char *filename, int rom_size);
void msx_mmu_load_cartridge(void *opaque, int slot, const char *filename);
void msx_mmu_slot_select(void *opaque, uint32_t value);


/* v9918.c */
void *v9918_init(qemu_irq irq);
void v9918_reset(void *opaque);
uint32_t v9918_read(void *opaque, uint32_t addr);
void v9918_write(void *opaque, uint32_t addr, uint32_t value);
void v9918_change_zoom(void *opaque);
