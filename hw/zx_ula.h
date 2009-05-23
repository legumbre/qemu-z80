#ifndef HW_ZXSPEC_H
#define HW_ZXSPEC_H
/* ZX Spectrum */

/* zx_ula.c */

void zx_ula_init(DisplayState *ds, uint8_t *zx_screen_base,
		 unsigned long zx_ram_offset);
void zx_ula_do_retrace(void);

#endif
