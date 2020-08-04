
#ifndef _WC_PRIV_H
#define _WC_PRIV_H
#include "status.h"

Str wc_conv_from_priv1(Str is, CharacterEncodingScheme ces);
Str wc_conv_from_ascii(Str is, CharacterEncodingScheme ces);
Str wc_char_conv_from_priv1(uint8_t c, wc_status *st);
void wc_push_to_raw(Str os, wc_wchar_t cc, wc_status *st);

#endif
