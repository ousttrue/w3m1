
#ifndef _WC_VIET_H
#define _WC_VIET_H
#include "status.h"

extern uint8_t wc_c0_tcvn57122_map[];
extern uint8_t wc_c0_viscii112_map[];
extern uint8_t wc_c0_vps2_map[];

extern Str       wc_conv_from_viet(Str is, CharacterEncodingScheme ces);
extern void      wc_push_to_viet(Str os, wc_wchar_t cc, wc_status *st);
extern void      wc_push_to_cp1258(Str os, wc_wchar_t cc, wc_status *st);
extern wc_wchar_t wc_tcvn57123_to_tcvn5712(wc_wchar_t cc);
extern uint32_t wc_tcvn5712_precompose(uint8_t c1, uint8_t c2);
extern uint32_t wc_cp1258_precompose(uint8_t c1, uint8_t c2);
extern Str       wc_char_conv_from_viet(uint8_t c, wc_status *st);

#endif
