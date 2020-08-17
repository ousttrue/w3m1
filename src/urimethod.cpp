#include "urimethod.h"
#include "fm.h"
#include "indep.h"
#include "myctype.h"
#include "textlist.h"
#include "frontend/buffer.h"

struct UriMethod
{
    std::string scheme;
    std::string format;
};

static struct UriMethod default_urimethods[] = {
    {"mailto", "file:///$LIB/w3mmail.cgi?%s"},
};
static std::vector<UriMethod> loadURIMethods(std::string_view filename)
{
    Str tmp;
    char *up, *p;

    auto f = fopen(expandPath(filename.data()), "r");
    if (f == NULL)
        return {};
    int i = 0;
    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        if (tmp->ptr[0] != '#')
            i++;
    }
    fseek(f, 0, 0);
    int n = i;

    std::vector<UriMethod> um(n + 1);
    i = 0;
    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        if (tmp->ptr[0] == '#')
            continue;
        while (IS_SPACE(tmp->Back()))
            tmp->Pop(1);
        for (up = p = tmp->ptr; *p != '\0'; p++)
        {
            if (*p == ':')
            {
                um[i].scheme = std::string_view(up).substr(0, p - up);
                p++;
                break;
            }
        }
        if (*p == '\0')
            continue;
        while (*p != '\0' && IS_SPACE(*p))
            p++;
        um[i].format = Strnew(p)->ptr;
        i++;
    }
    fclose(f);
    return um;
}

static struct std::vector<UriMethod> urimethods;
void initURIMethods()
{
    TextList *methodmap_list = NULL;
    if (non_null(urimethodmap_files))
        methodmap_list = make_domain_list(urimethodmap_files);
    if (methodmap_list == NULL)
        return;

    for (auto tl = methodmap_list->first; tl; tl = tl->next)
    {
        for (auto &ump : loadURIMethods(tl->ptr))
        {
            urimethods.push_back(ump);
        }
    }
}

Str searchURIMethods(URL *pu)
{
    struct UriMethod *ump;
    int i;
    Str scheme = NULL;
    Str url;
    char *p;

    if (pu->scheme != SCM_UNKNOWN)
        return NULL; /* use internal */
    if (urimethods.empty())
        return NULL;
    url = pu->ToStr();
    for (p = url->ptr; *p != '\0'; p++)
    {
        if (*p == ':')
        {
            scheme = Strnew_charp_n(url->ptr, p - url->ptr);
            break;
        }
    }
    if (scheme == NULL)
        return NULL;

    /*
     * RFC2396 3.1. Scheme Component
     * For resiliency, programs interpreting URI should treat upper case
     * letters as equivalent to lower case in scheme names (e.g., allow
     * "HTTP" as well as "http").
     */
    for (auto &ump : urimethods)
    {
        if (ump.scheme == scheme->ptr)
        {
            return Sprintf(ump.format.c_str(), url_quote(url->ptr));
        }
    }
    for (auto &ump : default_urimethods)
    {
        if (ump.scheme == scheme->ptr)
        {
            return Sprintf(ump.format.c_str(), url_quote(url->ptr));
        }
    }
    return NULL;
}

/*
 * RFC2396: Uniform Resource Identifiers (URI): Generic Syntax
 * Appendix A. Collected BNF for URI
 * uric          = reserved | unreserved | escaped
 * reserved      = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" |
 *                 "$" | ","
 * unreserved    = alphanum | mark
 * mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" |
 *                  "(" | ")"
 * escaped       = "%" hex hex
 */

#define URI_PATTERN "([-;/?:@&=+$,a-zA-Z0-9_.!~*'()]|%[0-9A-Fa-f][0-9A-Fa-f])*"
void chkExternalURIBuffer(const BufferPtr &buf)
{
    for (auto &ump : urimethods)
    {
        reAnchor(buf, Sprintf("%s:%s", ump.scheme.c_str(), URI_PATTERN)->ptr);
    }
    for (auto &ump : default_urimethods)
    {
        reAnchor(buf, Sprintf("%s:%s", ump.scheme, URI_PATTERN)->ptr);
    }
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(const BufferPtr &buf)
{
    static const char *url_like_pat[] = {
        "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
        "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
        "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
        "news:[^<> 	][^<> 	]*",
        "nntp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
        "https?://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*",
        "ftp://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
        NULL};
    int i;
    for (i = 0; url_like_pat[i]; i++)
    {
        reAnchor(buf, const_cast<char *>(url_like_pat[i]));
    }
    chkExternalURIBuffer(buf);
    buf->check_url |= CHK_URL;
}
