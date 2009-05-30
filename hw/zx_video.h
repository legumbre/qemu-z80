#ifndef HW_ZX_VIDEO_H
#define HW_ZX_VIDEO_H
/* ZX Spectrum Video */

void zx_video_init(ram_addr_t zx_vram_offset);
void zx_video_do_retrace(void);

#endif
