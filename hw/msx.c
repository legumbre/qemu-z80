/*
 * QEMU MSX Emulator
 *
 * Copyright (c) 2009 Juha Riihimäki
 *
 * This code is licensed under the GPL version 2
 */
#include "hw.h"
#include "sysemu.h"
#include "boards.h"
#include "devices.h"
#include "isa.h"
#include "console.h"
#include "msx.h"

typedef struct {
    void *mmu;
    void *vdp;
    uint8_t port[3];
    uint8_t control;
    uint8_t keystate[16];
} PPIState;

#define PPI_GROUP_A_MODE ((s->control & 0x60) >> 5)
#define PPI_GROUP_B_MODE ((s->control & 0x04) >> 2)
#define PPI_PORT_A_INPUT (s->control & 0x10)
#define PPI_PORT_B_INPUT (s->control & 0x02)
#define PPI_PORT_C_LSB_INPUT (s->control & 0x01)
#define PPI_PORT_C_MSB_INPUT (s->control & 0x08)

static uint32_t ppi_read(void *opaque, uint32_t addr)
{
    uint32_t result = 0;
    PPIState *s = (PPIState *)opaque;
    switch (addr & 3) {
        case 0: /* port A */
            if (PPI_GROUP_A_MODE == 0) {
                result = s->port[0];
            } else {
                /* TODO: other modes */
            }
            break;
        case 1: /* port B */
            if (PPI_GROUP_B_MODE == 0) {
                if (PPI_PORT_B_INPUT && !PPI_PORT_C_LSB_INPUT) {
                    s->port[1] = s->keystate[s->port[2] & 0x0f];
                }
                result = s->port[1];
            } else {
                /* TODO: other modes */
            }
            break;
        case 2: /* port C */
            if (PPI_GROUP_B_MODE == 0) {
                result |= s->port[2] & 0x0f;
            } else {
                /* TODO: other modes */
            }
            if (PPI_GROUP_A_MODE == 0) {
                result |= s->port[2] & 0xf0;
            } else {
                /* TODO: other modes */
            }
            break;
        case 3: /* control */
            result = s->control;
            break;
    }
    return result;
}

static void ppi_write(void *opaque, uint32_t addr, uint32_t value)
{
    PPIState *s = (PPIState *)opaque;
    switch (addr & 3) {
        case 0: /* port A */
            if (PPI_GROUP_A_MODE == 0) {
                if (!PPI_PORT_A_INPUT) {
                    s->port[0] = value;
                    msx_mmu_slot_select(s->mmu, value);
                }
            } else {
                /* TODO: other modes */
            }
            break;
        case 1: /* port B */
            if (PPI_GROUP_B_MODE == 0) {
                if (!PPI_PORT_B_INPUT) {
                    s->port[1] = value;
                }
            }
            break;
        case 2: /* port C */
            if (PPI_GROUP_B_MODE == 0) {
                if (!PPI_PORT_C_LSB_INPUT) {
                    s->port[2] = (s->port[2] & 0xf0) | (value & 0x0f);
                }
            } else {
                /* TODO: other modes */
            }
            if (PPI_GROUP_A_MODE == 0) {
                if (!PPI_PORT_C_MSB_INPUT) {
                    s->port[2] = (s->port[2] & 0x0f) | (value & 0xf0);
                }
            }
            break;
        case 3: /* control */
            if (value & 0x80) {
                s->control = value;
            } else {
                uint8_t bit = ((value >> 1) & 7);
                if ((bit < 4 && !PPI_GROUP_B_MODE && !PPI_PORT_C_LSB_INPUT) ||
                    (bit > 3 && !PPI_GROUP_A_MODE && !PPI_PORT_C_MSB_INPUT)) {
                    if (value & 1) {
                    s->port[2] |= 1 << bit;
                    } else {
                    s->port[2] &= ~(1 << bit);
                    }
                } else {
                    /* TODO: other modes */
                }
            }
            break;
    }
}

/* all host keys mapped to corresponding msx key plus these extras:
 * left alt ...... msx "code"
 * right alt ..... msx "graph"
 * num lock ...... msx "stop"
 * scroll lock ... msx "stop"
 * end ........... msx "select"
 * page up ....... msx "insert"
 * page down ..... msx "delete"
 */
