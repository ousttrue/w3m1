
#ifndef _WC_ISO2022_H
#define _WC_ISO2022_H
#include "status.h"

extern uint8_t WC_ISO_MAP[];

Str  wc_conv_from_iso2022(Str is, CharacterEncodingScheme ces);
void wc_push_to_iso2022(Str os, wc_wchar_t cc, wc_status *st);
void wc_push_to_euc(Str os, wc_wchar_t cc, wc_status *st);
void wc_push_to_eucjp(Str os, wc_wchar_t cc, wc_status *st);
void wc_push_to_euctw(Str os, wc_wchar_t cc, wc_status *st);
void wc_push_to_iso8859(Str os, wc_wchar_t cc, wc_status *st);
void wc_push_to_iso2022_end(Str os, wc_status *st);
void wc_push_iso2022_esc(Str os, CodedCharacterSet ccs, uint8_t g, uint8_t invoke, wc_status *st);
int  wc_parse_iso2022_esc(uint8_t **ptr, wc_status *st);
Str  wc_char_conv_from_iso2022(uint8_t c, wc_status *st);

#endif
