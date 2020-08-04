#include "option.h"
#include <string.h>
#include <gc.h>
#define New_N(type,n) ((type*)GC_MALLOC((n)*sizeof(type)))


#include "status.h"
#include "ucs.h"


wc_option WcOption = {
    WC_OPT_DETECT_ON,	/* auto_detect */
    true,		/* use_combining */
    true,		/* use_language_tag */
    true,		/* ucs_conv */
    false,		/* pre_conv */
    true,		/* fix_width_conv */
    false,		/* use_gb12345_map */
    false,		/* use_jisx0201 */
    false,		/* use_jisc6226 */
    false,		/* use_jisx0201k */
    false,		/* use_jisx0212 */
    false,		/* use_jisx0213 */
    true,		/* strict_iso2022 */
    false,		/* gb18030_as_ucs */
    false,		/* no_replace */
    true,		/* use_wide */
    false,		/* east_asian_width */
};

static wc_status output_st;
static wc_option output_option;
static bool output_set = false;

#define wc_option_cmp(opt1, opt2) \
    memcmp((void *)(opt1), (void *)(opt2), sizeof(wc_option))

void wc_input_init(CharacterEncodingScheme ces, wc_status *st)
{
    wc_gset *gset;
    int i, g;

    st->ces_info = &GetCesInfo(ces);
    gset = st->ces_info->gset;

    st->state = 0;
    st->g0_ccs = WC_CCS_NONE;
    st->g1_ccs = WC_CCS_NONE;
    st->design[0] = gset[0].ccs;
    st->design[1] = gset[1].ccs;	/* for ISO-2022-JP/EUC-JP */ 
    st->design[2] = WC_CCS_NONE;
    st->design[3] = WC_CCS_NONE;
    st->gl = 0;
    st->gr = 1;
    st->ss = 0;

    for (i = 0; gset[i].ccs; i++) {
	if (gset[i].init) {
	    g = gset[i].g & 0x03;
	    if (! st->design[g])
		st->design[g] = gset[i].ccs;
	}
    }

    st->tag = NULL;
    st->ntag = 0;

}

void
wc_output_init(CharacterEncodingScheme ces, wc_status *st)
{
    wc_gset *gset;

    size_t i, n, nw;


    if (output_set && ces == output_st.ces_info->id &&
	! wc_option_cmp(&WcOption, &output_option)) {
	*st = output_st;
	return;
    }

    st->state = 0;
    st->ces_info = &GetCesInfo(ces);
    gset = st->ces_info->gset;

    st->g0_ccs = ((ces == WC_CES_ISO_2022_JP || ces == WC_CES_ISO_2022_JP_2 ||
	ces == WC_CES_ISO_2022_JP_3) && WcOption.use_jisx0201)
	? WC_CCS_JIS_X_0201 : gset[0].ccs;
    st->g1_ccs = ((ces == WC_CES_ISO_2022_JP || ces == WC_CES_ISO_2022_JP_2 ||
	ces == WC_CES_ISO_2022_JP_3) && WcOption.use_jisc6226)
	? WC_CCS_JIS_C_6226 : gset[1].ccs;
    st->design[0] = st->g0_ccs;
    st->design[1] = WC_CCS_NONE;
    st->design[2] = WC_CCS_NONE;
    st->design[3] = WC_CCS_NONE;
    st->gl = 0;
    st->gr = 0;
    st->ss = 0;

    if (ces & WC_CES_T_ISO_2022)
	wc_create_gmap(st);


    st->tag = NULL;
    st->ntag = 0;

    if (! WcOption.ucs_conv) {
	st->tlist = NULL;
	st->tlistw = NULL;
    } else {

    for (i = n = nw = 0; gset[i].ccs; i++) {
	if (WC_CCS_IS_WIDE(gset[i].ccs))
	    nw++;
	else
	    n++;
    }
    st->tlist = New_N(wc_table *, n + 1);
    st->tlistw = New_N(wc_table *, nw + 1);
    for (i = n = nw = 0; gset[i].ccs; i++) {
	if (WC_CCS_IS_WIDE(gset[i].ccs)) {
	    switch (gset[i].ccs) {
	    case WC_CCS_JIS_X_0212:
		if (! WcOption.use_jisx0212)
		    continue;
		break;
	    case WC_CCS_JIS_X_0213_1:
	    case WC_CCS_JIS_X_0213_2:
		if (! WcOption.use_jisx0213)
		    continue;
		break;
	    case WC_CCS_GB_2312:
		if (WcOption.use_gb12345_map &&
		    ces != WC_CES_GBK && ces != WC_CES_GB18030) {
		    st->tlistw[nw++] = wc_get_ucs_table(WC_CCS_GB_12345);
		    continue;
		}
		break;
	    }
	    st->tlistw[nw++] = wc_get_ucs_table(gset[i].ccs);
	} else {
	    switch (gset[i].ccs) {
	    case WC_CCS_JIS_X_0201K:
		if (! WcOption.use_jisx0201k)
		    continue;
		break;
	    }
	    st->tlist[n++] = wc_get_ucs_table(gset[i].ccs);
	}
    }
    st->tlist[n] = NULL;
    st->tlistw[nw] = NULL;
    }


    output_st = *st;
    output_set = true;
    output_option = WcOption;
}

bool
wc_ces_has_ccs(CodedCharacterSet ccs, wc_status *st)
{
    wc_gset *gset = st->ces_info->gset;
    int i;

    for (i = 0; gset[i].ccs; i++) {
	if (ccs == gset[i].ccs)
	    return true;
    }
    return false;
}
