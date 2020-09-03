#include <sstream>
#include <stdio.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include "indep.h"

#include "gc_helper.h"

#include "myctype.h"
#include "entity.h"

unsigned char QUOTE_MAP[0x100] = {
    /* NUL SOH STX ETX EOT ENQ ACK BEL  BS  HT  LF  VT  FF  CR  SO  SI */
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    /* DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN  EM SUB ESC  FS  GS  RS  US */
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    /* SPC   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
    24, 72, 76, 40, 8, 40, 41, 72, 72, 72, 72, 40, 72, 8, 0, 64,
    /*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 72, 74, 72, 75, 40,
    /*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 72, 72, 72, 0,
    /*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~ DEL */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 72, 72, 72, 24,

    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    // keep indent
};

const char *HTML_QUOTE_MAP[] = {
    NULL,
    "&amp;",
    "&lt;",
    "&gt;",
    "&quot;",
    NULL,
    NULL,
    NULL,
};

const char *html_quote_char(int c)
{
    return HTML_QUOTE_MAP[(int)is_html_quote(c)];
}

clen_t
strtoclen(const char *s)
{
#ifdef HAVE_STRTOLL
    return strtoll(s, NULL, 10);
#elif defined(HAVE_STRTOQ)
    return strtoq(s, NULL, 10);
#elif defined(HAVE_ATOLL)
    return atoll(s);
#elif defined(HAVE_ATOQ)
    return atoq(s);
#else
    return atoi(s);
#endif
}

#ifndef HAVE_BCOPY
void bcopy(const void *src, void *dest, int len)
{
    int i;
    if (src == dest)
        return;
    if (src < dest)
    {
        for (i = len - 1; i >= 0; i--)
            ((char *)dest)[i] = ((const char *)src)[i];
    }
    else
    { /* src > dest */
        for (i = 0; i < len; i++)
            ((char *)dest)[i] = ((const char *)src)[i];
    }
}

void bzero(void *ptr, int len)
{
    int i;
    char *p = ptr;
    for (i = 0; i < len; i++)
        *(p++) = 0;
}
#endif /* not HAVE_BCOPY */

char *
allocStr(const char *s, int len)
{
    char *ptr;

    if (s == NULL)
        return NULL;
    if (len < 0)
        len = strlen(s);
    ptr = NewAtom_N(char, len + 1);
    if (ptr == NULL)
    {
        fprintf(stderr, "fm: Can't allocate string. Give me more memory!\n");
        exit(-1);
    }
    bcopy(s, ptr, len);
    ptr[len] = '\0';
    return ptr;
}

int strCmp(const void *s1, const void *s2)
{
    return strcmp(*(const char **)s1, *(const char **)s2);
}

char *
currentdir()
{
    char *path;
#ifdef HAVE_GETCWD
#ifdef MAXPATHLEN
    path = NewAtom_N(char, MAXPATHLEN);
    getcwd(path, MAXPATHLEN);
#else
    path = getcwd(NULL, 0);
#endif
#else /* not HAVE_GETCWD */
#ifdef HAVE_GETWD
    path = NewAtom_N(char, 1024);
    getwd(path);
#else  /* not HAVE_GETWD */
    FILE *f;
    char *p;
    path = NewAtom_N(char, 1024);
    f = popen("pwd", "r");
    fgets(path, 1024, f);
    pclose(f);
    for (p = path; *p; p++)
        if (*p == '\n')
        {
            *p = '\0';
            break;
        }
#endif /* not HAVE_GETWD */
#endif /* not HAVE_GETCWD */
    return path;
}

