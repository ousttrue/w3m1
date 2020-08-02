#pragma once
#include "ces.h"
#include <string_view>

CharacterEncodingScheme wc_guess_charset(char *charset, CharacterEncodingScheme orig);
CharacterEncodingScheme wc_guess_charset_short(const char *charset, CharacterEncodingScheme orig);
CharacterEncodingScheme wc_guess_locale_charset(std::string_view locale, CharacterEncodingScheme orig);
CharacterEncodingScheme wc_charset_to_ces(const char *charset);
CharacterEncodingScheme wc_charset_short_to_ces(const char *charset);
CharacterEncodingScheme wc_locale_to_ces(const char *locale);
CharacterEncodingScheme wc_guess_8bit_charset(CharacterEncodingScheme orig);
char *wc_ces_to_charset(CharacterEncodingScheme ces);
char *wc_ces_to_charset_desc(CharacterEncodingScheme ces);
bool wc_check_ces(CharacterEncodingScheme ces);

struct wc_ces_list
{
    CharacterEncodingScheme id;
    char *name;
    char *desc;
};
wc_ces_list *wc_get_ces_list(void);
