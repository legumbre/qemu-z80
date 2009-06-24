/*
 * Texas Instruments TMS9918A Video Display Processor emulation
 *
 * Copyright (c) 2009 Juha Riihimäki
 * Sprite code based on fMSX X11 screen drivers written by Arnold Metselaar
 *
 * This code is licensed under the GPL version 2
 */
#include "hw.h"
#include "qemu-timer.h"
#include "isa.h"
#include "console.h"
#include "msx.h"

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#define BORDER_SIZE 16

#define VRAM_SIZE 0x4000
#define VRAM_ADDR(addr) ((addr) & (VRAM_SIZE - 1))
#define VRAM_ADDR_INC(addr) addr = VRAM_ADDR(addr + 1)

typedef struct {
    qemu_irq irq;
    DisplayState *ds;
    int invalidate;
    int render_dirty;
    int vdp_dirty;
    int zoom;
    QEMUTimer *timer;
    
    uint8_t *vram;
    
    uint16_t addr;
    int addr_mode;
    int addr_seq;
    uint8_t addr_latch;
    uint8_t data;
    uint8_t status;
    uint8_t ctrl[8];
    
    struct {
        uint8_t plane;
        uint8_t line;
    } line_sprite[4];
} V9918State;

static void v9918_ctrl(V9918State *s, uint8_t reg, uint8_t value)
{
    static const uint8_t mask[8] = {0x03, 0xfb, 0x0f, 0xff, 0x07, 0x7f, 0x07, 0xff};
    reg &= 0x07;
    value &= mask[reg];
    s->ctrl[reg] = value;
    if (reg == 1 && (s->status & 0x80)) {
        qemu_set_irq(s->irq, value & 0x20);
    }
}

uint32_t v9918_read(void *opaque, uint32_t addr)
{
    uint32_t result;
    V9918State *s = (V9918State *)opaque;
    s->addr_seq = 1;
    if (addr & 1) {
        result = s->status;
        s->status &= 0x1f;
        qemu_irq_lower(s->irq);
    } else {
        result = s->data;
        s->data = s->vram[s->addr];
        VRAM_ADDR_INC(s->addr);
    }
    return result;
}

void v9918_write(void *opaque, uint32_t addr, uint32_t value)
{
    V9918State *s = (V9918State *)opaque;
    if (addr & 1) {
        if (s->addr_seq) {
            s->addr_seq = 0;
            s->addr_latch = value;
        } else {
            s->addr_seq = 1;
            if (value & 0x80) {
                v9918_ctrl(s, value, s->addr_latch);
                s->vdp_dirty = 1;
            } else {
                s->addr = VRAM_ADDR((value << 8) + s->addr_latch);
                s->addr_mode = value & 0x40;
                if (!s->addr_mode) {
                    s->data = s->vram[s->addr];
                    VRAM_ADDR_INC(s->addr);
                }
            }
        }
    } else {
        s->addr_seq = 1;
        if (s->addr_mode) {
            s->vram[s->addr] = s->data = value;
            VRAM_ADDR_INC(s->addr);
        } else {
            s->data = s->vram[s->addr];
            VRAM_ADDR_INC(s->addr);
            s->vram[s->addr] = value;
        }
        s->vdp_dirty = 1;
    }
}

