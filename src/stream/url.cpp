#include <sstream>
#include "fm.h"
#include "gc_helper.h"
#include "file.h"
#include "stream/url.h"
#include "indep.h"
#include "stream/cookie.h"
#include "frontend/terms.h"
#include "html/form.h"
#include "frontend/display.h"
#include "html/anchor.h"
#include "stream/http_request.h"
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <string_view>
#include <sys/stat.h>
#include "myctype.h"
#include "regex.h"

#ifdef INET6
/* see rc.c, "dns_order" and dnsorders[] */
int ai_family_order_table[7][3] = {
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 0:unspec */
    {PF_INET, PF_INET6, PF_UNSPEC},    /* 1:inet inet6 */
    {PF_INET6, PF_INET, PF_UNSPEC},    /* 2:inet6 inet */
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 3: --- */
    {PF_INET, PF_UNSPEC, PF_UNSPEC},   /* 4:inet */
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 5: --- */
    {PF_INET6, PF_UNSPEC, PF_UNSPEC},  /* 6:inet6 */
};
#endif /* INET6 */

URLScheme g_schemeTable[] = {
    {"http", SCM_HTTP, 80},
    {"gopher", SCM_GOPHER, 70},
    {"ftp", SCM_FTP, 21},
    {"ftp", SCM_FTPDIR, 21},
    {"file", SCM_LOCAL, 0},
    {"local", SCM_LOCAL_CGI, 0},
    {"exec", SCM_EXEC},
    {"nntp", SCM_NNTP, 119},
    {"nntp", SCM_NNTP_GROUP},
    {"news", SCM_NEWS, 119},
    {"news", SCM_NEWS_GROUP},
    {"data", SCM_DATA, 0},
    {"mailto", SCM_MAILTO, 0},
    {"https", SCM_HTTPS, 443},
};
const URLScheme *GetScheme(URLSchemeTypes index)
{
    for (auto &s : g_schemeTable)
    {
        if (s.type == index)
        {
            return &s;
        }
    }
    return nullptr;
}

/* #define HTTP_DEFAULT_FILE    "/index.html" */

#ifndef HTTP_DEFAULT_FILE
#define HTTP_DEFAULT_FILE "/"
#endif /* not HTTP_DEFAULT_FILE */

static std::string_view DefaultFile(int scheme)
{
    switch (scheme)
    {
    case SCM_HTTP:
    case SCM_HTTPS:
        return HTTP_DEFAULT_FILE;
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
    case SCM_FTP:
    case SCM_FTPDIR:
        return "/";
    }
    return "";
}

