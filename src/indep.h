/* $Id: indep.h,v 1.16 2003/09/22 21:02:19 ukai Exp $ */
#ifndef INDEP_H
#define INDEP_H

#include "config.h"
#include <wc.h>

#ifndef TRUE
#define TRUE 1
#endif /* TRUE */
#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

#define RAW_MODE 0
#define PAGER_MODE 1
#define HTML_MODE 2
#define HEADER_MODE 3

extern unsigned char QUOTE_MAP[];
extern char *HTML_QUOTE_MAP[];
#define HTML_QUOTE_MASK 0x07   /* &, <, >, " */
#define SHELL_UNSAFE_MASK 0x08 /* [^A-Za-z0-9_./:\200-\377] */

#define FILE_QUOTE_MASK 0x30   /* [\0- #%&+:?\177-\377] */
#define URL_UNSAFE_MASK 0x70   /* [^A-Za-z0-9_$\-.] */
#define GET_QUOTE_TYPE(c) QUOTE_MAP[(int)(unsigned char)(c)]
#define is_html_quote(c) (GET_QUOTE_TYPE(c) & HTML_QUOTE_MASK)
#define is_shell_unsafe(c) (GET_QUOTE_TYPE(c) & SHELL_UNSAFE_MASK)
#define is_file_quote(c) (GET_QUOTE_TYPE(c) & FILE_QUOTE_MASK)
#define is_url_unsafe(c) (GET_QUOTE_TYPE(c) & URL_UNSAFE_MASK)
#define html_quote_char(c) HTML_QUOTE_MAP[(int)is_html_quote(c)]

#define url_unquote_char(pstr) \
    ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2])) ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) | GET_MYCDIGIT((*(pstr))[-1])) : -1)

extern clen_t strtoclen(const char *s);
extern char *allocStr(const char *s, int len);
extern int strCmp(const void *s1, const void *s2);
extern char *currentdir(void);
char *cleanupName(const char *name);
char *expandPath(const char *name);
#ifndef HAVE_STRCHR
extern char *strchr(const char *s, int c);
#endif /* not HAVE_STRCHR */
#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif /* not HAVE_STRCASECMP */
#ifndef HAVE_STRCASESTR
extern char *strcasestr(const char *s1, const char *s2);
#endif
extern int strcasemstr(char *str, char *srch[], char **ret_ptr);

char *remove_space(const char *str);

extern int non_null(char *s);

extern void cleanup_line(Str s, int mode);

std::string html_quote(std::string_view str);
char *html_quote(const char *str);
inline char *html_quote(Str str)
{
    return str->ptr;
}

extern char *file_quote(char *str);
extern char *file_unquote(std::string_view str);
GCStr *UrlEncode(GCStr *src);
GCStr *UrlDecode(GCStr* src, bool is_form, bool safe);
inline Str Str_form_unquote(Str x)
{
    return UrlDecode(x, TRUE, FALSE);
}
extern char *shell_quote(std::string_view str);

extern char *w3m_auxbin_dir();
extern char *w3m_lib_dir();
extern char *w3m_etc_dir();
extern char *w3m_conf_dir();
extern char *w3m_help_dir();

std::pair<const char *, std::string_view> getescapecmd(const char *s, CharacterEncodingScheme ces);
char *html_unquote(const char *str, CharacterEncodingScheme ces);
void ToUpper(Str str);
void ToLower(Str str);
void StripLeft(Str str);
void StripRight(Str str);
inline void Strip(Str str)
{
    StripLeft(str);
    StripRight(str);
}
Str Sprintf(const char *fmt, ...);

#endif /* INDEP_H */
