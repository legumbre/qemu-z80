/*
 * SAM Coupé Video Emulation
 *
 * Copyright (c) 2007-2009 Stuart Brady <stuart.brady@gmail.com>
 *
 * Uses code from VGA emulation
 *   Copyright (c) 2003 Fabrice Bellard
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
#include "sam_video.h"
#include "pixel_ops.h"
#include "pixel_ops_dup.h"

typedef unsigned int rgb_to_pixel_dup_func(unsigned int r,
                                           unsigned int g,
                                           unsigned int b);

typedef struct {
    DisplayState *ds;
    uint8_t *vram_ptr;

    int bwidth;
    int bheight;
    int swidth;
    int sheight;
    int twidth;
    int theight;

    int border;
    int prevborder;

    int flash;
    int flashcount;

    int invalidate;
    uint32_t palette[16];
    rgb_to_pixel_dup_func *rgb_to_pixel;
} ZXVState;

static const uint32_t sam_cols[16] = {
    0x00000000, /*  0: Black          */
    0x000000c0, /*  1: Blue           */
    0x00c00000, /*  2: Red            */
    0x00c000c0, /*  3: Magenta        */
    0x0000c000, /*  4: Green          */
    0x0000c0c0, /*  5: Cyan           */
    0x00c0c000, /*  6: Yellow         */
    0x00c0c0c0, /*  7: Light grey     */

    0x00000000, /*  8: Black          */
    0x000000ff, /*  9: Bright blue    */
    0x00ff0000, /* 10: Bright red     */
    0x00ff00ff, /* 11: Bright magenta */
    0x0000ff00, /* 12: Bright green   */
    0x0000ffff, /* 13: Bright cyan    */
    0x00ffff00, /* 14: Bright yellow  */
    0x00ffffff, /* 15: White          */
};

/* copied from vga.c / vga_template.h */

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

typedef void sam_draw_line_func(uint8_t *d, uint32_t font_data,
                               uint32_t xorcol, uint32_t bgcol);

#define DEPTH 8
#include "zx_glyphs.h"

#define DEPTH 16
#include "zx_glyphs.h"

#define DEPTH 32
#include "zx_glyphs.h"

enum {
    sam_pixfmt_8 = 0,
    sam_pixfmt_15rgb,
    sam_pixfmt_16rgb,
    sam_pixfmt_32rgb,
    sam_pixfmt_32bgr,
    NB_DEPTHS
};

static sam_draw_line_func *sam_draw_line_table[NB_DEPTHS] = {
    zx_draw_glyph_line_8,
    zx_draw_glyph_line_16,
    zx_draw_glyph_line_16,
    zx_draw_glyph_line_32,
    zx_draw_glyph_line_32,
};

static rgb_to_pixel_dup_func *rgb_to_pixel_dup_table[NB_DEPTHS] = {
    rgb_to_pixel8_dup,
    rgb_to_pixel15_dup,
    rgb_to_pixel16_dup,
    rgb_to_pixel32_dup,
    rgb_to_pixel32bgr_dup,
};

static inline int get_pixfmt_index(DisplayState *s)
{
    switch(ds_get_bits_per_pixel(s)) {
    default:
    case 8:
        return sam_pixfmt_8;
    case 15:
        return sam_pixfmt_15rgb;
    case 16:
        return sam_pixfmt_16rgb;
    case 32:
        if (is_surface_bgr(s->surface)) {
            return sam_pixfmt_32bgr;
        } else {
            return sam_pixfmt_32rgb;
        }
    }
}

/* end of code copied from vga.c / vga_template.h */

static ZXVState *samvstate;

void sam_video_set_border(int col)
{
    ZXVState *s = samvstate;

    s->border = col;
};

void sam_video_do_retrace(void)
{
    ZXVState *s = samvstate;

    if (++s->flashcount == 16) {
        s->flashcount = 0;
        s->invalidate = 1;
        s->flash = !s->flash;
    }
}

static void sam_draw_scanline(ZXVState *s1, uint8_t *d,
                             const uint8_t *s)
{
    int x, x_incr;
    uint32_t col;

    x_incr = (ds_get_bits_per_pixel(s1->ds) + 7) >> 3;

    for (x = 0; x < 128; x++) {
        col = s1->palette[*s >> 4];
        *(uint32_t *)d = col;
        d += x_incr;
        col = s1->palette[*s & 0x0f];
        *(uint32_t *)d = col;
        d += x_incr;
        s++;
    }
}

