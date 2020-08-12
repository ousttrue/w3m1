#include "fm.h"
#include "file.h"
#include "transport/url.h"
#include "indep.h"
#include "http/cookie.h"
#include "frontend/terms.h"
#include "html/form.h"
#include "frontend/display.h"
#include "html/anchor.h"
#include "http/http_request.h"
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

#ifdef __WATT32__
#define write(a, b, c) write_s(a, b, c)
#endif /* __WATT32__ */

#ifdef __MINGW32_VERSION
#define write(a, b, c) send(a, b, c, 0)
#define close(fd) closesocket(fd)
#endif

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

static JMP_BUF AbortLoading;

/* XXX: note html.h SCM_ */
static int
    DefaultPort[] = {
        80,  /* http */
        70,  /* gopher */
        21,  /* ftp */
        21,  /* ftpdir */
        0,   /* local - not defined */
        0,   /* local-CGI - not defined? */
        0,   /* exec - not defined? */
        119, /* nntp */
        119, /* nntp group */
        119, /* news */
        119, /* news group */
        0,   /* data - not defined */
        0,   /* mailto - not defined */
        443, /* https */
};

URLScheme schemetable[] = {
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
    for (auto &s : schemetable)
    {
        if (s.schema == index)
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

#ifdef SOCK_DEBUG
#include <stdarg.h>
static void
sock_log(char *message, ...)
{
    FILE *f = fopen("zzzsocklog", "a");
    va_list va;

    if (f == NULL)
        return;
    va_start(va, message);
    vfprintf(f, message, va);
    fclose(f);
}
#endif

static std::string_view DefaultFile(int scheme)
{
    switch (scheme)
    {
    case SCM_HTTP:
    case SCM_HTTPS:
        return allocStr(HTTP_DEFAULT_FILE, -1);
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
    case SCM_FTP:
    case SCM_FTPDIR:
        return allocStr("/", -1);
    }
    return "";
}

int openSocket(const char *hostname,
               const char *remoteport_name, unsigned short remoteport_num)
{
    int sock = -1;
#ifdef INET6
    int *af;
    struct addrinfo hints, *res0, *res;
    int error;
    char *hname;
#else  /* not INET6 */
    struct sockaddr_in hostaddr;
    struct hostent *entry;
    struct protoent *proto;
    unsigned short s_port;
    int a1, a2, a3, a4;
    unsigned long adr;
#endif /* not INET6 */
    MySignalHandler prevtrap = NULL;

    if (fmInitialized)
    {
        /* FIXME: gettextize? */
        message(Sprintf("Opening socket...")->ptr, 0, 0);
        refresh();
    }

    if (hostname == NULL)
    {
#ifdef SOCK_DEBUG
        sock_log("openSocket() failed. reason: Bad hostname \"%s\"\n",
                 hostname);
#endif
        return -1;
    }

    auto success = TrapJmp([&] {

#ifdef INET6
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
#else /* not INET6 */
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
#ifdef SOCK_DEBUG
            sock_log("openSocket: socket() failed. reason: %s\n", strerror(errno));
#endif
            goto error;
        }
        regexCompile("^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$", 0);
        if (regexMatch(hostname, -1, 1))
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
#ifdef SOCK_DEBUG
                sock_log("openSocket: connect() failed. reason: %s\n",
                         strerror(errno));
#endif
                goto error;
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
#ifdef SOCK_DEBUG
                sock_log("openSocket: gethostbyname() failed. reason: %s\n",
                         strerror(errno));
#endif
                goto error;
            }
            hostaddr.sin_family = AF_INET;
            hostaddr.sin_port = s_port;
            for (h_addr_list = entry->h_addr_list; *h_addr_list; h_addr_list++)
            {
                bcopy((void *)h_addr_list[0], (void *)&hostaddr.sin_addr,
                      entry->h_length);
#ifdef SOCK_DEBUG
                adr = ntohl(*(long *)&hostaddr.sin_addr);
                sock_log("openSocket: connecting %d.%d.%d.%d\n",
                         (adr >> 24) & 0xff,
                         (adr >> 16) & 0xff, (adr >> 8) & 0xff, adr & 0xff);
#endif
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
#ifdef SOCK_DEBUG
                else
                {
                    sock_log("openSocket: connect() failed. reason: %s\n",
                             strerror(errno));
                }
#endif
            }
            if (result < 0)
            {
                goto error;
            }
        }
#endif /* not INET6 */

        return true;
    });

    return success ? sock : -1;
}

