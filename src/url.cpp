/* $Id: url.c,v 1.100 2010/12/15 10:50:24 htrb Exp $ */

#include "fm.h"
#include "etc.h"
#include "url.h"
#include "file.h"
#include "indep.h"
#include "cookie.h"
#include "terms.h"
#include "form.h"
#include "display.h"
#include "anchor.h"
#include "http_request.h"

#ifndef __MINGW32_VERSION
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#else
#include <winsock.h>
#endif /* __MINGW32_VERSION */

#include <assert.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <string_view>

#include <sys/stat.h>
#ifdef __EMX__
#include <io.h> /* ?? */
#endif          /* __EMX__ */

#include "html.h"
#include "Str.h"
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
#ifdef USE_SSL
        443, /* https */
#endif       /* USE_SSL */
};

SchemeKeyValue schemetable[] = {
    {"http", SCM_HTTP},
    {"gopher", SCM_GOPHER},
    {"ftp", SCM_FTP},
    {"local", SCM_LOCAL},
    {"file", SCM_LOCAL},
    /*  {"exec", SCM_EXEC}, */
    {"nntp", SCM_NNTP},
    /*  {"nntp", SCM_NNTP_GROUP}, */
    {"news", SCM_NEWS},
    /*  {"news", SCM_NEWS_GROUP}, */
    {"data", SCM_DATA},
    {"mailto", SCM_MAILTO},
    {"https", SCM_HTTPS},
    // {NULL, SCM_UNKNOWN},
};

SchemeKeyValue &GetScheme(int index)
{
    return schemetable[index];
}

Str tmp;
static const char *scheme_str[] = {
    "http",
    "gopher",
    "ftp",
    "ftp",
    "file",
    "file",
    "exec",
    "nntp",
    "nntp",
    "news",
    "news",
    "data",
    "mailto",
#ifdef USE_SSL
    "https",
#endif /* USE_SSL */
};

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

static char *
DefaultFile(int scheme)
{
    switch (scheme)
    {
    case SCM_HTTP:
#ifdef USE_SSL
    case SCM_HTTPS:
#endif /* USE_SSL */
        return allocStr(HTTP_DEFAULT_FILE, -1);
#ifdef USE_GOPHER
    case SCM_GOPHER:
        return allocStr("1", -1);
#endif /* USE_GOPHER */
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
    case SCM_FTP:
    case SCM_FTPDIR:
        return allocStr("/", -1);
    }
    return NULL;
}

static void
    KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
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
    if (SETJMP(AbortLoading) != 0)
    {
#ifdef SOCK_DEBUG
        sock_log("openSocket() failed. reason: user abort\n");
#endif
        if (sock >= 0)
            close(sock);
        goto error;
    }
    TRAP_ON;
    if (hostname == NULL)
    {
#ifdef SOCK_DEBUG
        sock_log("openSocket() failed. reason: Bad hostname \"%s\"\n",
                 hostname);
#endif
        goto error;
    }

#ifdef INET6
    /* rfc2732 compliance */
    hname = const_cast<char *>(hostname);
    if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']')
    {
        hname = allocStr(hostname + 1, -1);
        hname[strlen(hname) - 1] = '\0';
        if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
            goto error;
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
                goto error;
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
                goto error;
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

    TRAP_OFF;
    return sock;
error:
    TRAP_OFF;
    return -1;
}

#define COPYPATH_SPC_ALLOW 0
#define COPYPATH_SPC_IGNORE 1
#define COPYPATH_SPC_REPLACE 2

static char *
copyPath(char *orgpath, int length, int option)
{
    Str tmp = Strnew();
    while (*orgpath && length != 0)
    {
        if (IS_SPACE(*orgpath))
        {
            switch (option)
            {
            case COPYPATH_SPC_ALLOW:
                tmp->Push(*orgpath);
                break;
            case COPYPATH_SPC_IGNORE:
                /* do nothing */
                break;
            case COPYPATH_SPC_REPLACE:
                tmp->Push("%20");
                break;
            }
        }
        else
            tmp->Push(*orgpath);
        orgpath++;
        length--;
    }
    return tmp->ptr;
}

