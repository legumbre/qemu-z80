#ifndef HW_SAM_VIDEO_H
#define HW_SAM_VIDEO_H
/* SAM Coupé Video */

void sam_video_init(ram_addr_t sam_vram_offset);
void sam_video_do_retrace(void);
void sam_video_set_border(int col);

#endif