int openSocket4(const char *hostname,
                const char *remoteport_name, unsigned short remoteport_num)
{
    int sock = -1;
    struct sockaddr_in hostaddr;
    struct hostent *entry;
    struct protoent *proto;
    unsigned short s_port;
    int a1, a2, a3, a4;
    unsigned long adr;
    MySignalHandler prevtrap = NULL;

    if (fmInitialized)
    {
        /* FIXME: gettextize? */
        message(Sprintf("Opening socket...")->ptr, 0, 0);
        refresh();
    }

    if (hostname == NULL)
    {
        return -1;
    }

    auto success = TrapJmp([&] {
        s_port = htons(remoteport_num);
        bzero((char *)&hostaddr, sizeof(struct sockaddr_in));
        if ((proto = getprotobyname("tcp")) == NULL)
        {
            /* protocol number of TCP is 6 */
            proto = New(struct protoent);
            proto->p_proto = 6;
        }
        if ((sock = socket(AF_INET, SOCK_STREAM, proto->p_proto)) < 0)
        {
            return false;
        }
        regexCompile("^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$", 0);
        if (regexMatch(const_cast<char *>(hostname), -1, 1))
        {
            sscanf(hostname, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
            adr = htonl((a1 << 24) | (a2 << 16) | (a3 << 8) | a4);
            bcopy((void *)&adr, (void *)&hostaddr.sin_addr, sizeof(long));
            hostaddr.sin_family = AF_INET;
            hostaddr.sin_port = s_port;
            if (fmInitialized)
            {
                message(Sprintf("Connecting to %s", hostname)->ptr, 0, 0);
                refresh();
            }
            if (connect(sock, (struct sockaddr *)&hostaddr,
                        sizeof(struct sockaddr_in)) < 0)
            {
                return false;
            }
        }
        else
        {
            char **h_addr_list;
            int result = -1;
            if (fmInitialized)
            {
                message(Sprintf("Performing hostname lookup on %s", hostname)->ptr,
                        0, 0);
                refresh();
            }
            if ((entry = gethostbyname(hostname)) == NULL)
            {
                return false;
            }
            hostaddr.sin_family = AF_INET;
            hostaddr.sin_port = s_port;
            for (h_addr_list = entry->h_addr_list; *h_addr_list; h_addr_list++)
            {
                bcopy((void *)h_addr_list[0], (void *)&hostaddr.sin_addr,
                      entry->h_length);
                if (fmInitialized)
                {
                    message(Sprintf("Connecting to %s", hostname)->ptr, 0, 0);
                    refresh();
                }
                if ((result = connect(sock, (struct sockaddr *)&hostaddr,
                                      sizeof(struct sockaddr_in))) == 0)
                {
                    break;
                }
            }
            if (result < 0)
            {
                return false;
            }
        }

        return true;
    });

    return success ? sock : -1;
}

int openSocket6(const char *hostname,
                const char *remoteport_name, unsigned short remoteport_num)
{
    int sock = -1;
    int *af;
    struct addrinfo hints, *res0, *res;
    int error;
    char *hname;
    MySignalHandler prevtrap = NULL;

    if (fmInitialized)
    {
        /* FIXME: gettextize? */
        message(Sprintf("Opening socket...")->ptr, 0, 0);
        refresh();
    }

    if (hostname == NULL)
    {
        return -1;
    }

    auto success = TrapJmp([&] {
        /* rfc2732 compliance */
        hname = const_cast<char *>(hostname);
        if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']')
        {
            hname = allocStr(hostname + 1, -1);
            hname[strlen(hname) - 1] = '\0';
            if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
                return false;
        }
        for (af = ai_family_order_table[DNS_order];; af++)
        {
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = *af;
            hints.ai_socktype = SOCK_STREAM;
            if (remoteport_num != 0)
            {
                Str portbuf = Sprintf("%d", remoteport_num);
                error = getaddrinfo(hname, portbuf->ptr, &hints, &res0);
            }
            else
            {
                error = -1;
            }
            if (error && remoteport_name && remoteport_name[0] != '\0')
            {
                /* try default port */
                error = getaddrinfo(hname, remoteport_name, &hints, &res0);
            }
            if (error)
            {
                if (*af == PF_UNSPEC)
                {
                    return false;
                }
                /* try next ai family */
                continue;
            }

            sock = -1;
            for (res = res0; res; res = res->ai_next)
            {
                sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                if (sock < 0)
                {
                    continue;
                }
                if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
                {
                    close(sock);
                    sock = -1;
                    continue;
                }
                break;
            }
            if (sock < 0)
            {
                freeaddrinfo(res0);
                if (*af == PF_UNSPEC)
                {
                    return false;
                }
                /* try next ai family */
                continue;
            }
            freeaddrinfo(res0);
            break;
        }

        return true;
    });

    return success ? sock : -1;
}

int openSocket(const char *hostname,
               const char *remoteport_name, unsigned short remoteport_num)
{
    return openSocket6(hostname, remoteport_name, remoteport_num);
}

std::tuple<std::string_view, URLSchemeTypes> URLScheme::Parse(std::string_view url)
{
    for (auto &scheme : g_schemeTable)
    {
        if (url.size() > scheme.name.size() && url.starts_with(scheme.name) && url[scheme.name.size()] == ':')
        {
            // found
            return {url.substr(scheme.name.size() + 1), scheme.type};
        }
    }

    // skip for ':'
    auto p = url;
    for (; p.size(); p = p.substr(1))
    {
        if ((IS_ALNUM(p[0]) || p[0] == '.' || p[0] == '+' || p[0] == '-'))
        {
            continue;
        }
        else
        {
            break;
        }
    }

    if (p[0] != ':')
    {
        return {url, SCM_MISSING};
    }
    p = p.substr(1);

    return {p, SCM_MISSING};
}

std::tuple<const char *, URLSchemeTypes> getURLScheme(const char *url)
{
    auto [remain, type] = URLScheme::Parse(url);
    return {remain.data(), type};
}