enum SpaceMode
{
    COPYPATH_SPC_ALLOW = 0,
    COPYPATH_SPC_IGNORE = 1,
    // COPYPATH_SPC_REPLACE = 2,
};
static char *copyPath(const char *orgpath, int length, SpaceMode option)
{
    Str tmp = Strnew();
    while (*orgpath && length != 0)
    {
        if (IS_SPACE(*orgpath))
        {
            // already quoted
            assert(false);

            switch (option)
            {
            case COPYPATH_SPC_ALLOW:
                tmp->Push(*orgpath);
                break;
            case COPYPATH_SPC_IGNORE:
                /* do nothing */
                break;
                // case COPYPATH_SPC_REPLACE:
                //     tmp->Push("%20");
                //     break;
            }
        }
        else
            tmp->Push(*orgpath);
        orgpath++;
        length--;
    }
    return tmp->ptr;
}

std::tuple<const char *, URLSchemeTypes> getURLScheme(const char *url)
{
    // skip for ':'
    auto p = url;
    for (; *p; ++p)
    {
        if ((IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
        {
            continue;
        }
        else
        {
            break;
        }
    }
    if (*p != ':')
    {
        return {url, SCM_MISSING};
    }
    ++p;

    std::string_view view(url);
    for (auto &scheme : schemetable)
    {
        if (view.starts_with(scheme.name) && view[scheme.name.size()] == ':')
        {
            return {p, scheme.schema};
        }
    }

    return {p, SCM_MISSING};
}

const char *string_strchr(const std::string &src, int c)
{
    auto pos = src.find(c);
    if (pos == std::string::npos)
    {
        return nullptr;
    }
    return src.c_str() + pos;
}

#define URL_QUOTE_MASK 0x10 /* [\0- \177-\377] */
#define is_url_quote(c) (GET_QUOTE_TYPE(c) & URL_QUOTE_MASK)
static const char xdigit[0x11] = "0123456789ABCDEF";
char *url_quote(std::string_view str)
{
    Str tmp = Strnew();
    for (auto p = str.data(); *p; p++)
    {
        if (is_url_quote(*p))
        {
            tmp->Push('%');
            tmp->Push(xdigit[((unsigned char)*p >> 4) & 0xF]);
            tmp->Push(xdigit[(unsigned char)*p & 0xF]);
        }
        else
        {
            tmp->Push(*p);
        }
    }
    return tmp->ptr;
}

//
// scheme://userinfo@host:port/path?query#fragment
//
void URL::Parse(std::string_view _url, const URL *current)
{
    *this = {};

    /* quote 0x01-0x20, 0x7F-0xFF */
    auto url = url_quote(_url);

    /* RFC1808: Relative Uniform Resource Locators
     * 4.  Resolving Relative URLs
     */
    if (*url == '\0' || *url == '#')
    {
        if (current)
            *this = *current;
        ParseFragment(url);
        return;
    }

    auto p = ParseScheme(url, current);
    p = ParseUserinfoHostPort(p);

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

    p = ParsePath(p);

    p = ParseQuery(p);
    ParseFragment(p);
}

const char *URL::ParseScheme(const char *p, const URL *current)
{
    // const char *q = nullptr;
    auto [pos, scheme] = getURLScheme(p);
    this->scheme = scheme;
    if (this->scheme == SCM_MISSING)
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
                this->scheme = SCM_LOCAL;
                break;
            case SCM_FTP:
            case SCM_FTPDIR:
                this->scheme = SCM_FTP;
                break;
            case SCM_NNTP:
            case SCM_NNTP_GROUP:
                this->scheme = SCM_NNTP;
                break;
            case SCM_NEWS:
            case SCM_NEWS_GROUP:
                this->scheme = SCM_NEWS;
                break;
            default:
                this->scheme = current->scheme;
                break;
            }
        }
        else
        {
            this->scheme = SCM_LOCAL;
        }
    }
    return pos;
}

const char *URL::ParseUserinfoHostPort(const char *p)
{
    if (strncmp(p, "//", 2) != 0)
    {
        /* the url doesn't begin with '//' */
        return p;
    }
    /* URL begins with // */
    /* it means that 'scheme:' is abbreviated */
    p += 2;

analyze_url:
    auto q = p;
#ifdef INET6
    if (*q == '[')
    { /* rfc2732,rfc2373 compliance */
        p++;
        while (IS_XDIGIT(*p) || *p == ':' || *p == '.')
            p++;
        if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == NULL))
            p = q;
    }
