#pragma once
#include "Str.h"
#include "ces.h"
#include "ccs.h"

using ConvFromFunc = Str (*)(Str, CharacterEncodingScheme);
using PushToFunc = void (*)(Str, wc_wchar_t, struct wc_status *);
using CharConvFunc = Str (*)(uint8_t, struct wc_status *);

struct wc_ces_info
{
    CharacterEncodingScheme id;
    const char *name;
    const char *desc;
    wc_gset *gset;
    uint8_t *gset_ext;
    ConvFromFunc conv_from;
    PushToFunc push_to;
    CharConvFunc char_conv;

    bool has_ccs(CodedCharacterSet ccs) const
    {
        for (auto i = 0; gset[i].ccs; i++)
        {
            if (ccs == gset[i].ccs)
                return true;
        }
        return false;
    }
};

bool wc_check_ces(CharacterEncodingScheme ces);
const wc_ces_info &GetCesInfo(CharacterEncodingScheme ces);
struct wc_ces_list
{
    CharacterEncodingScheme id;
    const char *name;
    const char *desc;
};
wc_ces_list *wc_get_ces_list();
const char *wc_ces_to_charset(CharacterEncodingScheme ces);
const char *wc_ces_to_charset_desc(CharacterEncodingScheme ces);