static const char *string_strchr(const std::string &src, int c)
{
    auto pos = src.find(c);
    if (pos == std::string::npos)
    {
        return nullptr;
    }
    return src.c_str() + pos;
}

#define URL_QUOTE_MASK 0x10 /* [\0- \177-\377] */
static bool is_url_quote(char c)
{
    return (GET_QUOTE_TYPE(c) & URL_QUOTE_MASK);
}

static const char xdigit[0x11] = "0123456789ABCDEF";
std::string url_quote(std::string_view p)
{
    std::string str;
    for (; p.size(); p = p.substr(1))
    {
        if (is_url_quote(p[0]))
        {
            str.push_back('%');
            str.push_back(xdigit[((unsigned char)p[0] >> 4) & 0xF]);
            str.push_back(xdigit[(unsigned char)p[0] & 0xF]);
        }
        else
        {
            str.push_back(p[0]);
        }
    }
    return str;
}

static char *expandName(char *name)
{
    char *p;
    struct passwd *passent, *getpwnam(const char *);
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    p = name;
    if (*p == '/')
    {
        if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2))) && personal_document_root)
        {
            char *q;
            p += 2;
            q = strchr(p, '/');
            if (q)
            { /* /~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            }
            else
            { /* /~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_m_charp(passent->pw_dir, "/",
                                     personal_document_root, NULL);
            if (*personal_document_root == '\0' && *p == '/')
                p++;
        }
        else
            goto rest;
        if (extpath->Cmp("/") == 0 && *p == '/')
            p++;
        extpath->Push(p);
        return extpath->ptr;
    }
    else
        return expandPath(p);
rest:
    return name;
}

URL::URL(URLSchemeTypes scheme, const Userinfo &userinfo, std::string_view host, int port,
         std::string_view path, std::string_view query, std::string_view framgment)
    : scheme(scheme), userinfo(userinfo), host(host), port(port), path(path), query(query), fragment(framgment)
{
    if (scheme == SCM_LOCAL)
    {
        real_file = cleanupName(file_unquote(path.data()));
    }
}

// https://developer.mozilla.org/en-US/docs/Glossary/origin
bool URL::HasSameOrigin(const URL &rhs) const
{
    if (scheme != rhs.scheme)
    {
        return false;
    }
    if (host != rhs.host)
    {
        return false;
    }
    if (port != rhs.port)
    {
        return false;
    }
    return true;
}

std::string URL::ToRefererOrigin() const
{
    std::stringstream ss;
    auto schemeInfo = GetScheme(this->scheme);
    ss
        << schemeInfo->name
        << ':'
        << host;
    if (this->port != schemeInfo->port)
    {
        ss << ':' << this->port;
    }
    ss << '/';
    return ss.str();
}

//
// scheme://userinfo@host:port/path?query#fragment
//
static std::tuple<std::string_view, URLSchemeTypes> ParseScheme(std::string_view p, const URL *current)
{
    // const char *q = nullptr;
    auto [remain, scheme] = URLScheme::Parse(p);
    if (scheme == SCM_MISSING)
    {
        /* scheme part is not found in the url. This means either
        * (a) the url is relative to the current or (b) the url
        * denotes a filename (therefore the scheme is SCM_LOCAL).
        */
        if (current)
        {
            switch (current->scheme)
            {
            case SCM_LOCAL:
            case SCM_LOCAL_CGI:
                scheme = SCM_LOCAL;
                break;
            case SCM_FTP:
            case SCM_FTPDIR:
                scheme = SCM_FTP;
                break;
            case SCM_NNTP:
            case SCM_NNTP_GROUP:
                scheme = SCM_NNTP;
                break;
            case SCM_NEWS:
            case SCM_NEWS_GROUP:
                scheme = SCM_NEWS;
                break;
            default:
                scheme = current->scheme;
                break;
            }
        }
        else
        {
            scheme = SCM_LOCAL;
        }
    }
    return {remain, scheme};
}