#endif
    while (*p && strchr(":/@?#", *p) == NULL)
        p++;
    switch (*p)
    {
    case ':':
    {
        /* scheme://user:pass@host or
            * scheme://host:port
            */
        this->host = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
        q = ++p;
        while (*p && strchr("@/?#", *p) == NULL)
            p++;
        if (*p == '@')
        {
            /* scheme://user:pass@...       */
            this->userinfo.pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
            q = ++p;
            this->userinfo.name = this->host;
            this->host.clear();
            goto analyze_url;
        }
        /* scheme://host:port/ */
        auto tmp = Strnew_charp_n(q, p - q);
        this->port = atoi(tmp->ptr);
        /* *p is one of ['\0', '/', '?', '#'] */
        break;
    }
    case '@':
        /* scheme://user@...            */
        this->userinfo.name = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
        q = ++p;
        goto analyze_url;
    case '\0':
    /* scheme://host                */
    case '/':
    case '?':
    case '#':
        this->host = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
        this->port = DefaultPort[this->scheme];
        break;
    }

    return p;
}

const char *URL::ParsePath(const char *p)
{
    if ((*p == '\0' || *p == '#' || *p == '?') && this->host.empty())
    {
        this->path = "";
        return p;
    }

    auto q = p;
    if (*p == '/')
        p++;
    if (*p == '\0' || *p == '#' || *p == '?')
    { /* scheme://host[:port]/ */
        this->path = DefaultFile(this->scheme);
        return p;
    }

    auto cgi = strchr(p, '?');

again:
    while (*p && *p != '#' && p != cgi)
        p++;
    if (*p == '#' && this->scheme == SCM_LOCAL)
    {
        /* 
        * According to RFC2396, # means the beginning of
        * URI-reference, and # should be escaped.  But,
        * if the scheme is SCM_LOCAL, the special
        * treatment will apply to # for convinience.
        */
        if (p > q && *(p - 1) == '/' && (cgi == NULL || p < cgi))
        {
            /* 
            * # comes as the first character of the file name
            * that means, # is not a label but a part of the file
            * name.
            */
            p++;
            goto again;
        }
        else if (*(p + 1) == '\0')
        {
            /* 
            * # comes as the last character of the file name that
            * means, # is not a label but a part of the file
            * name.
            */
            p++;
        }
    }
    if (this->scheme == SCM_LOCAL || this->scheme == SCM_MISSING)
    {
        this->path = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    }
    else
    {
        this->path = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    }
    return p;
}

const char *URL::ParseQuery(const char *p)
{
    if (*p == '?')
    {
        auto q = ++p;
        while (*p && *p != '#')
            p++;
        this->query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    }
    return p;
}

void URL::ParseFragment(const char *p)
{
    if (this->scheme == SCM_MISSING)
    {
        this->scheme = SCM_LOCAL;
        this->path = p;
        this->fragment.clear();
    }
    else if (*p == '#')
    {
        this->fragment = p + 1;
    }
}

char *
expandName(char *name)
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

