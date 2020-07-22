
#include "wc.h"
#include "big5.h"
#include "hkscs.h"
#include "search.h"
#include "wtf.h"
#include "ucs.h"


#define C0 WC_HKSCS_MAP_C0
#define GL WC_HKSCS_MAP_GL
#define C1 WC_HKSCS_MAP_C1
#define LB WC_HKSCS_MAP_LB
#define UB WC_HKSCS_MAP_UB
#define UH WC_HKSCS_MAP_UH

uint8_t WC_HKSCS_MAP[ 0x100 ] = {
    C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0,
    C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0,
    GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL,
    GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, C0,

    C1, C1, C1, C1, C1, C1, C1, C1, UH, UH, UH, UH, UH, UH, UH, UH,
    UH, UH, UH, UH, UH, UH, UH, UH, UH, UH, UH, UH, UH, UH, UH, UH,
    UH, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, C1,
};

wc_wchar_t
wc_hkscs_to_cs128w(wc_wchar_t cc)
{
    cc.code = WC_HKSCS_N(cc.code);
    if (cc.code < 0x4000)
	cc.ccs = WC_CCS_HKSCS_1;
    else {
	cc.ccs = WC_CCS_HKSCS_2;
	cc.code -= 0x4000;
    }
    cc.code = WC_N_CS128W(cc.code);
    return cc;
}

wc_wchar_t
wc_cs128w_to_hkscs(wc_wchar_t cc)
{
    cc.code = WC_CS128W_N(cc.code);
    if (cc.ccs == WC_CCS_HKSCS_2)
	cc.code += 0x4000;
    cc.ccs = WC_CCS_HKSCS;
    cc.code = WC_N_HKSCS(cc.code);
    return cc;
}

uint32_t
wc_hkscs_to_N(uint32_t c)
{
    if (c < 0xA140)	/* 0x8840 - 0xA0FE */
	return WC_HKSCS_N(c);
    			/* 0xFA40 - 0xFEFE */
    return WC_HKSCS_N(c) - 0x59 * 0x9D;
}

Str
wc_conv_from_hkscs(Str is, wc_ces ces)
{
    Str os;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->length;
    uint8_t *p;
    int state = WC_HKSCS_NOSTATE;
    uint32_t hkscs;

    for (p = sp; p < ep && *p < 0x80; p++) 
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->length);
    if (p > sp)
	os->Push((char *)is->ptr, (int)(p - sp));

    for (; p < ep; p++) {
	switch (state) {
	case WC_HKSCS_NOSTATE:
	    switch (WC_HKSCS_MAP[*p]) {
	    case UB:
	    case UH:
		state = WC_HKSCS_MBYTE1;
		break;
	    case C1:
		wtf_push_unknown(os, p, 1);
		break;
	    default:
		os->Push((char)*p);
		break;
	    }
	    break;
	case WC_HKSCS_MBYTE1:
	    if (WC_HKSCS_MAP[*p] & LB) {
		hkscs = ((uint32_t)*(p-1) << 8) | *p;
		if (*(p-1) >= 0xA1 && *(p-1) <= 0xF9)
		    wtf_push(os, WC_CCS_BIG5, hkscs);
		else
		    wtf_push(os, WC_CCS_HKSCS, hkscs);
	    } else
		wtf_push_unknown(os, p-1, 2);
	    state = WC_HKSCS_NOSTATE;
	    break;
	}
    }
    switch (state) {
    case WC_HKSCS_MBYTE1:
	wtf_push_unknown(os, p-1, 1);
	break;
    }
    return os;
}

void
wc_push_to_hkscs(Str os, wc_wchar_t cc, wc_status *st)
{
  while (1) {
    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
	os->Push((char)cc.code);
	return;
    case WC_CCS_BIG5_1:
    case WC_CCS_BIG5_2:
	cc = wc_cs94w_to_big5(cc);
    case WC_CCS_BIG5:
	os->Push((char)(cc.code >> 8));
	os->Push((char)(cc.code & 0xff));
	return;
    case WC_CCS_HKSCS_1:
    case WC_CCS_HKSCS_2:
	cc = wc_cs128w_to_hkscs(cc);
    case WC_CCS_HKSCS:
	os->Push((char)(cc.code >> 8));
	os->Push((char)(cc.code & 0xff));
	return;
    case WC_CCS_UNKNOWN_W:
	if (!WcOption.no_replace)
	    os->Push(WC_REPLACE_W);
	return;
    case WC_CCS_UNKNOWN:
	if (!WcOption.no_replace)
	    os->Push(WC_REPLACE);
	return;
    default:
#ifdef USE_UNICODE
	if (WcOption.ucs_conv)
	    cc = wc_any_to_any_ces(cc, st);
	else
#endif
	    cc.ccs = WC_CCS_IS_WIDE(cc.ccs) ? WC_CCS_UNKNOWN_W : WC_CCS_UNKNOWN;
	continue;
    }
  }
}

Str
wc_char_conv_from_hkscs(uint8_t c, wc_status *st)
{
    static Str os;
    static uint8_t hkscsu;
    uint32_t hkscs;

    if (st->state == -1) {
	st->state = WC_HKSCS_NOSTATE;
	os = Strnew_size(8);
    }

    switch (st->state) {
    case WC_HKSCS_NOSTATE:
	switch (WC_HKSCS_MAP[c]) {
	case UB:
	case UH:
	    hkscsu = c;
	    st->state = WC_HKSCS_MBYTE1;
	    return NULL;
	case C1:
	    break;
	default:
	    os->Push((char)c);
	    break;
	}
	break;
    case WC_HKSCS_MBYTE1:
	if (WC_HKSCS_MAP[c] & LB) {
	    hkscs = ((uint32_t)hkscsu << 8) | c;
	    if (hkscsu >= 0xA1 && hkscsu <= 0xF9 && c >= 0xA1)
		wtf_push(os, WC_CCS_BIG5, hkscs);
	    else
		wtf_push(os, WC_CCS_HKSCS, hkscs);
	}
	break;
    }
    st->state = -1;
    return os;
}