char *cleanupName(const char *name)
{
    auto buf = allocStr(name, -1);
    auto p = buf;
    auto q = name;
    while (*q != '\0')
    {
        if (strncmp(p, "/../", 4) == 0)
        { /* foo/bar/../FOO */
            if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0)
            {
                /* ../../       */
                p += 3;
                q += 3;
            }
            else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0)
            {
                /* ../../../    */
                p += 3;
                q += 3;
            }
            else
            {
                while (p != buf && *--p != '/')
                    ; /* ->foo/FOO */
                *p = '\0';
                q += 3;
                strcat(buf, q);
            }
        }
        else if (strcmp(p, "/..") == 0)
        { /* foo/bar/..   */
            if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0)
            {
                /* ../..        */
            }
            else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0)
            {
                /* ../../..     */
            }
            else
            {
                while (p != buf && *--p != '/')
                    ; /* ->foo/ */
                *++p = '\0';
            }
            break;
        }
        else if (strncmp(p, "/./", 3) == 0)
        {              /* foo/./bar */
            *p = '\0'; /* -> foo/bar           */
            q += 2;
            strcat(buf, q);
        }
        else if (strcmp(p, "/.") == 0)
        {                /* foo/. */
            *++p = '\0'; /* -> foo/              */
            break;
        }
        else if (strncmp(p, "//", 2) == 0)
        { /* foo//bar */
            /* -> foo/bar           */
            *p = '\0';
            q++;
            strcat(buf, q);
        }
        else
        {
            p++;
            q++;
        }
    }
    return buf;
}

