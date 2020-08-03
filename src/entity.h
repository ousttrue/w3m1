#pragma once
#include <tuple>
#include <string_view>
#include "ces.h"

uint32_t getescapechar(const char **s);
inline uint32_t getescapechar(char **s)
{
    return getescapechar(const_cast<const char **>(s));
}

std::pair<const char *, std::string_view> getescapecmd(const char *s, CharacterEncodingScheme ces);
char *html_unquote(const char *str, CharacterEncodingScheme ces);
