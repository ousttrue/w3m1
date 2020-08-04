#include "priv.h"
#include "wtf.h"

Str
wc_conv_from_priv1(Str is, CharacterEncodingScheme ces)
{
    Str os;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->Size();
    uint8_t *p;
    CodedCharacterSet ccs = GetCesInfo(ces).gset[1].ccs;

    for (p = sp; p < ep && *p < 0x80; p++)
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->Size());
    if (p > sp)
	os->Push(is->ptr, (int)(p - sp));

    for (; p < ep; p++) {
	if (*p & 0x80)
	    wtf_push(os, ccs, (uint32_t)*p);
	else
	    os->Push((char)*p);
    }
    return os;
}

Str
wc_char_conv_from_priv1(uint8_t c, wc_status *st)
{
    Str os = Strnew_size(1);

    if (c & 0x80)
	wtf_push(os, st->ces_info->gset[1].ccs, (uint32_t)c);
    else
	os->Push((char)c);
    return os;
}

Str
wc_conv_from_ascii(Str is, CharacterEncodingScheme ces)
{
    Str os;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->Size();
    uint8_t *p;

    for (p = sp; p < ep && *p < 0x80; p++)
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->Size());
    if (p > sp)
	os->Push(is->ptr, (int)(p - sp));

    for (; p < ep; p++) {
	if (*p & 0x80)
	    wtf_push_unknown(os, p, 1);
	else
	    os->Push((char)*p);
    }
    return os;
}

void
wc_push_to_raw(Str os, wc_wchar_t cc, wc_status *st)
{

    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
    case WC_CCS_RAW:
	os->Push((char)cc.code);
    }
    return;
}
