
#pragma once
#include <Str.h>
#include <config.h>
#include <stdint.h>
#include "ces.h"

typedef uint32_t wc_ccs;
typedef uint32_t wc_locale;

struct wc_wchar_t
{
    wc_ccs ccs;
    uint32_t code;
};

struct wc_map
{
    uint16_t code;
    uint16_t code2;
};

struct wc_map3
{
    uint16_t code;
    uint16_t code2;
    uint16_t code3;
};

typedef wc_wchar_t (*WcConvFunc)(wc_ccs, uint16_t);

struct wc_table
{
    wc_ccs ccs;
    size_t n;
    wc_map *map;
    WcConvFunc conv;
};

struct wc_gset
{
    wc_ccs ccs;
    uint8_t g;
    bool init;
};

typedef Str (*ConvFromFunc)(Str, CharacterEncodingScheme);
typedef void (*PushToFunc)(Str, wc_wchar_t, struct wc_status *);
typedef Str (*CharConvFunc)(uint8_t, struct wc_status *);

struct wc_ces_info
{
    CharacterEncodingScheme id;
    char *name;
    char *desc;
    wc_gset *gset;
    uint8_t *gset_ext;
    ConvFromFunc conv_from;
    PushToFunc push_to;
    CharConvFunc char_conv;
};

struct wc_ces_list
{
    CharacterEncodingScheme id;
    char *name;
    char *desc;
};

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

struct wc_status
{
    wc_ces_info *ces_info;
    uint8_t gr;
    uint8_t gl;
    uint8_t ss;
    wc_ccs g0_ccs;
    wc_ccs g1_ccs;
    wc_ccs design[4];

    wc_table **tlist;
    wc_table **tlistw;

    int state;

    Str tag;
    int ntag;
    uint32_t base;
    int shift;
};
