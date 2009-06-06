#ifndef HW_ZX_KEYBOARD_H
#define HW_ZX_KEYBOARD_H
/* ZX Spectrum Keyboard */

void zx_keyboard_init(void);
uint32_t zx_keyboard_read(void *opaque, uint32_t addr);

#endif
