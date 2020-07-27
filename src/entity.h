#pragma once
#include <tuple>
#include <string_view>

const char *conv_entity(unsigned int ch);
int getescapechar(const char **s);
inline int getescapechar(char **s)
{
    return getescapechar(const_cast<const char **>(s));
}

std::pair<const char *, std::string_view> getescapecmd(const char *s);
char *html_unquote(const char *str);