void URL::Parse2(std::string_view url, const URL *current)
{
    const char *p;
    Str tmp;
    int relative_uri = FALSE;

    this->Parse(url, current);
    if (this->scheme == SCM_MAILTO)
        return;

    if (this->scheme == SCM_DATA)
        return;
    if (this->scheme == SCM_NEWS || this->scheme == SCM_NEWS_GROUP)
    {
        if (this->path.size() && !string_strchr(this->path, '@') &&
            (!(p = string_strchr(this->path, '/')) || strchr(p + 1, '-') ||
             *(p + 1) == '\0'))
            this->scheme = SCM_NEWS_GROUP;
        else
            this->scheme = SCM_NEWS;
        return;
    }
    if (this->scheme == SCM_NNTP || this->scheme == SCM_NNTP_GROUP)
    {
        if (this->path.size() && this->path[0] == '/')
            this->path = this->path.substr(1);
        if (this->path.size() && !string_strchr(this->path, '@') &&
            (!(p = string_strchr(this->path.c_str(), '/')) || strchr(p + 1, '-') ||
             *(p + 1) == '\0'))
            this->scheme = SCM_NNTP_GROUP;
        else
            this->scheme = SCM_NNTP;
        if (current && (current->scheme == SCM_NNTP ||
                        current->scheme == SCM_NNTP_GROUP))
        {
            if (this->host.empty())
            {
                this->host = current->host;
                this->port = current->port;
            }
        }
        return;
    }
    if (this->scheme == SCM_LOCAL)
    {
        char *q = expandName(file_unquote(this->path));
#ifdef SUPPORT_DOS_DRIVE_PREFIX
        Str drive;
        if (IS_ALPHA(q[0]) && q[1] == ':')
        {
            drive = Strnew_charp_n(q, 2);
            drive->Push(file_quote(q + 2));
            this->file = drive->ptr;
        }
        else
#endif
            this->path = file_quote(q);
    }

    if (current && (this->scheme == current->scheme || (this->scheme == SCM_FTP && current->scheme == SCM_FTPDIR) || (this->scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI)) && this->host.empty())
    {
        /* Copy omitted element from the current URL */
        this->userinfo = current->userinfo;
        this->host = current->host;
        this->port = current->port;
        if (this->path.size() && this->path[0])
        {
#ifdef USE_EXTERNAL_URI_LOADER
            if (this->scheme == SCM_UNKNOWN && strchr(const_cast<char *>(this->path.c_str()), ':') == NULL && current && (p = string_strchr(current->path, ':')) != NULL)
            {
                this->path = Strnew_m_charp(current->path.substr(0, p - current->path.c_str()), ":", this->path)->ptr;
            }
            else
#endif
                if (
#ifdef USE_GOPHER
                    this->scheme != SCM_GOPHER &&
#endif /* USE_GOPHER */
                    this->path[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
                    && !(this->scheme == SCM_LOCAL && IS_ALPHA(this->file[0]) && this->file[1] == ':')
#endif
                )
            {
                /* file is relative [process 1] */
                p = this->path.c_str();
                if (current->path.size())
                {
                    tmp = Strnew(current->path);
                    while (tmp->Size() > 0)
                    {
                        if (tmp->Back() == '/')
                            break;
                        tmp->Pop(1);
                    }
                    tmp->Push(p);
                    this->path = tmp->ptr;
                    relative_uri = TRUE;
                }
            }
#ifdef USE_GOPHER
            else if (this->scheme == SCM_GOPHER && this->file[0] == '/')
            {
                p = this->file;
                this->file = allocStr(p + 1, -1);
            }
#endif /* USE_GOPHER */
        }
        else
        { /* scheme:[?query][#label] */
            this->path = current->path;
            if (this->query.empty())
                this->query = current->query;
        }
        /* comment: query part need not to be completed
	 * from the current URL. */
    }
    if (this->path.size())
    {
#ifdef __EMX__
        if (this->scheme == SCM_LOCAL)
        {
            if (strncmp(this->file, "/$LIB/", 6))
            {
                char abs[_MAX_PATH];

                _abspath(abs, file_unquote(this->file), _MAX_PATH);
                this->file = file_quote(cleanupName(abs));
            }
        }
#else
        if (this->scheme == SCM_LOCAL && this->path[0] != '/' && this->path != "-")
        {
            /* local file, relative path */
            tmp = Strnew(w3mApp::Instance().CurrentDir);
            if (tmp->Back() != '/')
                tmp->Push('/');
            tmp->Push(file_unquote(this->path));
            this->path = file_quote(cleanupName(tmp->ptr));
        }
#endif
        else if (this->scheme == SCM_HTTP
#ifdef USE_SSL
                 || this->scheme == SCM_HTTPS
#endif
        )
        {
            if (relative_uri)
            {
                /* In this case, this->file is created by [process 1] above.
		 * this->file may contain relative path (for example, 
		 * "/foo/../bar/./baz.html"), cleanupName() must be applied.
		 * When the entire abs_path is given, it still may contain
		 * elements like `//', `..' or `.' in the this->file. It is 
		 * server's responsibility to canonicalize such path.
		 */
                this->path = cleanupName(this->path.c_str());
            }
        }
        else if (
#ifdef USE_GOPHER
            this->scheme != SCM_GOPHER &&
#endif /* USE_GOPHER */
            this->path[0] == '/')
        {
            /*
	     * this happens on the following conditions:
	     * (1) ftp scheme (2) local, looks like absolute path.
	     * In both case, there must be no side effect with
	     * cleanupName(). (I hope so...)
	     */
            this->path = cleanupName(this->path.c_str());
        }
        if (this->scheme == SCM_LOCAL)
        {
#ifdef SUPPORT_NETBIOS_SHARE
            if (this->host && strcmp(this->host, "localhost") != 0)
            {
                Str tmp = Strnew("//");
                Strcat_m_charp(tmp, this->host,
                               cleanupName(file_unquote(this->file)), NULL);
                this->real_file = tmp->ptr;
            }
            else
#endif
                this->real_file = cleanupName(file_unquote(this->path));
        }
    }
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
        if (this->scheme == SCM_DATA)
        {
            tmp->Push(this->path);
            return tmp;
        }
        if (this->scheme != SCM_NEWS && this->scheme != SCM_NEWS_GROUP)
        {
            tmp->Push("//");
        }
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
            if (this->port != DefaultPort[this->scheme])
            {
                tmp->Push(':');
                tmp->Push(Sprintf("%d", this->port));
            }
        }
        if (this->scheme != SCM_NEWS && this->scheme != SCM_NEWS_GROUP &&
            (this->path.empty() || this->path[0] != '/'))
            tmp->Push('/');
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

static int
domain_match(char *pat, char *domain)
{
    if (domain == NULL)
        return 0;
    if (*pat == '.')
        pat++;
    for (;;)
    {
        if (!strcasecmp(pat, domain))
            return 1;
        domain = strchr(domain, '.');
        if (domain == NULL)
            return 0;
        domain++;
    }
}

int check_no_proxy(char *domain)
{
    TextListItem *tl;
    int ret = 0;
    MySignalHandler prevtrap = NULL;

    if (w3mApp::Instance().NO_proxy_domains == NULL || w3mApp::Instance().NO_proxy_domains->nitem == 0 ||
        domain == NULL)
        return 0;
    for (tl = w3mApp::Instance().NO_proxy_domains->first; tl != NULL; tl = tl->next)
    {
        if (domain_match(tl->ptr, domain))
            return 1;
    }
    if (!NOproxy_netaddr)
    {
        return 0;
    }
    /* 
     * to check noproxy by network addr
     */

    auto success = TrapJmp([&]() {
        {
#ifndef INET6
            struct hostent *he;
            int n;
            unsigned char **h_addr_list;
            char addr[4 * 16], buf[5];

            he = gethostbyname(domain);
            if (!he)
            {
                ret = 0;
                goto end;
            }
            for (h_addr_list = (unsigned char **)he->h_addr_list; *h_addr_list;
                 h_addr_list++)
            {
                sprintf(addr, "%d", h_addr_list[0][0]);
                for (n = 1; n < he->h_length; n++)
                {
                    sprintf(buf, ".%d", h_addr_list[0][n]);
                    addr->Push(buf);
                }
                for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next)
                {
                    if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0)
                    {
                        ret = 1;
                        goto end;
                    }
                }
            }
#else  /* INET6 */
            int error;
            struct addrinfo hints;
            struct addrinfo *res, *res0;
            char addr[4 * 16];
            int *af;

            for (af = ai_family_order_table[DNS_order];; af++)
            {
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = *af;
                error = getaddrinfo(domain, NULL, &hints, &res0);
                if (error)
                {
                    if (*af == PF_UNSPEC)
                    {
                        break;
                    }
                    /* try next */
                    continue;
                }
                for (res = res0; res != NULL; res = res->ai_next)
                {
                    switch (res->ai_family)
                    {
                    case AF_INET:
                        inet_ntop(AF_INET,
                                  &((struct sockaddr_in *)res->ai_addr)->sin_addr,
                                  addr, sizeof(addr));
                        break;
                    case AF_INET6:
                        inet_ntop(AF_INET6,
                                  &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, addr, sizeof(addr));
                        break;
                    default:
                        /* unknown */
                        continue;
                    }
                    for (tl = w3mApp::Instance().NO_proxy_domains->first; tl != NULL; tl = tl->next)
                    {
                        if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0)
                        {
                            freeaddrinfo(res0);
                            ret = 1;
                            return true;
                        }
                    }
                }
                freeaddrinfo(res0);
                if (*af == PF_UNSPEC)
                {
                    break;
                }
            }
#endif /* INET6 */
        }

        return true;
    });

    if (!success)
    {
        ret = 0;
    }

    return ret;
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
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}