static std::tuple<std::string_view, URL::Userinfo, std::string_view, int> ParseUserinfoHostPort(std::string_view p, URLSchemeTypes scheme)
{
    if (!p.starts_with("//"))
    {
        /* the url doesn't begin with '//' */
        return {p, {}, {}, {}};
    }
    /* URL begins with // */
    /* it means that 'scheme:' is abbreviated */
    p = p.substr(2);

    std::string_view host;
    URL::Userinfo userinfo;
    int port = 0;

analyze_url:
    auto q = p;

    if (q[0] == '[')
    { /* rfc2732,rfc2373 compliance */
        p.remove_prefix(1);
        while (IS_XDIGIT(p[0]) || p[0] == ':' || p[0] == '.')
            p.remove_prefix(1);
        if (p[0] != ']' || (p.size() >= 2 && strchr(":/?#", p[1]) == NULL))
            p = q;
    }

    while (p.size() && strchr(":/@?#", p[0]) == NULL)
        p.remove_prefix(1);
    switch (p[0])
    {
    case ':':
    {
        /* scheme://user:pass@host or
        * scheme://host:port
        */
        host = q.substr(0, p.data() - q.data());
        q = p.substr(1);
        while (p.size() && strchr("@/?#", p[0]) == NULL)
            p.remove_prefix(1);
        if (p[0] == '@')
        {
            /* scheme://user:pass@...       */
            userinfo.pass = q.substr(0, p.data() - q.data());
            q = p.substr(1);
            userinfo.name = host;
            host = {};
            goto analyze_url;
        }
        /* scheme://host:port/ */
        auto tmp = q.substr(0, p.data() - q.data());
        port = atoi(tmp.data());
        /* *p is one of ['\0', '/', '?', '#'] */
        break;
    }
    case '@':
        /* scheme://user@...            */
        userinfo.name = q.substr(0, p.data() - q.data());
        q = p.substr(1);
        goto analyze_url;
    case '\0':
    /* scheme://host                */
    case '/':
    case '?':
    case '#':
        host = q.substr(0, p.data() - q.data());
        port = g_schemeTable[scheme].port;
        break;
    }

    return {p, userinfo, host, port};
}

static std::tuple<std::string_view, std::string_view> ParsePath(std::string_view p, URLSchemeTypes scheme, std::string_view host)
{
    if ((p[0] == '\0' || p[0] == '#' || p[0] == '?') && host.empty())
    {
        return {p, {}};
    }

    auto q = p;
    if (p[0] == '/')
        p.remove_prefix(1);
    if (p[0] == '\0' || p[0] == '#' || p[0] == '?')
    { /* scheme://host[:port]/ */
        return {p, DefaultFile(scheme)};
    }

    auto cgi = strchr(p.data(), '?');
    if (!cgi)
    {
        cgi = "";
    }

again:
    while (p[0] && p[0] != '#' && p != cgi)
        p.remove_prefix(1);
    if (p[0] == '#' && scheme == SCM_LOCAL)
    {
        /* 
        * According to RFC2396, # means the beginning of
        * URI-reference, and # should be escaped.  But,
        * if the scheme is SCM_LOCAL, the special
        * treatment will apply to # for convinience.
        */
        if (p > q && *(p.data() - 1) == '/' && (cgi == NULL || p < cgi))
        {
            /* 
            * # comes as the first character of the file name
            * that means, # is not a label but a part of the file
            * name.
            */
            p.remove_prefix(1);
            goto again;
        }
        else if (p[1] == '\0')
        {
            /* 
            * # comes as the last character of the file name that
            * means, # is not a label but a part of the file
            * name.
            */
            p.remove_prefix(1);
        }
    }

    if (scheme == SCM_LOCAL || scheme == SCM_MISSING)
    {
        return {p, q.substr(0, p.data() - q.data())};
    }

    return {p, q.substr(0, p.data() - q.data())};
}

static std::tuple<std::string_view, std::string_view> ParseQuery(std::string_view p)
{
    if (p[0] != '?')
    {
        return {p, {}};
    }

    auto q = p.substr(1);
    while (p[0] && p[0] != '#')
        p.remove_prefix(1);
    return {p, q.substr(0, p.data() - q.data())};
}

