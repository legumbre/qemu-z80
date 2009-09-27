#ifndef HW_SAM_KEYBOARD_H
#define HW_SAM_KEYBOARD_H
/* SAM Coupé Keyboard */

void sam_keyboard_init(void);
uint32_t sam_keyboard_read(void *opaque, uint32_t addr);

#endif