char *
expandPath(const char *name)
{
    struct passwd *passent, *getpwnam(const char *);
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    auto p = name;
    if (*p == '~')
    {
        p++;
#ifndef __MINGW32_VERSION
        if (IS_ALPHA(*p))
        {
            auto q = strchr(p, '/');
            if (q)
            { /* ~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            }
            else
            { /* ~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew(passent->pw_dir);
        }
        else
#endif /* __MINGW32_VERSION */
            if (*p == '/' || *p == '\0')
        { /* ~/dir... or ~ */
            extpath = Strnew(getenv("HOME"));
        }
        else
            goto rest;
        if (extpath->Cmp("/") == 0 && *p == '/')
            p++;
        extpath->Push(p);
        return extpath->ptr;
    }
rest:
    return const_cast<char *>(name);
}

#ifndef HAVE_STRCHR
char *
strchr(const char *s, int c)
{
    while (*s)
    {
        if ((unsigned char)*s == c)
            return (char *)s;
        s++;
    }
    return NULL;
}
#endif /* not HAVE_STRCHR */

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2)
{
    int x;
    while (*s1)
    {
        x = TOLOWER(*s1) - TOLOWER(*s2);
        if (x != 0)
            return x;
        s1++;
        s2++;
    }
    return -TOLOWER(*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
    int x;
    while (*s1 && n)
    {
        x = TOLOWER(*s1) - TOLOWER(*s2);
        if (x != 0)
            return x;
        s1++;
        s2++;
        n--;
    }
    return n ? -TOLOWER(*s2) : 0;
}
#endif /* not HAVE_STRCASECMP */

#ifndef HAVE_STRCASESTR
/* string search using the simplest algorithm */
char *
strcasestr(const char *s1, const char *s2)
{
    int len1, len2;
    if (s2 == NULL)
        return (char *)s1;
    if (*s2 == '\0')
        return (char *)s1;
    len1 = strlen(s1);
    len2 = strlen(s2);
    while (*s1 && len1 >= len2)
    {
        if (strncasecmp(s1, s2, len2) == 0)
            return (char *)s1;
        s1++;
        len1--;
    }
    return 0;
}
#endif

static int
strcasematch(char *s1, char *s2)
{
    int x;
    while (*s1)
    {
        if (*s2 == '\0')
            return 1;
        x = TOLOWER(*s1) - TOLOWER(*s2);
        if (x != 0)
            break;
        s1++;
        s2++;
    }
    return (*s2 == '\0');
}

/* search multiple strings */
int strcasemstr(char *str, char *srch[], char **ret_ptr)
{
    int i;
    while (*str)
    {
        for (i = 0; srch[i]; i++)
        {
            if (strcasematch(str, srch[i]))
            {
                if (ret_ptr)
                    *ret_ptr = str;
                return i;
            }
        }
        str++;
    }
    return -1;
}

char *remove_space(const char *str)
{
    const char *p;
    const char *q;
    for (p = str; *p && IS_SPACE(*p); p++)
        ;
    for (q = p; *q; q++)
        ;
    for (; q > p && IS_SPACE(*(q - 1)); q--)
        ;
    return Strnew_charp_n(p, q - p)->ptr;
}

int non_null(char *s)
{
    if (s == NULL)
        return false;
    while (*s)
    {
        if (!IS_SPACE(*s))
            return true;
        s++;
    }
    return false;
}

// newline to '\n'
void cleanup_line(Str s, int mode)
{
    if (s->Size() >= 2 &&
        s->ptr[s->Size() - 2] == '\r' && s->ptr[s->Size() - 1] == '\n')
    {
        s->Pop(2);
        s->Push('\n');
    }
    else if (s->Back() == '\r')
        s->ptr[s->Size() - 1] = '\n';
    else if (s->Back() != '\n')
        s->Push('\n');
    if (mode != PAGER_MODE)
    {
        for (int i = 0; i < s->Size(); i++)
        {
            if (s->ptr[i] == '\0')
                s->ptr[i] = ' ';
        }
    }
}

std::string html_quote(std::string_view str)
{
    std::stringstream ss;
    for (auto c : str)
    {
        auto q = html_quote_char(c);
        if (q)
        {
            ss << q;
        }
        else
        {
            ss << c;
        }
    }
    return ss.str();
}

char *html_quote(const char *str)
{
    auto tmp = new GCStr();
    for (auto p = str; *p; p++)
    {
        auto q = html_quote_char(*p);
        if (q)
        {
            tmp->Push(q);
        }
        else
        {
            tmp->Push(*p);
        }
    }
    return tmp->ptr;
}

char *
file_quote(char *str)
{
    Str tmp = NULL;
    char *p;
    char buf[4];

    for (p = str; *p; p++)
    {
        if (is_file_quote(*p))
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            sprintf(buf, "%%%02X", (unsigned char)*p);
            tmp->Push(buf);
        }
        else
        {
            if (tmp)
                tmp->Push(*p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return str;
}

char *
file_unquote(std::string_view str)
{
    Str tmp = Strnew();
    for (auto p = str.data(); *p;)
    {
        if (*p == '%')
        {
            auto q = p;
            auto c = url_unquote_char(&q);
            if (c >= 0)
            {
                if (c != '\0' && c != '\n' && c != '\r')
                    tmp->Push((char)c);
                p = q;
                continue;
            }
        }
        tmp->Push(*p);
        p++;
    }
    return tmp->ptr;
}

char *
shell_quote(std::string_view str)
{
    Str tmp = Strnew();
    for (auto p = str.data(); *p; p++)
    {
        if (is_shell_unsafe(*p))
        {
            tmp->Push('\\');
            tmp->Push(*p);
        }
        else
        {
            tmp->Push(*p);
        }
    }
    return tmp->ptr;
}

static const char *
w3m_dir(const char *name, const char *dft)
{
#ifdef USE_PATH_ENVVAR
    auto value = getenv(name);
    return value ? value : dft;
#else
    return dft;
#endif
}

const char *w3m_auxbin_dir()
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

const char *w3m_lib_dir()
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

const char *w3m_etc_dir()
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

const char *w3m_conf_dir()
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

const char *w3m_help_dir()
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}
/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */

///
/// &amp; => "&"
///
std::pair<const char *, std::string_view> getescapecmd(const char *s, CharacterEncodingScheme ces)
{
    auto save = s;
    auto [pos, ch] = ucs4_from_entity(s);
    s = pos.data();
    if (ch >= 0)
    {
        // ENTITY
        return {s, std::string_view(from_unicode(ch, ces))};
    }
    else
    {
        // NOT ENTITY
        return {s, std::string_view(save, s - save)};
    }
}

///
/// &#12345; => \xxx\xxx\xxx
///
char *
html_unquote(const char *str, CharacterEncodingScheme ces)
{
#ifndef NDEBUG
    std::string org = str;
#endif

    Str tmp = Strnew();
    for (auto p = str; *p;)
    {
        if (*p == '&')
        {
            auto [pos, unicode] = ucs4_from_entity(p);
            if (unicode == -1)
            {
                auto [pos, q] = getescapecmd(p, ces);
                tmp->Push(q);
                p = pos;
            }
            else
            {
                p = pos.data();
                auto utf8 = SingleCharacter::unicode_to_utf8(unicode);
                tmp->Push((const char *)utf8.bytes.data(), utf8.size());
            }
        }
        else
        {
            tmp->Push(*p);
            p++;
        }
    }

#ifndef NDEBUG
    assert(org == str);
#endif

    return tmp->ptr;
}

GCStr *UrlEncode(GCStr *src)
{
    GCStr *tmp = new GCStr();
    auto end = src->ptr + src->Size();
    for (auto p = src->ptr; p < end; p++)
    {
        if (*p == ' ')
        {
            // space
            tmp->Push('+');
        }
        else if (is_url_unsafe(*p))
        {
            //
            char buf[4];
            sprintf(buf, "%%%02X", (unsigned char)*p);
            tmp->Push(buf);
        }
        else
        {
            tmp->Push(*p);
        }
    }
    return tmp;
}

GCStr *UrlDecode(GCStr *src, bool is_form, bool safe)
{
    Str tmp = Strnew();
    char *p = src->ptr, *ep = src->ptr + src->Size(), *q;
    int c;

    for (; p < ep;)
    {
        if (is_form && *p == '+')
        {
            tmp->Push(' ');
            p++;
            continue;
        }
        else if (*p == '%')
        {
            q = p;
            c = url_unquote_char(&q);
            if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c)))
            {
                tmp->Push((char)c);
                p = q;
                continue;
            }
        }
        tmp->Push(*p);
        p++;
    }
    return tmp;
}