URL URL::Parse(std::string_view _url, const URL *current)
{
    // *this = {};

    /* quote 0x01-0x20, 0x7F-0xFF */
    auto quoted = url_quote(_url);
    auto url = quoted.c_str();

    /* RFC1808: Relative Uniform Resource Locators
     * 4.  Resolving Relative URLs
     */
    // if (url[0] == '\0' || url[0] == '#')
    // {
    //     if (current)
    //         *this = *current;
    //     ParseFragment(url);
    //     return;
    // }

    auto [p1, scheme] = ParseScheme(url, current);
    auto [p2, userinfo, host, port] = ParseUserinfoHostPort(p1, scheme);

    // /* scheme part has been found */
    // if (this->scheme == SCM_UNKNOWN)
    // {
    //     this->path = allocStr(url, -1);
    //     return;
    // }

    // /* get host and port */
    // if (p[0] != '/' || p[1] != '/')
    // { /* scheme:foo or scheme:/foo */
    //     this->host.clear();
    //     if (this->scheme != SCM_UNKNOWN)
    //         this->port = DefaultPort[this->scheme];
    //     else
    //         this->port = 0;
    //     goto analyze_file;
    // }

    // /* after here, p begins with // */
    // if (this->scheme == SCM_LOCAL)
    // { /* file://foo           */
    //     if (p[2] == '/' || p[2] == '~'
    //         /* <A HREF="file:///foo">file:///foo</A>  or <A HREF="file://~user">file://~user</A> */
    //     )
    //     {
    //         p += 2;
    //         goto analyze_file;
    //     }
    // }
    // p += 2; /* scheme://foo         */
    //         /*          ^p is here  */

    auto [p3, path] = ::ParsePath(p2, scheme, host);
    auto [remain, query] = ParseQuery(p3);

    // ParseFragment(p);
    std::string_view fragment;
    if (scheme == SCM_MISSING)
    {
        scheme = SCM_LOCAL;
        path = remain;
    }
    else if (remain.size() && remain[0] == '#')
    {
        fragment = remain.substr(1);
    }

    //
    // post process
    //
    auto p = remain.data();

    if (scheme == SCM_LOCAL)
    {
        char *q = expandName(file_unquote(path));
        path = file_quote(q);
    }

    bool relative_uri = false;
    if (current && scheme == current->scheme && host.empty())
    {
        /* Copy omitted element from the current URL */
        userinfo = current->userinfo;
        host = current->host;
        port = current->port;
        if (path.size() && path[0])
        {
            if (scheme == SCM_UNKNOWN && strchr(const_cast<char *>(path.data()), ':') == NULL && current && (p = string_strchr(current->path, ':')) != NULL)
            {
                path = Strnew_m_charp(current->path.substr(0, p - current->path.data()), ":", path)->ptr;
            }
            else if (path[0] != '/')
            {
                /* file is relative [process 1] */
                p = path.data();
                if (current->path.size())
                {
                    auto tmp = Strnew(current->path);
                    while (tmp->Size() > 0)
                    {
                        if (tmp->Back() == '/')
                            break;
                        tmp->Pop(1);
                    }
                    tmp->Push(p);
                    path = tmp->ptr;
                    relative_uri = TRUE;
                }
            }
        }
        else
        { /* scheme:[?query][#label] */
            path = current->path;
            if (query.empty())
                query = current->query;
        }
        /* comment: query part need not to be completed
	    * from the current URL. */
    }

    if (path.size())
    {
        if (scheme == SCM_LOCAL && path[0] != '/' && path != "-")
        {
            /* local file, relative path */
            auto tmp = Strnew(w3mApp::Instance().CurrentDir);
            if (tmp->Back() != '/')
                tmp->Push('/');
            tmp->Push(file_unquote(path));
            path = file_quote(cleanupName(tmp->ptr));
        }
        else if (scheme == SCM_HTTP || scheme == SCM_HTTPS)
        {
            if (relative_uri)
            {
                /* In this case, file is created by [process 1] above.
                * file may contain relative path (for example, 
                * "/foo/../bar/./baz.html"), cleanupName() must be applied.
                * When the entire abs_path is given, it still may contain
                * elements like `//', `..' or `.' in the file. It is 
                * server's responsibility to canonicalize such path.
                */
                path = cleanupName(path.data());
            }
        }
        else if (path[0] == '/')
        {
            /*
            * this happens on the following conditions:
            * (1) ftp scheme (2) local, looks like absolute path.
            * In both case, there must be no side effect with
            * cleanupName(). (I hope so...)
            */
            path = cleanupName(path.data());
        }

        return URL(scheme, userinfo, std::string(host), port, std::string(path), std::string(query), std::string(fragment));
    }
}

URL URL::ParsePath(std::string_view file)
{
    return URL(SCM_LOCAL, {}, {}, {}, expandPath(file.data()), {}, {});
}