static const int ppi_keymap[] = {
/* SDLi, row, column */
      1,  7, 2, /* escape */
      2,  0, 1, /* 1 */
      3,  0, 2, /* 2 */
      4,  0, 3, /* 3 */
      5,  0, 4, /* 4 */
      6,  0, 5, /* 5 */
      7,  0, 6, /* 6 */
      8,  0, 7, /* 7 */
      9,  1, 0, /* 8 */
     10,  1, 1, /* 9 */
     11,  0, 0, /* 0 */
     12,  1, 2, /* minus */
     13,  1, 3, /* equals */
     14,  7, 5, /* backspace */
     15,  7, 3, /* tab */
     16,  4, 6, /* q */
     17,  5, 4, /* w */
     18,  3, 2, /* e */
     19,  4, 7, /* r */
     20,  5, 1, /* t */
     21,  5, 6, /* y */
     22,  5, 2, /* u */
     23,  3, 6, /* i */
     24,  4, 4, /* o */
     25,  4, 5, /* p */
     26,  1, 5, /* left bracket */
     27,  1, 6, /* right bracket */
     28,  7, 7, /* enter */
     29,  6, 1, /* left control */
     30,  2, 6, /* a */
     31,  5, 0, /* s */
     32,  3, 1, /* d */
     33,  3, 3, /* f */
     34,  3, 4, /* g */
     35,  3, 5, /* h */
     36,  3, 7, /* j */
     37,  4, 0, /* k */
     38,  4, 1, /* l */
     39,  1, 7, /* semicolon */
     40,  2, 0, /* quote */
     41,  2, 1, /* backquote */
     42,  6, 0, /* left shift */
     43,  4, 1, /* backslash */
     44,  5, 7, /* z */
     45,  5, 5, /* x */
     46,  3, 0, /* c */
     47,  5, 3, /* v */
     48,  2, 7, /* b */
     49,  4, 3, /* n */
     50,  4, 2, /* m */
     51,  2, 2, /* comma */
     52,  2, 3, /* period */
     53,  2, 4, /* slash */
     54,  6, 0, /* right shift */
     55,  9, 0, /* kp mul */
     56,  6, 4, /* left alt == CODE */
     57,  8, 0, /* space */
     58,  6, 3, /* caps lock */
     59,  6, 5, /* f1 */
     60,  6, 6, /* f2 */
     61,  6, 7, /* f3 */
     62,  7, 0, /* f4 */
     63,  7, 1, /* f5 */
    /* 64 - 68 = f6 - f10 --> not in msx keyboard */
     69,  7, 4, /* num lock == STOP */
     70,  7, 4, /* scroll lock == STOP */
     71, 10, 2, /* kp 7 */
     72, 10, 3, /* kp 8 */
     73, 10, 4, /* kp 9 */
     74, 10, 5, /* kp minus */
     75,  9, 7, /* kp 4 */
     76, 10, 0, /* kp 5 */
     77, 10, 1, /* kp 6 */
     78,  9, 1, /* kp plus */
     79,  9, 4, /* kp 1 */
     80,  9, 5, /* kp 2 */
     81,  9, 6, /* kp 3 */
     82,  9, 3, /* kp 0 */
     83, 10, 7, /* kp . */

    0xe01d, 6, 1, /* right control */
    0xe035, 9, 2, /* kp div */
    0xe038, 6, 2, /* right alt == GRAPH */
    0xe047, 8, 1, /* home */
    0xe048, 8, 5, /* up */
    0xe049, 8, 2, /* page up == INSERT */
    0xe04b, 8, 4, /* left */
    0xe04d, 8, 7, /* right */
    0xe04f, 7, 6, /* end == SELECT */
    0xe050, 8, 6, /* down */
    0xe051, 8, 2, /* page down == DELETE */
    0xe052, 8, 2, /* insert */
    0xe053, 8, 3, /* delete */

/* TODO: dead */
-1
};

static void ppi_key_event(void *opaque, int keycode)
{
    static int extcode = 0;
    
    PPIState *s = (PPIState *)opaque;
    if (keycode == 0xe0) {
        extcode = keycode << 8;
        return;
    }
    int press = !(keycode & 0x80);
    keycode &= 0x7f;
    if (extcode) {
        keycode |= extcode;
        extcode = 0;
    }
    const int *p = ppi_keymap;
    for (; *p >= 0; p += 3) {
        if (*p == keycode) {
            if (press) {
                s->keystate[p[1]] &= ~(1 << p[2]);
            } else {
                s->keystate[p[1]] |= 1 << p[2];
            }
            break;
        }
    }
    if (keycode == 64 && !press) { /* F6 */
        v9918_change_zoom(s->vdp);
    }
}