static void sam_border_row(ZXVState *s, uint8_t *d)
{
    int x, x_incr;
    sam_draw_line_func *sam_draw_line;

    sam_draw_line = sam_draw_line_table[get_pixfmt_index(s->ds)];
    x_incr = (ds_get_bits_per_pixel(s->ds) + 7) >> 3;

    for (x = 0; x < s->twidth / 8; x++) {
        sam_draw_line(d, 0xff, s->palette[s->border], 0);
        d += 8 * x_incr;
    }
}

static void sam_border_sides(ZXVState *s, uint8_t *d)
{
    int x, x_incr;
    sam_draw_line_func *sam_draw_line;

    sam_draw_line = sam_draw_line_table[get_pixfmt_index(s->ds)];
    x_incr = (ds_get_bits_per_pixel(s->ds) + 7) >> 3;

    for (x = 0; x < s->bwidth / 8; x++) {
        sam_draw_line(d, 0xff, s->palette[s->border], 0);
        d += 8 * x_incr;
    }
    d += s->swidth * x_incr;
    for (x = 0; x < s->bwidth / 8; x++) {
        sam_draw_line(d, 0xff, s->palette[s->border], 0);
        d += 8 * x_incr;
    }
}

static void update_palette(ZXVState *s)
{
    int i, r, g, b;
    for(i = 0; i < 16; i++) {
        r = (sam_cols[i] >> 16) & 0xff;
        g = (sam_cols[i] >> 8) & 0xff;
        b = sam_cols[i] & 0xff;
        s->palette[i] = s->rgb_to_pixel(r, g, b);
    }
}

static void sam_update_display(void *opaque)
{
    int y;
    uint8_t *d;
    ZXVState *s = (ZXVState *)opaque;
    uint32_t addr;
    int x_incr;
    int dirty = s->invalidate;
    static int inited = 0;

    x_incr = (ds_get_bits_per_pixel(s->ds) + 7) >> 3;

    if (unlikely(inited == 0)) {
        s->rgb_to_pixel = rgb_to_pixel_dup_table[get_pixfmt_index(s->ds)];
        update_palette(s);
        inited = 1;
    }

    if (unlikely(ds_get_width(s->ds) != s->twidth ||
                 ds_get_height(s->ds) != s->theight)) {
        qemu_console_resize(s->ds, s->twidth, s->theight);
        s->invalidate = 1;
        s->prevborder = -1;
    }

//    if (!dirty) {
//        for (addr = 0; addr < 0x1b00; addr += TARGET_PAGE_SIZE) {
//            if (cpu_physical_memory_get_dirty(addr, VGA_DIRTY_FLAG)) {
//                dirty = 1;
//            }
//        }
//    }

    dirty = 1;

    if (dirty) {
        d = ds_get_data(s->ds);
        d += s->bheight * ds_get_linesize(s->ds);
        d += s->bwidth * x_incr;

        for (y = 0; y < 192; y++) {
            addr = y << 7;
            sam_draw_scanline(s, d, s->vram_ptr + addr);
            d += ds_get_linesize(s->ds);
        }

        s->invalidate = 0;
//        cpu_physical_memory_reset_dirty(0, 0x1b00, VGA_DIRTY_FLAG);
    }

    if (s->border != s->prevborder) {
        d = ds_get_data(s->ds);
        for (y = 0; y < s->bheight; y++) {
            sam_border_row(s, d + (y * ds_get_linesize(s->ds)));
        }
        for (y = s->bheight; y < s->theight - s->bheight; y++) {
            sam_border_sides(s, d + (y * ds_get_linesize(s->ds)));
        }
        for (y = s->theight - s->bheight; y < s->theight; y++) {
            sam_border_row(s, d + (y * ds_get_linesize(s->ds)));
        }
        s->prevborder = s->border;
    }

    dpy_update(s->ds, 0, 0, s->twidth, s->theight);
}

static void sam_invalidate_display(void *opaque)
{
    ZXVState *s = (ZXVState *)opaque;
    s->invalidate = 1;
    s->prevborder = -1;
}

void sam_video_init(ram_addr_t sam_vram_offset)
{
    ZXVState *s = qemu_mallocz(sizeof(ZXVState));
    samvstate = s;
    s->invalidate = 1;
    s->prevborder = -1;
    s->flashcount = 0;
    s->vram_ptr = qemu_get_ram_ptr(sam_vram_offset) + (30 << 14);

    s->ds = graphic_console_init(sam_update_display, sam_invalidate_display,
                                 NULL, NULL, s);

    s->bwidth = 32;
    s->bheight = 24;
    s->swidth = 256;
    s->sheight = 192;
    s->twidth = s->swidth + s->bwidth * 2;
    s->theight = s->sheight + s->bheight * 2;
    s->border = 0;
    s->flash = 0;
}
