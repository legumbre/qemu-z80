#ifndef HW_ZX_VIDEO_H
#define HW_ZX_VIDEO_H
/* ZX Spectrum Video */

void zx_video_init(uint8_t *zx_vram_base);
void zx_video_do_retrace(void);

#endif
