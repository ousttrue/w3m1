
#include "wc.h"
#include "iso2022.h"
#include "hz.h"
#include "wtf.h"
#include "ucs.h"


Str
wc_conv_from_hz(Str is, CharacterEncodingScheme ces)
{
    Str os;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->Size();
    uint8_t *p;
    int state = WC_HZ_NOSTATE;

    for (p = sp; p < ep && *p < 0x80 && *p != WC_C_HZ_TILDA; p++)
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->Size());
    if (p > sp)
	os->Push(is->ptr, (int)(p - sp));

    for (; p < ep; p++) {
	switch (state) {
	case WC_HZ_NOSTATE:
	    if (*p == WC_C_HZ_TILDA)
		state = WC_HZ_TILDA;
	    else if (WC_ISO_MAP[*p] == WC_ISO_MAP_GR)
		state = WC_HZ_MBYTE1_GR;		/* GB 2312 ? */
	    else if (*p & 0x80)
		wtf_push_unknown(os, p, 1);
	    else
		os->Push((char)*p);
	    break;
	case WC_HZ_TILDA:
	    if (*p == WC_C_HZ_SI) {
		state = WC_HZ_MBYTE;
		break;
	    } else if (*p == WC_C_HZ_TILDA)
		os->Push((char)*p);
	    else if (*p == '\n')
		break;
	    else
		wtf_push_unknown(os, p-1, 2);
	    state = WC_HZ_NOSTATE;
	    break;
	case WC_HZ_TILDA_MB:
	    if (*p == WC_C_HZ_SO || *p == '\n') {
		state = WC_HZ_NOSTATE;
		break;
	    }
	    else if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
		wtf_push(os, WC_CCS_GB_2312, ((uint32_t)*(p-1) << 8) | *p);
	    else
		wtf_push_unknown(os, p-1, 2);
	    state = WC_HZ_MBYTE;
	    break;
	case WC_HZ_MBYTE:
	    if (*p == WC_C_HZ_TILDA)
		state = WC_HZ_TILDA_MB;
	    else if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
		state = WC_HZ_MBYTE1;
	    else
		wtf_push_unknown(os, p, 1);
	    break;
	case WC_HZ_MBYTE1:
	    if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
		wtf_push(os, WC_CCS_GB_2312, ((uint32_t)*(p-1) << 8) | *p);
	    else
		wtf_push_unknown(os, p-1, 2);
	    state = WC_HZ_MBYTE;
	    break;
	case WC_HZ_MBYTE1_GR:
	    if (WC_ISO_MAP[*p] == WC_ISO_MAP_GR)
		wtf_push(os, WC_CCS_GB_2312, ((uint32_t)*(p-1) << 8) | *p);
	    else
		wtf_push_unknown(os, p-1, 2);
	    state = WC_HZ_NOSTATE;
	    break;
	}
    }
    switch (state) {
    case WC_HZ_TILDA:
    case WC_HZ_TILDA_MB:
    case WC_HZ_MBYTE1:
    case WC_HZ_MBYTE1_GR:
	wtf_push_unknown(os, p-1, 1);
	break;
    }
    return os;
}

void
wc_push_to_hz(Str os, wc_wchar_t cc, wc_status *st)
{
  while (1) {
    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
	if (st->gl) {
	    os->Push(WC_C_HZ_TILDA);
	    os->Push(WC_C_HZ_SO);
	    st->gl = 0;
	}
	if ((char)cc.code == WC_C_HZ_TILDA)
	    os->Push(WC_C_HZ_TILDA);
	os->Push((char)cc.code);
	return;
    case WC_CCS_GB_2312:
	if (! st->gl) {
	    os->Push(WC_C_HZ_TILDA);
	    os->Push(WC_C_HZ_SI);
	    st->gl = 1;
	}
	os->Push((char)((cc.code >> 8) & 0x7f));
	os->Push((char)(cc.code & 0x7f));
	return;
    case WC_CCS_UNKNOWN_W:
	if (WcOption.no_replace)
	    return;
	if (st->gl) {
	    os->Push(WC_C_HZ_TILDA);
	    os->Push(WC_C_HZ_SO);
	    st->gl = 0;
	}
	os->Push(WC_REPLACE_W);
	return;
    case WC_CCS_UNKNOWN:
	if (WcOption.no_replace)
	    return;
	if (st->gl) {
	    os->Push(WC_C_HZ_TILDA);
	    os->Push(WC_C_HZ_SO);
	    st->gl = 0;
	}
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

void
wc_push_to_hz_end(Str os, wc_status *st)
{
    if (st->gl) {
	os->Push(WC_C_HZ_TILDA);
	os->Push(WC_C_HZ_SO);
	st->gl = 0;
    }
}
