#pragma once
#include <tuple>
#include <string_view>
#include "ces.h"

const char *conv_entity(unsigned int ch, CharacterEncodingScheme ces);

int getescapechar(const char **s);
inline int getescapechar(char **s)
{
    return getescapechar(const_cast<const char **>(s));
}

std::pair<const char *, std::string_view> getescapecmd(const char *s, CharacterEncodingScheme ces);
char *html_unquote(const char *str, CharacterEncodingScheme ces);
