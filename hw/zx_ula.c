/*
 * QEMU ZX Spectrum Video Emulation.
 * 
 * Copyright (c) 2007 Stuart Brady
 * Copyright (c) 2003 Fabrice Bellard
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
#include "isa.h"
#include "console.h"
#include "zx_ula.h"

typedef struct {
    DisplayState *ds;
    uint8_t *vram_ptr;
    unsigned long vram_offset;

    int bwidth;
    int bheight;
    int swidth;
    int sheight;
    int twidth;
    int theight;

    int border;
    int prevborder;

    int invalidate;
} ZXVState;

char *colnames[8] = {
    "black",
    "blue",
    "red",
    "magenta",
    "green",
    "cyan",
    "yellow",
    "white"
};

uint32_t cols[16] = {
    0x00000000,
    0x000000c0,
    0x00c00000,
    0x00c000c0,
    0x0000c000,
    0x0000c0c0,
    0x00c0c000,
    0x00c0c0c0,
    0x00000000,
    0x000000ff,
    0x00ff0000,
    0x00ff00ff,
    0x0000ff00,
    0x0000ffff,
    0x00ffff00,
    0x00ffffff,
};

/* copied from vga_template.h */

#define cbswap_32(__x) \
((uint32_t)( \
                (((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
                (((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
                (((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
                (((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) ))

#ifdef WORDS_BIGENDIAN
#define PAT(x) (x)
#else
#define PAT(x) cbswap_32(x)
#endif

static const uint32_t dmask16[16] = {
    PAT(0x00000000),
    PAT(0x000000ff),
    PAT(0x0000ff00),
    PAT(0x0000ffff),
    PAT(0x00ff0000),
    PAT(0x00ff00ff),
    PAT(0x00ffff00),
    PAT(0x00ffffff),
    PAT(0xff000000),
    PAT(0xff0000ff),
    PAT(0xff00ff00),
    PAT(0xff00ffff),
    PAT(0xffff0000),
    PAT(0xffff00ff),
    PAT(0xffffff00),
    PAT(0xffffffff),
};

static const uint32_t dmask4[4] = {
    PAT(0x00000000),
    PAT(0x0000ffff),
    PAT(0xffff0000),
    PAT(0xffffffff),
};

//#define DEPTH 8
//#include "vga_template.h"

//#define DEPTH 15
//#include "vga_template.h"

//#define BGR_FORMAT
//#define DEPTH 15
//#include "vga_template.h"

//#define DEPTH 16
//#include "vga_template.h"

//#define BGR_FORMAT
//#define DEPTH 16
//#include "vga_template.h"

//#define DEPTH 32
//#include "vga_template.h"

//#define BGR_FORMAT
//#define DEPTH 32
//#include "vga_template.h"

static inline void zx_draw_line_8(uint8_t *d,
                                  uint32_t font_data,
                                  uint32_t xorcol,
                                  uint32_t bgcol)
{
    ((uint32_t *)d)[0] = (dmask16[(font_data >> 4)] & xorcol) ^ bgcol;
    ((uint32_t *)d)[1] = (dmask16[(font_data >> 0) & 0xf] & xorcol) ^ bgcol;
}

static inline void zx_draw_line_16(uint8_t *d,
                                   uint32_t font_data,
                                   uint32_t xorcol,
                                   uint32_t bgcol)
{
    ((uint32_t *)d)[0] = (dmask4[(font_data >> 6)] & xorcol) ^ bgcol;
    ((uint32_t *)d)[1] = (dmask4[(font_data >> 4) & 3] & xorcol) ^ bgcol;
    ((uint32_t *)d)[2] = (dmask4[(font_data >> 2) & 3] & xorcol) ^ bgcol;
    ((uint32_t *)d)[3] = (dmask4[(font_data >> 0) & 3] & xorcol) ^ bgcol;
}

static inline void zx_draw_line_32(uint8_t *d,
                                   uint32_t font_data,
                                   uint32_t xorcol,
                                   uint32_t bgcol)
{
    ((uint32_t *)d)[0] = (-((font_data >> 7)) & xorcol) ^ bgcol;
    ((uint32_t *)d)[1] = (-((font_data >> 6) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[2] = (-((font_data >> 5) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[3] = (-((font_data >> 4) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[4] = (-((font_data >> 3) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[5] = (-((font_data >> 2) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[6] = (-((font_data >> 1) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[7] = (-((font_data >> 0) & 1) & xorcol) ^ bgcol;
}

extern int zx_flash;
static ZXVState *zxvstate;

void zx_set_flash_dirty(void) {
    ZXVState *s = zxvstate;
    s->invalidate = 1;
}

static void zx_draw_line(ZXVState *s1, uint8_t *d,
                         const uint8_t *s, const uint8_t *as)
{
    int x;
    for (x = 0; x < 32; x++) {
        int attrib, fg, bg, bright, flash;

        attrib = *as;
        bright = (attrib & 0x40) >> 4;
        flash = (attrib & 0x80) && (zx_flash >= 16);
        if (flash) {
            fg = (attrib >> 3) & 0x07;
            bg = attrib & 0x07;
        } else {
            fg = attrib & 0x07;
            bg = (attrib >> 3) & 0x07;
        }
        fg |= bright;
        bg |= bright;
        
        zx_draw_line_32(d, *s, cols[fg] ^ cols[bg], cols[bg]);
        d += 8 * 4;
        s++; as++;
    }
}

static void zx_border_row(ZXVState *s, uint8_t *d)
{
    int x;
    for (x = 0; x < s->twidth; x++) {
        *((uint32_t *)d) = cols[s->border];
        d += 4;
    }
}

static void zx_border_sides(ZXVState *s, uint8_t *d)
{
    int x;
    for (x = 0; x < s->bwidth; x++) {
        *((uint32_t *)d) = cols[s->border];
        d += 4;
    }
    d += s->swidth * 4;
    for (x = 0; x < s->bwidth; x++) {
        *((uint32_t *)d) = cols[s->border];
        d += 4;
    }
}

static void zx_update_display(void *opaque)
{
    int y;
    uint8_t *d;
    ZXVState *s = (ZXVState *)opaque;
    uint32_t addr, attrib;
    int dirty = s->invalidate;

    if (!dirty) {
        for (addr = 0; addr < 0x1b00; addr += TARGET_PAGE_SIZE) {
            if (cpu_physical_memory_get_dirty(addr, VGA_DIRTY_FLAG)) {
                dirty = 1;
            }
        }
    }

    if (dirty) {
        d = s->ds->data;
        d += s->bheight * s->ds->linesize;
        d += s->bwidth * 4;

        for (y = 0; y < 192; y++) {
            addr = ((y & 0x07) << 8) | ((y & 0x38) << 2) | ((y & 0xc0) << 5);
            attrib = 0x1800 | ((y & 0xf8) << 2);
            zx_draw_line(s, d, s->vram_ptr + addr, s->vram_ptr + attrib);
            d += s->ds->linesize;
        }

        s->invalidate = 0;
        cpu_physical_memory_reset_dirty(0, 0x1b00, VGA_DIRTY_FLAG);
    }

    if (s->border != s->prevborder) {
        d = s->ds->data;
        for (y = 0; y < s->bheight; y++) {
            zx_border_row(s, d + y * s->ds->linesize);
        }
        for (y = s->bheight; y < s->theight - s->bheight; y++) {
            zx_border_sides(s, d + y * s->ds->linesize);
        }
        for (y = s->theight - s->bheight; y < s->theight; y++) {
            zx_border_row(s, d + y * s->ds->linesize);
        }
        s->prevborder = s->border;
    }

    dpy_update(s->ds, 0, 0, s->twidth, s->theight);
}

static void io_spectrum_write(void *opaque, uint32_t addr, uint32_t data)
{
    ZXVState *s = (ZXVState *)opaque;

/* port xxfe */
    s->border = data & 0x07;
};

void zx_ula_init(DisplayState *ds, uint8_t *zx_screen_base,
                 unsigned long ula_ram_offset)
{
    int zx_io_memory;

    ZXVState *s = qemu_mallocz(sizeof(ZXVState));
    if (!s)
        return;
    zxvstate = s;
    s->ds = ds;
    s->invalidate = 1;
    s->prevborder = -1;
//    s->vram_ptr = zx_screen_base;
//    s->vram_offset = ula_ram_offset;

    graphic_console_init(ds, zx_update_display,
                         NULL, NULL, NULL, s);

    s->bwidth = 32;
    s->bheight = 24;
    s->swidth = 256;
    s->sheight = 192;
    s->twidth = s->swidth + s->bwidth * 2;
    s->theight = s->sheight + s->bheight * 2;
    s->border = 0;
    dpy_resize(s->ds, s->twidth, s->theight);

    //zx_io_memory = cpu_register_io_memory(0, zx_mem_read, zx_mem_write, s);
    //cpu_register_physical_memory(0x4000, 0x2000, zx_io_memory);
//    cpu_register_physical_memory(0x4000, 0x2000, zx_io_memory);
//    cpu_register_physical_memory(0x4000, 0xc000, zx_io_memory);
    s->vram_ptr = phys_ram_base;//+ zx_io_memory;
    s->vram_offset = 0;//zx_io_memory;

    /* ZX Spectrum ULA */
    register_ioport_write(0, 0x10000, 1, io_spectrum_write, s);
}
