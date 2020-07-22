
#ifndef _WC_TYPES_H
#define _WC_TYPES_H

#include <Str.h>
#include <config.h>
#if defined(HAVE_STDINT_H)
#include <stdint.h>
#elif defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif

typedef unsigned char wc_uchar;
#if defined(HAVE_STDINT_H) || defined(HAVE_INTTYPES_H)
typedef uint8_t wc_uint8;
typedef uint16_t wc_uint16;
typedef uint32_t wc_uint32;
#else
typedef unsigned char wc_uint8;
typedef unsigned short wc_uint16;
typedef unsigned long wc_uint32;
#endif

typedef wc_uint32 wc_ccs;
typedef wc_uint32 wc_ces;
typedef wc_uint32 wc_locale;
typedef wc_uchar wc_bool;

struct wc_wchar_t
{
    wc_ccs ccs;
    wc_uint32 code;
};

struct wc_map
{
    wc_uint16 code;
    wc_uint16 code2;
};

struct wc_map3
{
    wc_uint16 code;
    wc_uint16 code2;
    wc_uint16 code3;
};

typedef wc_wchar_t (*WcConvFunc)(wc_ccs, wc_uint16);

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
    wc_uchar g;
    wc_bool init;
};

typedef Str (*ConvFromFunc)(Str, wc_ces);
typedef void (*PushToFunc)(Str, wc_wchar_t, struct wc_status *);
typedef Str (*CharConvFunc)(wc_uchar, struct wc_status *);

struct wc_ces_info
{
    wc_ces id;
    char *name;
    char *desc;
    wc_gset *gset;
    wc_uchar *gset_ext;
    ConvFromFunc conv_from;
    PushToFunc push_to;
    CharConvFunc char_conv;
};

struct wc_ces_list
{
    wc_ces id;
    char *name;
    char *desc;
};

struct wc_option
{
    wc_uint8 auto_detect;     /* automatically charset detect */
    wc_bool use_combining;    /* use combining characters */
    wc_bool use_language_tag; /* use language_tags */
    wc_bool ucs_conv;         /* charset conversion using Unicode */
    wc_bool pre_conv;         /* previously charset conversion */
    wc_bool fix_width_conv;   /* not allowed conversion between different
				   width charsets */
    wc_bool use_gb12345_map;  /* use GB 12345 Unicode map instead of
				   GB 2312 Unicode map */
    wc_bool use_jisx0201;     /* use JIS X 0201 Roman instead of US_ASCII */
    wc_bool use_jisc6226;     /* use JIS C 6226:1978 instead of JIS X 0208 */
    wc_bool use_jisx0201k;    /* use JIS X 0201 Katakana */
    wc_bool use_jisx0212;     /* use JIS X 0212 */
    wc_bool use_jisx0213;     /* use JIS X 0213 */
    wc_bool strict_iso2022;   /* strict ISO 2022 */
    wc_bool gb18030_as_ucs;   /* treat 4 bytes char. of GB18030 as Unicode */
    wc_bool no_replace;       /* don't output replace character */
    wc_bool use_wide;         /* use wide characters */
    wc_bool east_asian_width; /* East Asian Ambiguous characters are wide */
};

struct wc_status
{
    wc_ces_info *ces_info;
    wc_uint8 gr;
    wc_uint8 gl;
    wc_uint8 ss;
    wc_ccs g0_ccs;
    wc_ccs g1_ccs;
    wc_ccs design[4];

    wc_table **tlist;
    wc_table **tlistw;

    int state;

    Str tag;
    int ntag;
    wc_uint32 base;
    int shift;
};

#endif