SchemaTypes getURLScheme(char **url)
{
    // heading
    std::string_view view(*url);
    size_t i = 0;
    for (; i < view.size(); ++i, ++(*url))
    {
        auto p = *url;
        if ((IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
        {
            continue;
        }
        else
        {
            break;
        }
    }
    if (**url != ':')
    {
        return SCM_MISSING;
    }
    ++(*url);

    for (auto &scheme : schemetable)
    {
        if (view.starts_with(scheme.name) && view[scheme.name.size()] == ':')
        {
            return scheme.schema;
        }
    }

    return SCM_UNKNOWN;
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

void ParsedURL::Parse(std::string_view _url, const ParsedURL *current)
{
    *this = {};

    /* quote 0x01-0x20, 0x7F-0xFF */
    auto url = url_quote(const_cast<char *>(_url.data()));
    auto p = url;
    char *q = nullptr;

    /* RFC1808: Relative Uniform Resource Locators
     * 4.  Resolving Relative URLs
     */
    if (*url == '\0' || *url == '#')
    {
        if (current)
            *this = *current;
        goto do_label;
    }
    
    /* search for scheme */
    this->scheme = getURLScheme(&p);
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
        p = url;
        if (!strncmp(p, "//", 2))
        {
            /* URL begins with // */
            /* it means that 'scheme:' is abbreviated */
            p += 2;
            goto analyze_url;
        }
        /* the url doesn't begin with '//' */
        goto analyze_file;
    }
    /* scheme part has been found */
    if (this->scheme == SCM_UNKNOWN)
    {
        this->file = allocStr(url, -1);
        return;
    }
    /* get host and port */
    if (p[0] != '/' || p[1] != '/')
    { /* scheme:foo or scheme:/foo */
        this->host.clear();
        if (this->scheme != SCM_UNKNOWN)
            this->port = DefaultPort[this->scheme];
        else
            this->port = 0;
        goto analyze_file;
    }
    /* after here, p begins with // */
    if (this->scheme == SCM_LOCAL)
    { /* file://foo           */
#ifdef __EMX__
        p += 2;
        goto analyze_file;
#else
        if (p[2] == '/' || p[2] == '~'
        /* <A HREF="file:///foo">file:///foo</A>  or <A HREF="file://~user">file://~user</A> */
#ifdef SUPPORT_DOS_DRIVE_PREFIX
            || (IS_ALPHA(p[2]) && (p[3] == ':' || p[3] == '|'))
        /* <A HREF="file://DRIVE/foo">file://DRIVE/foo</A> */
#endif /* SUPPORT_DOS_DRIVE_PREFIX */
        )
        {
            p += 2;
            goto analyze_file;
        }
#endif /* __EMX__ */
    }
    p += 2; /* scheme://foo         */
            /*          ^p is here  */
analyze_url:
    q = p;
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
            this->pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
            q = ++p;
            this->user = this->host;
            this->host.clear();
            goto analyze_url;
        }
        /* scheme://host:port/ */
        tmp = Strnew_charp_n(q, p - q);
        this->port = atoi(tmp->ptr);
        /* *p is one of ['\0', '/', '?', '#'] */
        break;
    case '@':
        /* scheme://user@...            */
        this->user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
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
analyze_file:
#ifndef SUPPORT_NETBIOS_SHARE
    if (this->scheme == SCM_LOCAL && this->user.empty() &&
        this->host.size() && this->host[0] != '\0' &&
        this->host != "localhost")
    {
        this->scheme = SCM_FTP; /* ftp://host/... */
        if (this->port == 0)
            this->port = DefaultPort[SCM_FTP];
    }
#endif
    if ((*p == '\0' || *p == '#' || *p == '?') && this->host.empty())
    {
        this->file = "";
        goto do_query;
    }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (this->scheme == SCM_LOCAL)
    {
        q = p;
        if (*q == '/')
            q++;
        if (IS_ALPHA(q[0]) && (q[1] == ':' || q[1] == '|'))
        {
            if (q[1] == '|')
            {
                p = allocStr(q, -1);
                p[1] = ':';
            }
            else
                p = q;
        }
    }
#endif

    q = p;
#ifdef USE_GOPHER
    if (this->scheme == SCM_GOPHER)
    {
        if (*q == '/')
            q++;
        if (*q && q[0] != '/' && q[1] != '/' && q[2] == '/')
            q++;
    }
#endif /* USE_GOPHER */
    if (*p == '/')
        p++;
    if (*p == '\0' || *p == '#' || *p == '?')
    { /* scheme://host[:port]/ */
        this->file = DefaultFile(this->scheme);
        goto do_query;
    }
    {
        char *cgi = strchr(p, '?');
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
            this->file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
        else
            this->file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    }

do_query:
    if (*p == '?')
    {
        q = ++p;
        while (*p && *p != '#')
            p++;
        this->query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    }
do_label:
    if (this->scheme == SCM_MISSING)
    {
        this->scheme = SCM_LOCAL;
        this->file = p;
        this->label.clear();
    }
    else if (*p == '#')
        this->label = p + 1;
    else
        this->label.clear();
}

void ParsedURL::Parse2(std::string_view url, const ParsedURL *current)
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
        if (this->file.size() && !string_strchr(this->file, '@') &&
            (!(p = string_strchr(this->file, '/')) || strchr(p + 1, '-') ||
             *(p + 1) == '\0'))
            this->scheme = SCM_NEWS_GROUP;
        else
            this->scheme = SCM_NEWS;
        return;
    }
    if (this->scheme == SCM_NNTP || this->scheme == SCM_NNTP_GROUP)
    {
        if (this->file.size() && this->file[0] == '/')
            this->file = this->file.substr(1);
        if (this->file.size() && !string_strchr(this->file, '@') &&
            (!(p = string_strchr(this->file.c_str(), '/')) || strchr(p + 1, '-') ||
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
        char *q = expandName(file_unquote(this->file));
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
            this->file = file_quote(q);
    }

    if (current && (this->scheme == current->scheme || (this->scheme == SCM_FTP && current->scheme == SCM_FTPDIR) || (this->scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI)) && this->host.empty())
    {
        /* Copy omitted element from the current URL */
        this->user = current->user;
        this->pass = current->pass;
        this->host = current->host;
        this->port = current->port;
        if (this->file.size() && this->file[0])
        {
#ifdef USE_EXTERNAL_URI_LOADER
            if (this->scheme == SCM_UNKNOWN && strchr(const_cast<char *>(this->file.c_str()), ':') == NULL && current && (p = string_strchr(current->file, ':')) != NULL)
            {
                this->file = Strnew_m_charp(current->file.substr(0, p - current->file.c_str()), ":", this->file)->ptr;
            }
            else
#endif
                if (
#ifdef USE_GOPHER
                    this->scheme != SCM_GOPHER &&
#endif /* USE_GOPHER */
                    this->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
                    && !(this->scheme == SCM_LOCAL && IS_ALPHA(this->file[0]) && this->file[1] == ':')
#endif
                )
            {
                /* file is relative [process 1] */
                p = this->file.c_str();
                if (current->file.size())
                {
                    tmp = Strnew(current->file);
                    while (tmp->Size() > 0)
                    {
                        if (tmp->Back() == '/')
                            break;
                        tmp->Pop(1);
                    }
                    tmp->Push(p);
                    this->file = tmp->ptr;
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
            this->file = current->file;
            if (this->query.empty())
                this->query = current->query;
        }
        /* comment: query part need not to be completed
	 * from the current URL. */
    }
    if (this->file.size())
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
        if (this->scheme == SCM_LOCAL && this->file[0] != '/' &&
#ifdef SUPPORT_DOS_DRIVE_PREFIX /* for 'drive:' */
            !(IS_ALPHA(this->file[0]) && this->file[1] == ':') &&
#endif
            this->file != "-")
        {
            /* local file, relative path */
            tmp = Strnew(CurrentDir);
            if (tmp->Back() != '/')
                tmp->Push('/');
            tmp->Push(file_unquote(this->file));
            this->file = file_quote(cleanupName(tmp->ptr));
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
                this->file = cleanupName(this->file.c_str());
            }
        }
        else if (
#ifdef USE_GOPHER
            this->scheme != SCM_GOPHER &&
#endif /* USE_GOPHER */
            this->file[0] == '/')
        {
            /*
	     * this happens on the following conditions:
	     * (1) ftp scheme (2) local, looks like absolute path.
	     * In both case, there must be no side effect with
	     * cleanupName(). (I hope so...)
	     */
            this->file = cleanupName(this->file.c_str());
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
                this->real_file = cleanupName(file_unquote(this->file));
        }
    }
}

Str ParsedURL::ToStr(bool pass) const
{
    if (this->scheme == SCM_MISSING)
    {
        return Strnew("???");
    }
    else if (this->scheme == SCM_UNKNOWN)
    {
        return Strnew(this->file);
    }
    if (this->host.empty() && this->file.empty() && this->label.size())
    {
        /* local label */
        return Sprintf("#%s", this->label);
    }
    if (this->scheme == SCM_LOCAL && this->file == "-")
    {
        tmp = Strnew("-");
        if (this->label.size())
        {
            tmp->Push('#');
            tmp->Push(this->label);
        }
        return tmp;
    }
    tmp = Strnew(scheme_str[this->scheme]);
    tmp->Push(':');
    if (this->scheme == SCM_DATA)
    {
        tmp->Push(this->file);
        return tmp;
    }
    if (this->scheme != SCM_NEWS && this->scheme != SCM_NEWS_GROUP)
    {
        tmp->Push("//");
    }
    if (this->user.size())
    {
        tmp->Push(this->user);
        if (pass && this->pass.size())
        {
            tmp->Push(':');
            tmp->Push(this->pass);
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
        (this->file.empty() || this->file[0] != '/'))
        tmp->Push('/');
    tmp->Push(this->file);
    if (this->scheme == SCM_FTPDIR && tmp->Back() != '/')
        tmp->Push('/');
    if (this->query.size())
    {
        tmp->Push('?');
        tmp->Push(this->query);
    }
    if (this->label.size())
    {
        tmp->Push('#');
        tmp->Push(this->label);
    }
    return tmp;
}

char *
otherinfo(ParsedURL *target, const ParsedURL *current, char *referer)
{
    Str s = Strnew();

    s->Push("User-Agent: ");
    if (UserAgent == NULL || *UserAgent == '\0')
        s->Push(w3m_version);
    else
        s->Push(UserAgent);
    s->Push("\r\n");

    Strcat_m_charp(s, "Accept: ", AcceptMedia, "\r\n", NULL);
    Strcat_m_charp(s, "Accept-Encoding: ", AcceptEncoding, "\r\n", NULL);
    Strcat_m_charp(s, "Accept-Language: ", AcceptLang, "\r\n", NULL);

    if (target->host.size())
    {
        s->Push("Host: ");
        s->Push(target->host);
        if (target->port != DefaultPort[target->scheme])
            s->Push(Sprintf(":%d", target->port));
        s->Push("\r\n");
    }
    if (target->is_nocache || NoCache)
    {
        s->Push("Pragma: no-cache\r\n");
        s->Push("Cache-control: no-cache\r\n");
    }
    if (!NoSendReferer)
    {
        if (current && current->scheme == SCM_HTTPS && target->scheme != SCM_HTTPS)
        {
            /* Don't send Referer: if https:// -> http:// */
        }
        else if (referer == NULL && current && current->scheme != SCM_LOCAL &&
                 (current->scheme != SCM_FTP ||
                  (current->user.empty() && current->pass.empty())))
        {
            // char *p = current->label;
            s->Push("Referer: ");
            //current->label = NULL;
            auto withoutLabel = *current;
            withoutLabel.label.clear();
            s->Push(withoutLabel.ToStr());
            s->Push("\r\n");
        }
        else if (referer != NULL && referer != NO_REFERER)
        {
            char *p = strchr(referer, '#');
            s->Push("Referer: ");
            if (p)
                s->Push(referer, p - referer);
            else
                s->Push(referer);
            s->Push("\r\n");
        }
    }
    return s->ptr;
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

    if (NO_proxy_domains == NULL || NO_proxy_domains->nitem == 0 ||
        domain == NULL)
        return 0;
    for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next)
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
    if (SETJMP(AbortLoading) != 0)
    {
        ret = 0;
        goto end;
    }
    TRAP_ON;
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
                for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next)
                {
                    if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0)
                    {
                        freeaddrinfo(res0);
                        ret = 1;
                        goto end;
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
end:
    TRAP_OFF;
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

#ifdef USE_EXTERNAL_URI_LOADER
static struct table2 **urimethods;
static struct table2 default_urimethods[] = {
    {"mailto", "file:///$LIB/w3mmail.cgi?%s"},
    {NULL, NULL}};

static struct table2 *
loadURIMethods(char *filename)
{
    FILE *f;
    int i, n;
    Str tmp;
    struct table2 *um;
    char *up, *p;

    f = fopen(expandPath(filename), "r");
    if (f == NULL)
        return NULL;
    i = 0;
    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        if (tmp->ptr[0] != '#')
            i++;
    }
    fseek(f, 0, 0);
    n = i;
    um = New_N(struct table2, n + 1);
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
                um[i].item1 = Strnew_charp_n(up, p - up)->ptr;
                p++;
                break;
            }
        }
        if (*p == '\0')
            continue;
        while (*p != '\0' && IS_SPACE(*p))
            p++;
        um[i].item2 = Strnew(p)->ptr;
        i++;
    }
    um[i].item1 = NULL;
    um[i].item2 = NULL;
    fclose(f);
    return um;
}

