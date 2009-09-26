#ifndef HW_ZX_VIDEO_H
#define HW_ZX_VIDEO_H
/* ZX Spectrum Video */

void zx_video_init(ram_addr_t zx_vram_offset, int is_128k);
void zx_video_do_retrace(void);
void zx_video_set_border(int col);

#endif