static void v9918_sprite_collision_check(V9918State *s)
{
    const uint8_t *spr_tab = s->vram + ((int)(s->ctrl[5]) << 7);
    const uint8_t *spr_gen = s->vram + ((int)(s->ctrl[6]) << 11);
    const uint8_t *src;
    uint8_t n;
    for (n = 0, src = spr_tab; n < 32 && *src != 208; n++, src += 4);
    if (s->ctrl[1] & 0x02) { /* 16x16 sprites */
        uint8_t j;
        for (j = 0, src = spr_tab; j < n; j++, src += 4) {
            if (src[3] & 0x0f) { /* non-transparent color */
                uint8_t i;
                const uint8_t *d;
                for (i = j + 1, d = src + 4; i < n; i++, d += 4) {
                    if (d[3] & 0x0f) { /* non-transparent color */
                        uint8_t dv = src[0] - d[0];
                        if (dv < 16 || dv > 240) {
                            uint8_t dh = src[1] - d[1];
                            if (dh < 16 || dh > 240) {
                                const uint8_t *ps = spr_gen + ((int)(src[2] & 0xfc) << 3);
                                const uint8_t *pd = spr_gen + ((int)(d[2] & 0xfc) << 3);
                                if (dv < 16) {
                                    pd += dv;
                                } else {
                                    dv = 256 - dv;
                                    ps += dv;
                                }
                                if (dh > 240) {
                                    dh = 256 - dh;
                                    const uint8_t *t = ps;
                                    ps = pd;
                                    pd = t;
                                }
                                while (dv < 16) {
                                    uint16_t ls = (((uint16_t)*ps << 8) + *(ps + 16));
                                    uint16_t ld = (((uint16_t)*pd << 8) + *(pd + 16));
                                    if (ld & (ls >> dh)) {
                                        break;
                                    } else {
                                        dv++;
                                        ps++;
                                        pd++;
                                    }
                                }
                                if (dv < 16) {
                                    s->status |= 0x20; /* sprite collision */
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
    } else { /* 8x8 sprites */
        uint8_t j;
        for (j = 0, src = spr_tab; j < n; j++, src += 4) {
            if (src[3] & 0x0f) { /* non-transparent color */
                uint8_t i;
                const uint8_t *d;
                for (i = j + 1, d = src + 4; i < n; i++, d += 4) {
                    if (d[3] & 0x0f) { /* non-transparent color */
                        uint8_t dv = src[0] - d[0];
                        if (dv < 8 || dv > 248) {
                            uint8_t dh = src[1] - d[1];
                            if (dh < 8 || dh > 248) {
                                const uint8_t *ps = spr_gen + ((int)src[2] << 3);
                                const uint8_t *pd = spr_gen + ((int)d[2] << 3);
                                if (dv < 8) {
                                    pd += dv;
                                } else {
                                    dv = 256 - dv;
                                    ps += dv;
                                }
                                if (dh > 248) {
                                    dh = 256 - dh;
                                    const uint8_t *t = ps;
                                    ps = pd;
                                    pd = t;
                                }
                                while (dv < 8 && !(*pd & (*ps >> dh))) {
                                    dv++;
                                    ps++;
                                    pd++;
                                }
                                if (dv < 8) {
                                    s->status |= 0x20; /* sprite collision */
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static unsigned int V9918Palette[16 * 3] = {
      0,   0,   0,
      0,   0,   0,
     33, 200,  66,
     94, 220, 120,
     84,  85, 237,
    125, 118, 252,
    212,  82,  77,
     66, 235, 245,
    252,  85,  84,
    255, 121, 120,
    212, 193,  84,
    230, 206, 128,
     33, 176,  59,
    201,  91, 186,
    204, 204, 204,
    255, 255, 255
};

typedef void (*v9918_render_fn_t)(V9918State *s, int scanline,
                                  void *dest, unsigned int width);

#define ADDR_MSK(r, sh) ((((int)s->ctrl[r] + 1) << (sh)) - 1)
#define T_OFF(n, msk, len) ((msk) & (((1L << 17) - (len)) | (n)))
#define CHRTAB_MSK(p, sh) ((((int)(s->ctrl[2] & ((p) ? 0xdf : 0xff )) + 1) << (sh)) - 1)
#define CHRTAB(n, p, len) T_OFF((n), CHRTAB_MSK(p, 10), (len))
#define CHRGEN_MSK ADDR_MSK(4, 11)
#define CHRGEN(n, len) T_OFF(n, CHRGEN_MSK, len)
#define COLTAB_MSK ADDR_MSK(3, 6)
#define COLTAB(n, len) T_OFF(n, COLTAB_MSK, len)
#define SPRTAB_MSK ADDR_MSK(5, 7)
#define SPRTAB(n, len) T_OFF(n, SPRTAB_MSK, len)

#define BG_COLOR (s->ctrl[7] & 0x0f)
#define FG_COLOR (s->ctrl[7] >> 4)

#define BLANK_ENABLE (!(s->ctrl[1] & 0x40))
#define SPRITE_MAG (s->ctrl[1] & 0x01)
#define SPRITE_SIZE (s->ctrl[1] & 0x02)

/* IMPORTANT: for the sprite functions, "scanline" parameter is expected to be
 * a visible line number (0-191) instead of a true scanline number as in what
 * is passed to the graphics rendering functions */
static int v9918_scan_sprites(V9918State *s, int scanline,
                              int breakline, int maxsprites)
{
    const uint8_t *spr_tab = s->vram + ((int)(s->ctrl[5]) << 7);
    const uint8_t height = SPRITE_SIZE ? 16 : 8;
    const uint8_t b = SPRITE_MAG;
    int n, count;
    for (n = 0, count = 0; n < 32 && count <= maxsprites; n++, spr_tab += 4) {
        if (*spr_tab == breakline) {
            break;
        }
        if (*spr_tab == breakline + 1) {
            continue;
        }
        uint8_t sprline = (uint8_t)((scanline - (*spr_tab)) >> b);
        if (sprline < height) {
            if (count == maxsprites) {
                if (!(s->status & 0x40)) {
                    s->status = (s->status & 0xa0) | 0x40 | n;
                }
                //count--;
                break;
            }
            s->line_sprite[count].plane = n;
            s->line_sprite[count++].line = sprline;
        }
    }
    return count;
}

static uint8_t *v9918_render_sprites(V9918State *s, int scanline)
{
    int nspr = v9918_scan_sprites(s, scanline, 208, 4);
    if (!nspr) {
        return NULL;
    }
    static uint8_t zbuf[SCREEN_WIDTH + 32];
    memset(zbuf, 0, sizeof(zbuf));
    
    const uint8_t *spr_gen = s->vram + ((int)(s->ctrl[6]) << 11);
    const uint8_t *spr_tab = s->vram + ((int)(s->ctrl[5]) << 7);
    const uint8_t b = SPRITE_MAG;
    uint8_t h = SPRITE_SIZE ? 0xfc : 0xff;
    int visible_count = 0;
    while (nspr--) {
        const uint8_t *sp = spr_tab + (s->line_sprite[nspr].plane << 2);
        uint8_t c = sp[3];
        if (c & 0x0f) {
            int l = sp[1] - ((c & 0x80) >> 2);
            const uint8_t *p = spr_gen + (((int)(sp[2] & h)) << 3) + s->line_sprite[nspr].line;
            int k = ((int)*p) << 8;
            if (h == 0xfc) {
                k |= p[16];
            }
            if (l < 0) {
                k <<= (-l) >> b;
                l = 0;
            }
            if (k && l < 256) {
                visible_count++;
                c &= 0x0f;
                for (; k && l < 256; k <<= 1, l += b + 1) {
                    if (k & 0x8000) {
                        zbuf[l] = c;
                        if (b && l + 1 < 256) {
                            zbuf[l + 1] = c;
                        }
                    }
                }
            }
        }
            
    }
    return visible_count ? zbuf : NULL;
}

#include "pixel_ops.h"
#include "v9918_render_template.h"

static void v9918_render_screen(V9918State *s)
{
    if (!is_graphic_console()) {
        return;
    }
    
    int mode = ((s->ctrl[0] >> 1) & 1) | ((s->ctrl[1] >> 2) & 6);
    if (mode < 3) {
        mode++;
    } else if (mode == 4) {
        mode = 0;
    } else {
        return;
    }
    
    v9918_render_fn_t render_fn = 0;
    if (s->zoom == 1) {
        switch (ds_get_bits_per_pixel(s->ds)) {
            case 8:  render_fn = v9918_render_fn_8_z1[mode]; break;
            case 15: render_fn = v9918_render_fn_15_z1[mode]; break;
            case 16: render_fn = v9918_render_fn_16_z1[mode]; break;
            case 24: render_fn = v9918_render_fn_24_z1[mode]; break;
            case 32: render_fn = v9918_render_fn_32_z1[mode]; break;
            default: break;
        }
    } else if (s->zoom == 2) {
        switch (ds_get_bits_per_pixel(s->ds)) {
            case 8:  render_fn = v9918_render_fn_8_z2[mode]; break;
            case 15: render_fn = v9918_render_fn_15_z2[mode]; break;
            case 16: render_fn = v9918_render_fn_16_z2[mode]; break;
            case 24: render_fn = v9918_render_fn_24_z2[mode]; break;
            case 32: render_fn = v9918_render_fn_32_z2[mode]; break;
            default: break;
        }
    }
    uint8_t *fb = ds_get_data(s->ds);
    int linesize = ds_get_linesize(s->ds);
    if (!render_fn || !fb || linesize < s->zoom * SCREEN_WIDTH ||
        ds_get_width(s->ds) < s->zoom * SCREEN_WIDTH ||
        ds_get_height(s->ds) < s->zoom * SCREEN_HEIGHT) {
        return;
    }

    int i = BG_COLOR * 3 ?: 3;
    V9918Palette[0] = V9918Palette[i];
    V9918Palette[1] = V9918Palette[i + 1];
    V9918Palette[2] = V9918Palette[i + 2];
    
    for (i = 0; i < SCREEN_HEIGHT + 2 * BORDER_SIZE; i++) {
        render_fn(s, i, fb, linesize);
        fb += linesize * s->zoom;
    }
    
    s->render_dirty = 1;
}

static void v9918_vertical_retrace(V9918State *s)
{
    if (s->vdp_dirty) {
        if (!(s->status & 0x20)) {
            v9918_sprite_collision_check(s);
        }
        v9918_render_screen(s);
        s->vdp_dirty = 0;
    }
    s->status |= 0x80;
    if (s->ctrl[1] & 0x20) {
        qemu_irq_raise(s->irq);
    }
}

static void v9918_timer(void *opaque)
{
    V9918State *s = (V9918State *)opaque;
    v9918_vertical_retrace(s);
    int64_t next = qemu_get_clock(vm_clock) + muldiv64(1, ticks_per_sec, 50);
    qemu_mod_timer(s->timer, next);
}

static void v9918_invalidate_display(void *opaque)
{
    V9918State *s = (V9918State *)opaque;
    s->invalidate = 1;
}

static void v9918_update_display(void *opaque)
{
    V9918State *s = (V9918State *)opaque;
    if (s->invalidate) {
        s->invalidate = 0;
        if (ds_get_width(s->ds) != s->zoom * (SCREEN_WIDTH + 2 * BORDER_SIZE) ||
            ds_get_height(s->ds) != s->zoom * (SCREEN_HEIGHT + 2 * BORDER_SIZE)) {
            qemu_console_resize(s->ds,
                                s->zoom * (SCREEN_WIDTH + 2 * BORDER_SIZE),
                                s->zoom * (SCREEN_HEIGHT + 2 * BORDER_SIZE));
        }
        v9918_render_screen(s);
    }

    if (s->render_dirty) {
        s->render_dirty = 0;
        dpy_update(s->ds, 0, 0, ds_get_width(s->ds), ds_get_height(s->ds));
    }
}

void v9918_change_zoom(void *opaque)
{
    V9918State *s = (V9918State *)opaque;
    s->invalidate = 1;
    if (s->zoom == 1) {
        s->zoom = 2;
    } else {
        s->zoom = 1;
    }
}

void v9918_reset(void *opaque)
{
    V9918State *s = (V9918State *)opaque;
    s->addr = 0;
    s->addr_mode = 0;
    s->addr_seq = 0;
    s->addr_latch = 0;
    s->data = 0;
    s->status = 0;
    memset(s->ctrl, 0, sizeof(s->ctrl));
    memset(s->vram, 0, VRAM_SIZE);
}

void *v9918_init(qemu_irq irq)
{
    V9918State *s = (V9918State *)qemu_mallocz(sizeof(*s));
    s->irq = irq;
    s->invalidate = 1;
    s->zoom = 1;
    s->ds = graphic_console_init(v9918_update_display,
                                 v9918_invalidate_display,
                                 NULL, NULL, s);
    s->vram = qemu_mallocz(VRAM_SIZE);
    s->timer = qemu_new_timer(vm_clock, v9918_timer, s);
    v9918_reset(s);
    qemu_mod_timer(s->timer, qemu_get_clock(vm_clock));
    return s;
}
