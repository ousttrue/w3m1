#pragma once
#include "Str.h"
#include "ces.h"
#include "ccs.h"

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
extern wc_ces_info WcCesInfo[];
