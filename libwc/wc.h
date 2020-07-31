
#ifndef _WC_WC_H
#define _WC_WC_H

#include <Str.h>
#include "ces.h"
#include "wc_types.h"
#include "ccs.h"

#define WC_FALSE 0
#define WC_TRUE 1

#define WC_OPT_DETECT_OFF 0
#define WC_OPT_DETECT_ISO_2022 1
#define WC_OPT_DETECT_ON 2

enum LocaleTypes : uint32_t
{
    WC_LOCALE_NONE = 0,
    WC_LOCALE_JA_JP = 1,
    WC_LOCALE_ZH_CN = 2,
    WC_LOCALE_ZH_TW = 3,
    WC_LOCALE_ZH_HK = 4,
    WC_LOCALE_KO_KR = 5,
};

extern uint8_t WC_DETECT_MAP[];

extern wc_ces_info WcCesInfo[];
extern wc_option WcOption;
extern LocaleTypes WcLocale;
extern char *WcReplace;
extern char *WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

extern Str wc_Str_conv(Str is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
extern Str wc_Str_conv_strict(Str is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
extern Str wc_Str_conv_with_detect(Str is, CharacterEncodingScheme *f_ces, CharacterEncodingScheme hint, CharacterEncodingScheme t_ces);
inline Str wc_conv(const char *is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv(Strnew(is), f_ces, t_ces);
}
inline Str wc_conv_n(const char *is, int n, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv(Strnew_charp_n(is, n), f_ces, t_ces);
}
inline Str wc_conv_strict(const char *is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv_strict(Strnew(is), f_ces, t_ces);
}
inline Str wc_conv_n_strict(const char *is, int n, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv_strict(Strnew_charp_n(is, n), f_ces, t_ces);
}
inline Str wc_conv_with_detect(const char *is, CharacterEncodingScheme *f_ces, CharacterEncodingScheme hint, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv_with_detect(Strnew(is), f_ces, hint, t_ces);
}
inline Str wc_conv_n_with_detect(const char *is, int n, CharacterEncodingScheme *f_ces, CharacterEncodingScheme hint, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv_with_detect(Strnew_charp_n(is, n), f_ces, hint, t_ces);
}

extern void wc_output_init(CharacterEncodingScheme ces, wc_status *st);
extern void wc_push_end(Str os, wc_status *st);
extern bool wc_ces_has_ccs(wc_ccs ccs, wc_status *st);

extern void wc_char_conv_init(CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
extern Str wc_char_conv(char c);

extern void wc_putc_init(CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
extern void wc_putc(char *c, FILE *f);
extern void wc_putc_end(FILE *f);
extern void wc_putc_clear_status(void);

extern void wc_create_detect_map(CharacterEncodingScheme ces, bool esc);

extern CharacterEncodingScheme wc_guess_charset(char *charset, CharacterEncodingScheme orig);
CharacterEncodingScheme wc_guess_charset_short(const char *charset, CharacterEncodingScheme orig);
extern CharacterEncodingScheme wc_guess_locale_charset(char *locale, CharacterEncodingScheme orig);
CharacterEncodingScheme wc_charset_to_ces(const char *charset);
CharacterEncodingScheme wc_charset_short_to_ces(const char *charset);
extern CharacterEncodingScheme wc_locale_to_ces(char *locale);
extern CharacterEncodingScheme wc_guess_8bit_charset(CharacterEncodingScheme orig);
extern char *wc_ces_to_charset(CharacterEncodingScheme ces);
extern char *wc_ces_to_charset_desc(CharacterEncodingScheme ces);
extern bool wc_check_ces(CharacterEncodingScheme ces);
extern wc_ces_list *wc_get_ces_list(void);

#endif
