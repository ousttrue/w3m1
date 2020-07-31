#include "option.h"
#include "viet.h"
#include "detect.h"
#include "wtf.h"
#include "search.h"
#include "ucs.h"
#include "map/tcvn57123_tcvn5712.map"


uint8_t wc_c0_tcvn57122_map[ 0x20 ] = {
    0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,     
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,     
};
uint8_t wc_c0_viscii112_map[ 0x20 ] = {
    0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,     
    0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,     
};
uint8_t wc_c0_vps2_map[ 0x20 ] = {
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,     
    1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,     
};
static uint8_t tcvn5712_precompose_map[ 0x100 ] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*     A           E           I                 O */
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
/*                 U           Y                   */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
/*     a           e           i                 o */
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
/*                 u           y                   */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*     A( A^ E^ O^ O+ U+    a( a^ e^ o^ o+ u+      */
    0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0,
/*  `  ?  ~  '  .                                  */
    2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static uint8_t cp1258_precompose_map[ 0x100 ] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*     A           E           I                 O */
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
/*                 U           Y                   */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
/*     a           e           i                 o */
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
/*                 u           y                   */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*        A^ A(                   E^    `          */
    0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0,
/*        ?     O^ O+                      U+ ~    */
    0, 0, 2, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0,
/*        a^ a(                   e^    '          */
    0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0,
/*        .     o^ o+                      u+      */
    0, 0, 2, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
};

uint32_t
wc_tcvn5712_precompose(uint8_t c1, uint8_t c2)
{
    if (tcvn5712_precompose_map[c1] == 1 && tcvn5712_precompose_map[c2] == 2)
	return ((uint32_t)c1 << 8) | c2;
    else
	return 0;
}

wc_wchar_t
wc_tcvn57123_to_tcvn5712(wc_wchar_t cc)
{
    wc_map *map;

    map = wc_map_search((uint16_t)(cc.code & 0x7f7f),
	tcvn57123_tcvn5712_map, N_tcvn57123_tcvn5712_map);
    if (map) {
	cc.ccs = (map->code2 < 0x20) ? WC_CCS_TCVN_5712_2 : WC_CCS_TCVN_5712_1;
	cc.code = map->code2 | 0x80;
    } else {
	cc.ccs = WC_CCS_UNKNOWN;
    }
    return cc;
}

uint32_t
wc_cp1258_precompose(uint8_t c1, uint8_t c2)
{
    if (cp1258_precompose_map[c1] == 1 && cp1258_precompose_map[c2] == 2)
	return ((uint32_t)c1 << 8) | c2;
    else
	return 0;
}

Str
wc_conv_from_viet(Str is, CharacterEncodingScheme ces)
{
    Str os;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->Size();
    uint8_t *p;
    CodedCharacterSet ccs1 = WcCesInfo[WC_CCS_INDEX(ces)].gset[1].ccs;
    CodedCharacterSet ccs2 = WcCesInfo[WC_CCS_INDEX(ces)].gset[2].ccs;
    uint8_t *map = NULL;

    switch (ces) {
    case WC_CES_TCVN_5712:
	map = wc_c0_tcvn57122_map;
	break;
    case WC_CES_VISCII_11:
	map = wc_c0_viscii112_map;
	break;
    case WC_CES_VPS:
	map = wc_c0_vps2_map;
	break;
    }

    wc_create_detect_map(ces, false);
    for (p = sp; p < ep && ! WC_DETECT_MAP[*p]; p++)
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->Size());
    if (p > sp)
	os->Push(is->ptr, (int)(p - sp));

    for (; p < ep; p++) {
	if (*p & 0x80)
	    wtf_push(os, ccs1, (uint32_t)*p);
	else if (*p < 0x20 && map[*p])
	    wtf_push(os, ccs2, (uint32_t)*p);
	else
	    os->Push((char)*p);
    }
    return os;
}

void
wc_push_to_viet(Str os, wc_wchar_t cc, wc_status *st)
{
    CodedCharacterSet ccs1 = st->ces_info->gset[1].ccs;
    CodedCharacterSet ccs2 = WC_CCS_NONE, ccs3 = WC_CCS_NONE;
    uint8_t *map = NULL;

    switch (st->ces_info->id) {
    case WC_CES_CP1258:
	ccs3 = st->ces_info->gset[2].ccs;
	break;
    case WC_CES_TCVN_5712:
	map = wc_c0_tcvn57122_map;
	ccs2 = st->ces_info->gset[2].ccs;
	ccs3 = st->ces_info->gset[3].ccs;
	break;
    case WC_CES_VISCII_11:
	map = wc_c0_viscii112_map;
	ccs2 = st->ces_info->gset[2].ccs;
	break;
    case WC_CES_VPS:
	map = wc_c0_vps2_map;
	ccs2 = st->ces_info->gset[2].ccs;
	break;
    }

  while (1) {
    if (cc.ccs == ccs1) {
	os->Push((char)(cc.code | 0x80));
	return;
    } else if (cc.ccs == ccs2) {
	os->Push((char)(cc.code & 0x7f));
	return;
    } else if (cc.ccs == ccs3) {
	os->Push((char)((cc.code >> 8) & 0xff));
	os->Push((char)(cc.code & 0xff));
	return;
    }
    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
	if (cc.code < 0x20 && map && map[cc.code])
	    os->Push(' ');
	else
	    os->Push((char)cc.code);
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
wc_char_conv_from_viet(uint8_t c, wc_status *st)
{
    Str os = Strnew_size(1);
    uint8_t *map = NULL;

    switch (st->ces_info->id) {
    case WC_CES_TCVN_5712:
	map = wc_c0_tcvn57122_map;
	break;
    case WC_CES_VISCII_11:
	map = wc_c0_viscii112_map;
	break;
    case WC_CES_VPS:
	map = wc_c0_vps2_map;
	break;
    }

    if (c & 0x80)
	wtf_push(os, st->ces_info->gset[1].ccs, (uint32_t)c);
    else if (c < 0x20 && map[c])
	wtf_push(os, st->ces_info->gset[2].ccs, (uint32_t)c);
    else
	os->Push((char)c);
    return os;
}