Str URL::ToStr(bool usePass, bool useLabel) const
{
    if (this->scheme == SCM_MISSING)
    {
        return Strnew("???");
    }
    else if (this->scheme == SCM_UNKNOWN)
    {
        return Strnew(this->path);
    }
    if (this->host.empty() && this->path.empty() && this->fragment.size())
    {
        /* local label */
        return Sprintf("#%s", this->fragment);
    }
    if (this->scheme == SCM_LOCAL && this->path == "-")
    {
        auto tmp = Strnew("-");
        if (this->fragment.size())
        {
            tmp->Push('#');
            tmp->Push(this->fragment);
        }
        return tmp;
    }

    {
        auto scheme = GetScheme(this->scheme);
        auto tmp = Strnew(scheme->name);
        tmp->Push(':');
        if (this->userinfo.name.size())
        {
            tmp->Push(this->userinfo.name);
            if (usePass && this->userinfo.pass.size())
            {
                tmp->Push(':');
                tmp->Push(this->userinfo.pass);
            }
            tmp->Push('@');
        }
        if (this->host.size())
        {
            tmp->Push(this->host);
            if (this->port != g_schemeTable[this->scheme].port)
            {
                tmp->Push(':');
                tmp->Push(Sprintf("%d", this->port));
            }
        }
        tmp->Push(this->path);
        if (this->scheme == SCM_FTPDIR && tmp->Back() != '/')
            tmp->Push('/');
        if (this->query.size())
        {
            tmp->Push('?');
            tmp->Push(this->query);
        }
        if (useLabel && this->fragment.size())
        {
            tmp->Push('#');
            tmp->Push(this->fragment);
        }
        return tmp;
    }
}

TextList *
make_domain_list(char *domain_list)
{
    char *p;
    Str tmp;
    TextList *domains = NULL;

    p = domain_list;
    tmp = Strnew_size(64);
    while (*p)
    {
        while (*p && IS_SPACE(*p))
            p++;
        tmp->Clear();
        while (*p && !IS_SPACE(*p) && *p != ',')
            tmp->Push(*p++);
        if (tmp->Size() > 0)
        {
            if (domains == NULL)
                domains = newTextList();
            pushText(domains, tmp->ptr);
        }
        while (*p && IS_SPACE(*p))
            p++;
        if (*p == ',')
            p++;
    }
    return domains;
}

const char *
filename_extension(const char *path, int is_url)
{
    const char *last_dot = "";
    if (path == NULL)
        return last_dot;

    auto p = path;
    if (*p == '.')
        p++;
    for (; *p; p++)
    {
        if (*p == '.')
        {
            last_dot = p;
        }
        else if (is_url && *p == '?')
            break;
    }
    if (*last_dot == '.')
    {
        int i = 1;
        for (; last_dot[i] && i < 8; i++)
        {
            if (is_url && !IS_ALNUM(last_dot[i]))
                break;
        }
        return allocStr(last_dot, i);
    }
    else
        return last_dot;
}

URL *schemeToProxy(int scheme)
{
    URL *pu = NULL; /* for gcc */
    switch (scheme)
    {
    case SCM_HTTP:
        pu = &w3mApp::Instance().HTTP_proxy_parsed;
        break;
#ifdef USE_SSL
    case SCM_HTTPS:
        pu = &w3mApp::Instance().HTTPS_proxy_parsed;
        break;
#endif
    case SCM_FTP:
        pu = &w3mApp::Instance().FTP_proxy_parsed;
        break;
#ifdef USE_GOPHER
    case SCM_GOPHER:
        pu = &GOPHER_proxy_parsed;
        break;
#endif
#ifdef DEBUG
    default:
        abort();
#endif
    }
    return pu;
}

char *mybasename(std::string_view s)
{
    const char *p = s.data();
    while (*p)
        p++;
    while (s <= p && *p != '/')
        p--;
    if (*p == '/')
        p++;
    else
        p = s.data();
    return allocStr(p, -1);
}

char *
url_unquote_conv(std::string_view url, CharacterEncodingScheme charset)
{
    auto old_auto_detect = WcOption.auto_detect;
    Str tmp = UrlDecode(Strnew(url), FALSE, TRUE);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = w3mApp::Instance().SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(SCM_UNKNOWN, tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}
