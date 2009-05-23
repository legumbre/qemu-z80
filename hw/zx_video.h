#ifndef HW_ZX_VIDEO_H
#define HW_ZX_VIDEO_H
/* ZX Spectrum Video */

void zx_video_init(DisplayState *ds, uint8_t *zx_screen_base,
                   unsigned long zx_ram_offset);
void zx_video_do_retrace(void);

#endif
