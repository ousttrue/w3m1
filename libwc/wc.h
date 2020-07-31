
#ifndef _WC_WC_H
#define _WC_WC_H

#include <Str.h>
#include "ces.h"

#include "ccs.h"

#define WC_FALSE 0
#define WC_TRUE 1

#define WC_OPT_DETECT_OFF 0
#define WC_OPT_DETECT_ISO_2022 1
#define WC_OPT_DETECT_ON 2


extern uint8_t WC_DETECT_MAP[];

struct wc_option
{
    uint8_t auto_detect;     /* automatically charset detect */
    bool use_combining;    /* use combining characters */
    bool use_language_tag; /* use language_tags */
    bool ucs_conv;         /* charset conversion using Unicode */
    bool pre_conv;         /* previously charset conversion */
    bool fix_width_conv;   /* not allowed conversion between different
				   width charsets */
    bool use_gb12345_map;  /* use GB 12345 Unicode map instead of
				   GB 2312 Unicode map */
    bool use_jisx0201;     /* use JIS X 0201 Roman instead of US_ASCII */
    bool use_jisc6226;     /* use JIS C 6226:1978 instead of JIS X 0208 */
    bool use_jisx0201k;    /* use JIS X 0201 Katakana */
    bool use_jisx0212;     /* use JIS X 0212 */
    bool use_jisx0213;     /* use JIS X 0213 */
    bool strict_iso2022;   /* strict ISO 2022 */
    bool gb18030_as_ucs;   /* treat 4 bytes char. of GB18030 as Unicode */
    bool no_replace;       /* don't output replace character */
    bool use_wide;         /* use wide characters */
    bool east_asian_width; /* East Asian Ambiguous characters are wide */
};
extern wc_option WcOption;

extern char *WcReplace;
extern char *WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

#endif