static void ppi_reset(void *opaque)
{
    PPIState *s = (PPIState *)opaque;
    s->port[0] = 0;
    s->port[1] = 0;
    s->port[2] = 0;
    s->control = 0x9b; /* all ports input, mode 0 */
    memset(s->keystate, 0xff, sizeof(s->keystate));
}

static void *ppi_init(void *mmu, void *vdp)
{
    PPIState *s = qemu_mallocz(sizeof(*s));
    s->mmu = mmu;
    s->vdp = vdp;
    ppi_reset(s);
    qemu_add_kbd_event_handler(ppi_key_event, s);
    return s;
}

typedef struct {
    uint8_t reg;

    uint8_t enable;
    
    struct {
        int period;
        int amplitude;
        int fixed_amp;
    } tone[3];
    int noise_period;
    int envelope_period;
    int envelope_shape;
    
    uint8_t stick;
    uint8_t stickstate[2];
} PSGState;

static uint32_t psg_read(void *opaque, uint32_t addr)
{
    uint32_t result = 0xff;
    if (addr == 2) {
        PSGState *s = (PSGState *)opaque;
        switch (s->reg) {
            case 0 ... 5: /* tone generator control */
                if (s->reg & 1) {
                    result = s->tone[s->reg >> 1].period >> 8;
                } else {
                    result = s->tone[s->reg >> 1].period & 0xff;
                }
                break;
            case 6: /* noise generator control */
                result = s->noise_period;
                break;
            case 7: /* mixer control / io enable */
                result = s->enable;
                break;
            case 8 ... 10: /* amplitude control */
                result = (s->tone[s->reg - 8].fixed_amp ? 0 : 0x10) |
                         s->tone[s->reg - 8].amplitude;
                break;
            case 11: /* envelope period control lsb */
                result = s->envelope_period & 0xff;
                break;
            case 12: /* envelope period control msb */
                result = s->envelope_period >> 8;
                break;
            case 13: /* envelope shape/cycle control */
                result = s->envelope_shape;
                break;
            case 14: /* io port a */
                if (s->enable & 0x40) {
                    /* TODO: io port a input mode operation */
                } else {
                    result = s->stickstate[s->stick] & 0x3f;
                }
                break;
            case 15: /* io port b */
                if (s->enable & 0x80) {
                    result = (s->stick << 6) | 0x8f;
                } else {
                    /* TODO: io port b output mode operation */
                }
                break;
            default:
                break;
        }
    }
    return result;
}

