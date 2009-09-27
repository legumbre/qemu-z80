/*
 * SAM Coupé Keyboard Layout
 */

/* An extension of the ZX Spectrum keyboard layout */

#define DEF_ZX_KEY(name, row, column) DEF_SAM_KEY(name, row, column)
#include "zx_key_template.h"

/* SAM Coupé-specific keys */

DEF_SAM_KEY(F1,        0, 5)
DEF_SAM_KEY(F2,        0, 6)
DEF_SAM_KEY(F3,        0, 7)

DEF_SAM_KEY(F4,        1, 5)
DEF_SAM_KEY(F5,        1, 6)
DEF_SAM_KEY(F6,        1, 7)

DEF_SAM_KEY(F7,        2, 5)
DEF_SAM_KEY(F8,        2, 6)
DEF_SAM_KEY(F9,        2, 7)

DEF_SAM_KEY(ESCAPE,    3, 5)
DEF_SAM_KEY(TAB,       3, 6)
DEF_SAM_KEY(CAPSLOCK,  3, 7)

DEF_SAM_KEY(MINUS,     4, 5)
DEF_SAM_KEY(PLUS,      4, 6)
DEF_SAM_KEY(DELETE,    4, 7)

DEF_SAM_KEY(EQUALS,    5, 5)
DEF_SAM_KEY(DQUOTE,    5, 6)
DEF_SAM_KEY(F0,        5, 7)

DEF_SAM_KEY(SEMICOLON, 6, 5)
DEF_SAM_KEY(COLON,     6, 6)
DEF_SAM_KEY(EDIT,      6, 7)

DEF_SAM_KEY(COMMA,     7, 5)
DEF_SAM_KEY(PERIOD,    7, 6)
DEF_SAM_KEY(INVERSE,   7, 7)

/* high bits = 0xff */

DEF_SAM_KEY(CONTROL,   8, 0)
DEF_SAM_KEY(UP,        8, 1)
DEF_SAM_KEY(DOWN,      8, 2)
DEF_SAM_KEY(LEFT,      8, 3)
DEF_SAM_KEY(RIGHT,     8, 4)

#undef DEF_SAM_KEY
