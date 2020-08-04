#include "option.h"
#include "status.h"
#include "detect.h"
#include "conv.h"
#include "wtf.h"
#include "iso2022.h"
#include "hz.h"
#include "ucs.h"
#include "utf8.h"
#include "utf7.h"

char *WcReplace = "?";
char *WcReplaceW = "??";

static Str wc_conv_to_ces(Str is, CharacterEncodingScheme ces);

Str wc_Str_conv(Str is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    if (f_ces != WC_CES_WTF)
        is = (*GetCesInfo(f_ces).conv_from)(is, f_ces);
    if (t_ces != WC_CES_WTF)
        return wc_conv_to_ces(is, t_ces);
    else
        return is;
}

Str wc_Str_conv_strict(Str is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    Str os;
    wc_option opt = WcOption;

    WcOption.strict_iso2022 = true;
    WcOption.no_replace = true;
    WcOption.fix_width_conv = false;
    os = wc_Str_conv(is, f_ces, t_ces);
    WcOption = opt;
    return os;
}

static Str
wc_conv_to_ces(Str is, CharacterEncodingScheme ces)
{
    Str os;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->Size();
    uint8_t *p;
    wc_status st;

    switch (ces)
    {
    case WC_CES_HZ_GB_2312:
        for (p = sp; p < ep && *p != '~' && *p < 0x80; p++)
            ;
        break;
    case WC_CES_TCVN_5712:
    case WC_CES_VISCII_11:
    case WC_CES_VPS:
        for (p = sp; p < ep && 0x20 <= *p && *p < 0x80; p++)
            ;
        break;
    default:
        for (p = sp; p < ep && *p < 0x80; p++)
            ;
        break;
    }
    if (p == ep)
        return is;

    os = Strnew_size(is->Size());
    if (p > sp)
        p--; /* for precompose */
    if (p > sp)
        os->Push(is->ptr, (int)(p - sp));

    wc_output_init(ces, &st);

    switch (ces)
    {
    case WC_CES_ISO_2022_JP:
    case WC_CES_ISO_2022_JP_2:
    case WC_CES_ISO_2022_JP_3:
    case WC_CES_ISO_2022_CN:
    case WC_CES_ISO_2022_KR:
    case WC_CES_HZ_GB_2312:
    case WC_CES_TCVN_5712:
    case WC_CES_VISCII_11:
    case WC_CES_VPS:

    case WC_CES_UTF_8:
    case WC_CES_UTF_7:

        while (p < ep)
            (*st.ces_info->push_to)(os, wtf_parse(&p), &st);
        break;
    default:
        while (p < ep)
        {
            if (*p < 0x80 && wtf_width(p + 1))
            {
                os->Push((char)*p);
                p++;
            }
            else
                (*st.ces_info->push_to)(os, wtf_parse(&p), &st);
        }
        break;
    }

    wc_push_end(os, &st);

    return os;
}

Str wc_Str_conv_with_detect(Str is, CharacterEncodingScheme *f_ces, CharacterEncodingScheme hint, CharacterEncodingScheme t_ces)
{
    CharacterEncodingScheme detect;

    if (*f_ces == WC_CES_WTF || hint == WC_CES_WTF)
    {
        *f_ces = WC_CES_WTF;
        detect = WC_CES_WTF;
    }
    else if (WcOption.auto_detect == WC_OPT_DETECT_OFF)
    {
        *f_ces = hint;
        detect = hint;
    }
    else
    {
        if (*f_ces & WC_CES_T_8BIT)
            hint = *f_ces;
        detect = wc_auto_detect(is->ptr, is->Size(), hint);
        if (WcOption.auto_detect == WC_OPT_DETECT_ON)
        {
            if ((detect & WC_CES_T_8BIT) ||
                ((detect & WC_CES_T_NASCII) && !(*f_ces & WC_CES_T_8BIT)))
                *f_ces = detect;
        }
        else
        {
            if ((detect & WC_CES_T_ISO_2022) && !(*f_ces & WC_CES_T_8BIT))
                *f_ces = detect;
        }
    }
    return wc_Str_conv(is, detect, t_ces);
}

void wc_push_end(Str os, wc_status *st)
{
    if (st->ces_info->id & WC_CES_T_ISO_2022)
        wc_push_to_iso2022_end(os, st);
    else if (st->ces_info->id == WC_CES_HZ_GB_2312)
        wc_push_to_hz_end(os, st);

    else if (st->ces_info->id == WC_CES_UTF_8)
        wc_push_to_utf8_end(os, st);
    else if (st->ces_info->id == WC_CES_UTF_7)
        wc_push_to_utf7_end(os, st);

}

//
// Entity decode & character encoding ?
//
// https://dev.w3.org/html5/html-author/charref
//
// ucs4 codepoint 38 => '&'
//
const char *from_unicode(uint32_t codepoint, CharacterEncodingScheme ces)
{
    if (codepoint <= WC_C_UCS4_END)
    {
        uint8_t utf8[7];
        wc_ucs_to_utf8(codepoint, utf8);
        return wc_conv((char *)utf8, WC_CES_UTF_8, ces)->ptr;
    }

    return "?";
}