static void psg_write(void *opaque, uint32_t addr, uint32_t value)
{
    PSGState *s = (PSGState *)opaque;
    switch (addr) {
        case 0:
            s->reg = value & 0x0f;
            break;
        case 1:
            switch (s->reg) {
                case 0 ... 5: /* tone generator control */
                    {
                        int n = s->reg >> 1;
                        int p = s->tone[n].period;
                        if (s->reg & 1) {
                            p = (p & 0xff) | ((value & 0x0f) << 8);
                        } else {
                            p = (p & ~0xff) | value;
                        }
                        s->tone[n].period = p;
                    }
                    break;
                case 6: /* noise generator control */
                    s->noise_period = value & 0x1f;
                    break;
                case 7: /* mixer control / io enable */
                    s->enable = value;
                    break;
                case 8 ... 10: /* amplitude control */
                    s->tone[s->reg - 8].fixed_amp = !(value & 0x10);
                    s->tone[s->reg - 8].amplitude = value & 0x0f;
                    break;
                case 11: /* envelope period control lsb */
                    s->envelope_period = (s->envelope_period & ~0xff) | value;
                    break;
                case 12: /* envelope period control msb */
                    s->envelope_period = (s->envelope_period & 0xff) | (value << 8);
                    break;
                case 13: /* envelope shape/cycle control */
                    s->envelope_shape = value & 0x0f;
                    break;
                case 14: /* io port a */
                    if (s->enable & 0x40) {
                        /* TODO: io port a input mode operation */
                    } else {
                        /* TODO: io port a output mode operation */
                    }
                    break;
                case 15: /* io port b */
                    if (s->enable & 0x80) {
                        s->stick = (value >> 6) & 1;
                    } else {
                        /* TODO: io port b output mode operation */
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void psg_reset(void *opaque)
{
    PSGState *s = (PSGState *)opaque;
    s->enable = 0xbd;
    int i;
    for (i = 0; i < 3; i++) {
        s->tone[i].period = 0;
        s->tone[i].amplitude = 0;
        s->tone[i].fixed_amp = 0;
    }
    s->noise_period = 0;
    s->envelope_period = 0;
    s->envelope_shape = 0;
    
    s->stick = 0;
    s->stickstate[0] = 0x3f;
    s->stickstate[1] = 0x3f;
}

static void *psg_init(void)
{
    PSGState *s = qemu_mallocz(sizeof(*s));
    psg_reset(s);
    return s;
}

typedef struct {
    CPUState *cpu;
    qemu_irq *irq;
    void *mmu;
    void *vdp;
    void *ppi;
    void *psg;
} MSXState;

static void msx_interrupt(void *opaque, int source, int level)
{
    if (level) {
        MSXState *s = (MSXState *)opaque;
        cpu_interrupt(s->cpu, CPU_INTERRUPT_HARD);
    }
}

static uint32_t msx_io_read(void *opaque, uint32_t addr)
{
    MSXState *s = (MSXState *)opaque;
    uint32_t result = 0xff;
    addr &= 0xff;
    switch (addr) {
        case 0x90 ... 0x91: /* parallel port */
            if (addr == 0x90) {
                result =  0xfd; /* printer ready signal */
            }
            break;
        case 0x98 ... 0x99: /* video */
            result = v9918_read(s->vdp, addr - 0x98);
            break;
        case 0xa0 ... 0xa2: /* audio */
            result = psg_read(s->psg, addr - 0xa0);
            break;
        case 0xa8 ... 0xab: /* peripheral interface */
            result = ppi_read(s->ppi, addr - 0xa8);
            break;
        default:
            fprintf(stderr, "%s: unknown port 0x%02x\n", __FUNCTION__, addr);
            break;
    }
    
    return result;
}

static void msx_io_write(void *opaque, uint32_t addr, uint32_t value)
{
    MSXState *s = (MSXState *)opaque;
    addr &= 0xff;
    switch (addr) {
        case 0x90 ... 0x91: /* parallel port */
            break;
        case 0x98 ... 0x99: /* video */
            v9918_write(s->vdp, addr - 0x98, value);
            break;
        case 0xa0 ... 0xa2: /* audio */
            psg_write(s->psg, addr - 0xa0, value);
            break;
        case 0xa8 ... 0xab: /* peripheral interface */
            ppi_write(s->ppi, addr - 0xa8, value);
            break;
        default:
            fprintf(stderr, "%s: unknown port 0x%02x (value 0x%02x)\n",
                    __FUNCTION__, addr, value);
            break;
    }
}

static void msx_reset(void *opaque)
{
    MSXState *s = (MSXState *)opaque;
    cpu_reset(s->cpu);
    msx_mmu_reset(s->mmu);
    ppi_reset(s->ppi);
    psg_reset(s->psg);
    v9918_reset(s->vdp);
}

static void msx_init(ram_addr_t ram_size,
                     const char *boot_device,
                     const char *kernel_filename,
                     const char *kernel_cmdfline,
                     const char *initrd_filename,
                     const char *cpu_model)
{
    MSXState *s = qemu_mallocz(sizeof(*s));
    if (!cpu_model) {
        cpu_model = "z80";
    }
    s->cpu = cpu_init(cpu_model);
    s->irq = qemu_allocate_irqs(msx_interrupt, s, 1);
    s->mmu = msx_mmu_init(s->cpu, 3);

    register_ioport_read(0, 0x10000, 1, msx_io_read, s);
    register_ioport_write(0, 0x10000, 1, msx_io_write, s);
    
    s->vdp = v9918_init(s->irq[0]);
    s->ppi = ppi_init(s->mmu, s->vdp);
    s->psg = psg_init();
    
    msx_mmu_load_rom(s->mmu, 0, 0, "msx.rom", 0x8000);
    
    if (kernel_filename && *kernel_filename) {
        msx_mmu_load_cartridge(s->mmu, 1, kernel_filename);
    }
    
    register_savevm("cpu", 0, 4, cpu_save, cpu_load, s->cpu);
    qemu_register_reset(msx_reset, 0, s);
    msx_reset(s);
}

static QEMUMachine msx_machine = {
    .name = "msx",
    .desc = "MSX",
    .init = msx_init,
};

static void msx_machine_init(void) {
    qemu_register_machine(&msx_machine);
}

machine_init(msx_machine_init);