void ToUpper(Str str)
{
    auto p = str->ptr;
    auto end = p + str->Size();
    for (; p != end; ++p)
        *p = TOUPPER(*p);
}

void ToLower(Str str)
{
    auto p = str->ptr;
    auto end = p + str->Size();
    for (; p != end; ++p)
        *p = TOLOWER(*p);
}

void StripLeft(Str str)
{
    int i = 0;
    for (; i < str->Size(); i++)
    {
        if (!IS_SPACE(str->ptr[i]))
        {
            break;
        }
    }
    str->Delete(0, i);
}

void StripRight(Str str)
{
    int i = str->Size() - 1;
    for (; i >= 0; i--)
    {
        if (!IS_SPACE(str->ptr[i]))
        {
            break;
        }
    }
    str->Pop(str->Size() - (i + 1));
}

#define SP_NORMAL 0
#define SP_PREC 1
#define SP_PREC2 2

Str Sprintf(const char *fmt, ...)
{
    int len = 0;
    int status = SP_NORMAL;
    int p = 0;

    va_list ap;
    va_start(ap, fmt);
    for (auto f = fmt; *f; f++)
    {
    redo:
        switch (status)
        {
        case SP_NORMAL:
            if (*f == '%')
            {
                status = SP_PREC;
                p = 0;
            }
            else
                len++;
            break;
        case SP_PREC:
            if (IS_ALPHA(*f))
            {
                /* conversion char. */
                double vd;
                int vi;
                char *vs;
                void *vp;

                switch (*f)
                {
                case 'l':
                case 'h':
                case 'L':
                case 'w':
                    continue;
                case 'd':
                case 'i':
                case 'o':
                case 'x':
                case 'X':
                case 'u':
                    vi = va_arg(ap, int);
                    len += (p > 0) ? p : 10;
                    break;
                case 'f':
                case 'g':
                case 'e':
                case 'G':
                case 'E':
                    vd = va_arg(ap, double);
                    len += (p > 0) ? p : 15;
                    break;
                case 'c':
                    len += 1;
                    vi = va_arg(ap, int);
                    break;
                case 's':
                    vs = va_arg(ap, char *);
                    vi = strlen(vs);
                    len += (p > vi) ? p : vi;
                    break;
                case 'p':
                    vp = va_arg(ap, void *);
                    len += 10;
                    break;
                case 'n':
                    vp = va_arg(ap, void *);
                    break;
                }
                status = SP_NORMAL;
            }
            else if (IS_DIGIT(*f))
                p = p * 10 + *f - '0';
            else if (*f == '.')
                status = SP_PREC2;
            else if (*f == '%')
            {
                status = SP_NORMAL;
                len++;
            }
            break;
        case SP_PREC2:
            if (IS_ALPHA(*f))
            {
                status = SP_PREC;
                goto redo;
            }
            break;
        }
    }
    va_end(ap);

    auto s = Strnew_size(len * 2);
    va_start(ap, fmt);
    vsprintf(s->ptr, fmt, ap);
    va_end(ap);
    // s->m_size = ;
    s->Pop(s->Size() - strlen(s->ptr));
    if (s->Size() > len * 2)
    {
        fprintf(stderr, "Sprintf: string too long\n");
        exit(1);
    }
    return s;
}
