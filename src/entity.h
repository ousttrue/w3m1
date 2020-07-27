#pragma once

const char *conv_entity(unsigned int ch);
int getescapechar(const char **s);
inline int getescapechar(char **s)
{
    return getescapechar(const_cast<const char **>(s));
}
char *getescapecmd(const char **s);
inline char *getescapecmd(char **s)
{
    return getescapecmd(const_cast<const char **>(s));
}
char *html_unquote(const char *str);