void initURIMethods()
{
    TextList *methodmap_list = NULL;
    TextListItem *tl;
    int i;

    if (non_null(urimethodmap_files))
        methodmap_list = make_domain_list(urimethodmap_files);
    if (methodmap_list == NULL)
        return;
    urimethods = New_N(struct table2 *, (methodmap_list->nitem + 1));
    for (i = 0, tl = methodmap_list->first; tl; tl = tl->next)
    {
        urimethods[i] = loadURIMethods(tl->ptr);
        if (urimethods[i])
            i++;
    }
    urimethods[i] = NULL;
}

Str searchURIMethods(ParsedURL *pu)
{
    struct table2 *ump;
    int i;
    Str scheme = NULL;
    Str url;
    char *p;

    if (pu->scheme != SCM_UNKNOWN)
        return NULL; /* use internal */
    if (urimethods == NULL)
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
    for (i = 0; (ump = urimethods[i]) != NULL; i++)
    {
        for (; ump->item1 != NULL; ump++)
        {
            if (strcasecmp(ump->item1, scheme->ptr) == 0)
            {
                return Sprintf(ump->item2, url_quote(url->ptr));
            }
        }
    }
    for (ump = default_urimethods; ump->item1 != NULL; ump++)
    {
        if (strcasecmp(ump->item1, scheme->ptr) == 0)
        {
            return Sprintf(ump->item2, url_quote(url->ptr));
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
void chkExternalURIBuffer(BufferPtr buf)
{
    int i;
    struct table2 *ump;

    for (i = 0; (ump = urimethods[i]) != NULL; i++)
    {
        for (; ump->item1 != NULL; ump++)
        {
            reAnchor(buf, Sprintf("%s:%s", ump->item1, URI_PATTERN)->ptr);
        }
    }
    for (ump = default_urimethods; ump->item1 != NULL; ump++)
    {
        reAnchor(buf, Sprintf("%s:%s", ump->item1, URI_PATTERN)->ptr);
    }
}
#endif

ParsedURL *
schemeToProxy(int scheme)
{
    ParsedURL *pu = NULL; /* for gcc */
    switch (scheme)
    {
    case SCM_HTTP:
        pu = &HTTP_proxy_parsed;
        break;
#ifdef USE_SSL
    case SCM_HTTPS:
        pu = &HTTPS_proxy_parsed;
        break;
#endif
    case SCM_FTP:
        pu = &FTP_proxy_parsed;
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
