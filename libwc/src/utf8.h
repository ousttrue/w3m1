
#ifndef _WC_UTF8_H
#define _WC_UTF8_H
#include "ces.h"

#define WC_C_UTF8_L2	0x80
#define WC_C_UTF8_L3	0x800
#define WC_C_UTF8_L4	0x10000
#define WC_C_UTF8_L5	0x200000
#define WC_C_UTF8_L6	0x4000000

#define WC_UTF8_NOSTATE	0
#define WC_UTF8_NEXT	1

extern uint8_t WC_UTF8_MAP[];

extern size_t    wc_ucs_to_utf8(uint32_t ucs, uint8_t *utf8);
extern uint32_t wc_utf8_to_ucs(uint8_t *utf8);
extern Str       wc_conv_from_utf8(Str is, CharacterEncodingScheme ces);
extern void      wc_push_to_utf8(Str os, wc_wchar_t cc, wc_status *st);
extern void      wc_push_to_utf8_end(Str os, wc_status *st);
extern Str       wc_char_conv_from_utf8(uint8_t c, wc_status *st);

#endif
