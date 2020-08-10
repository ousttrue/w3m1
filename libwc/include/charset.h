#pragma once
#include "ces.h"
#include <string_view>

CharacterEncodingScheme wc_guess_charset(const char *charset, CharacterEncodingScheme orig);
CharacterEncodingScheme wc_guess_charset_short(const char *charset, CharacterEncodingScheme orig);
CharacterEncodingScheme wc_guess_locale_charset(std::string_view locale, CharacterEncodingScheme orig);
CharacterEncodingScheme wc_charset_to_ces(const char *charset);
CharacterEncodingScheme wc_charset_short_to_ces(const char *charset);
CharacterEncodingScheme wc_locale_to_ces(const char *locale);
CharacterEncodingScheme wc_guess_8bit_charset(CharacterEncodingScheme orig);
