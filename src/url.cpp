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

#ifdef USE_SSL
#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>
#endif

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

struct SchemaKeyValue
{
    std::string_view name;
    SchemaTypes schema;
};

SchemaKeyValue schemetable[] = {
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

static void add_index_file(ParsedURL *pu, URLFile *uf);

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

#ifdef USE_SSL
SSL_CTX *ssl_ctx = NULL;

void free_ssl_ctx()
{
    if (ssl_ctx != NULL)
        SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
    ssl_accept_this_site(NULL);
}

#if SSLEAY_VERSION_NUMBER >= 0x00905100
#include <openssl/rand.h>
static void
init_PRNG()
{
    char buffer[256];
    const char *file;
    long l;
    if (RAND_status())
        return;
    if ((file = RAND_file_name(buffer, sizeof(buffer))))
    {
#ifdef USE_EGD
        if (RAND_egd(file) > 0)
            return;
#endif
        RAND_load_file(file, -1);
    }
    if (RAND_status())
        goto seeded;
    srand48((long)time(NULL));
    while (!RAND_status())
    {
        l = lrand48();
        RAND_seed((unsigned char *)&l, sizeof(long));
    }
seeded:
    if (file)
        RAND_write_file(file);
}
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */

static SSL *
openSSLHandle(int sock, char *hostname, char **p_cert)
{
    SSL *handle = NULL;
    static char *old_ssl_forbid_method = NULL;
#ifdef USE_SSL_VERIFY
    static int old_ssl_verify_server = -1;
#endif

    if (old_ssl_forbid_method != ssl_forbid_method && (!old_ssl_forbid_method || !ssl_forbid_method ||
                                                       strcmp(old_ssl_forbid_method, ssl_forbid_method)))
    {
        old_ssl_forbid_method = ssl_forbid_method;
#ifdef USE_SSL_VERIFY
        ssl_path_modified = 1;
#else
        free_ssl_ctx();
#endif
    }
#ifdef USE_SSL_VERIFY
    if (old_ssl_verify_server != ssl_verify_server)
    {
        old_ssl_verify_server = ssl_verify_server;
        ssl_path_modified = 1;
    }
    if (ssl_path_modified)
    {
        free_ssl_ctx();
        ssl_path_modified = 0;
    }
#endif /* defined(USE_SSL_VERIFY) */
    if (ssl_ctx == NULL)
    {
        int option;
#if SSLEAY_VERSION_NUMBER < 0x0800
        ssl_ctx = SSL_CTX_new();
        X509_set_default_verify_paths(ssl_ctx->cert);
#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
        SSLeay_add_ssl_algorithms();
        SSL_load_error_strings();
        if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
            goto eend;
        option = SSL_OP_ALL;
        if (ssl_forbid_method)
        {
            if (strchr(ssl_forbid_method, '2'))
                option |= SSL_OP_NO_SSLv2;
            if (strchr(ssl_forbid_method, '3'))
                option |= SSL_OP_NO_SSLv3;
            if (strchr(ssl_forbid_method, 't'))
                option |= SSL_OP_NO_TLSv1;
            if (strchr(ssl_forbid_method, 'T'))
                option |= SSL_OP_NO_TLSv1;
        }
        SSL_CTX_set_options(ssl_ctx, option);
#ifdef USE_SSL_VERIFY
        /* derived from openssl-0.9.5/apps/s_{client,cb}.c */
#if 1 /* use SSL_get_verify_result() to verify cert */
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
        SSL_CTX_set_verify(ssl_ctx,
                           ssl_verify_server ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
#endif
        if (ssl_cert_file != NULL && *ssl_cert_file != '\0')
        {
            int ng = 1;
            if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert_file, SSL_FILETYPE_PEM) > 0)
            {
                char *key_file = (ssl_key_file == NULL || *ssl_key_file ==
                                                              '\0')
                                     ? ssl_cert_file
                                     : ssl_key_file;
                if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) > 0)
                    if (SSL_CTX_check_private_key(ssl_ctx))
                        ng = 0;
            }
            if (ng)
            {
                free_ssl_ctx();
                goto eend;
            }
        }
        if ((!ssl_ca_file && !ssl_ca_path) || SSL_CTX_load_verify_locations(ssl_ctx, ssl_ca_file, ssl_ca_path))
#endif /* defined(USE_SSL_VERIFY) */
            SSL_CTX_set_default_verify_paths(ssl_ctx);
#endif /* SSLEAY_VERSION_NUMBER >= 0x0800 */
    }
    handle = SSL_new(ssl_ctx);
    SSL_set_fd(handle, sock);
#if SSLEAY_VERSION_NUMBER >= 0x00905100
    init_PRNG();
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
#if (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT)
    SSL_set_tlsext_host_name(handle, hostname);
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) */
    if (SSL_connect(handle) > 0)
    {
        Str serv_cert = ssl_get_certificate(handle, hostname);
        if (serv_cert)
        {
            *p_cert = serv_cert->ptr;
            return handle;
        }
        close(sock);
        SSL_free(handle);
        return NULL;
    }
eend:
    close(sock);
    if (handle)
        SSL_free(handle);
    /* FIXME: gettextize? */
    disp_err_message(Sprintf("SSL error: %s",
                             ERR_error_string(ERR_get_error(), NULL))
                         ->ptr,
                     FALSE);
    return NULL;
}

static void
SSL_write_from_file(SSL *ssl, char *file)
{
    FILE *fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL)
    {
        while ((c = fgetc(fd)) != EOF)
        {
            buf[0] = c;
            SSL_write(ssl, buf, 1);
        }
        fclose(fd);
    }
}

#endif /* USE_SSL */

static void
write_from_file(int sock, char *file)
{
    FILE *fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL)
    {
        while ((c = fgetc(fd)) != EOF)
        {
            buf[0] = c;
            write(sock, buf, 1);
        }
        fclose(fd);
    }
}

ParsedURL *
baseURL(BufferPtr buf)
{
    if (buf->bufferprop & BP_NO_URL)
    {
        /* no URL is defined for the buffer */
        return NULL;
    }
    if (buf->baseURL != NULL)
    {
        /* <BASE> tag is defined in the document */
        return buf->baseURL;
    }
    else
        return &buf->currentURL;
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

static SchemaTypes getURLScheme(char **url)
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

void parseURL(char *url, ParsedURL *p_url, ParsedURL *current)
{
    char *p, *q;
    Str tmp;

    url = url_quote(url); /* quote 0x01-0x20, 0x7F-0xFF */

    p = url;
    p_url->scheme = SCM_MISSING;
    p_url->port = 0;
    p_url->user = NULL;
    p_url->pass = NULL;
    p_url->host = NULL;
    p_url->is_nocache = 0;
    p_url->file = NULL;
    p_url->real_file = NULL;
    p_url->query = NULL;
    p_url->label = NULL;

    /* RFC1808: Relative Uniform Resource Locators
     * 4.  Resolving Relative URLs
     */
    if (*url == '\0' || *url == '#')
    {
        if (current)
            copyParsedURL(p_url, current);
        goto do_label;
    }
    /* search for scheme */
    p_url->scheme = getURLScheme(&p);
    if (p_url->scheme == SCM_MISSING)
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
                p_url->scheme = SCM_LOCAL;
                break;
            case SCM_FTP:
            case SCM_FTPDIR:
                p_url->scheme = SCM_FTP;
                break;
#ifdef USE_NNTP
            case SCM_NNTP:
            case SCM_NNTP_GROUP:
                p_url->scheme = SCM_NNTP;
                break;
            case SCM_NEWS:
            case SCM_NEWS_GROUP:
                p_url->scheme = SCM_NEWS;
                break;
#endif
            default:
                p_url->scheme = current->scheme;
                break;
            }
        }
        else
            p_url->scheme = SCM_LOCAL;
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
    if (p_url->scheme == SCM_UNKNOWN)
    {
        p_url->file = allocStr(url, -1);
        return;
    }
    /* get host and port */
    if (p[0] != '/' || p[1] != '/')
    { /* scheme:foo or scheme:/foo */
        p_url->host = NULL;
        if (p_url->scheme != SCM_UNKNOWN)
            p_url->port = DefaultPort[p_url->scheme];
        else
            p_url->port = 0;
        goto analyze_file;
    }
    /* after here, p begins with // */
    if (p_url->scheme == SCM_LOCAL)
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
        p_url->host = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
        q = ++p;
        while (*p && strchr("@/?#", *p) == NULL)
            p++;
        if (*p == '@')
        {
            /* scheme://user:pass@...       */
            p_url->pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
            q = ++p;
            p_url->user = p_url->host;
            p_url->host = NULL;
            goto analyze_url;
        }
        /* scheme://host:port/ */
        tmp = Strnew_charp_n(q, p - q);
        p_url->port = atoi(tmp->ptr);
        /* *p is one of ['\0', '/', '?', '#'] */
        break;
    case '@':
        /* scheme://user@...            */
        p_url->user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
        q = ++p;
        goto analyze_url;
    case '\0':
    /* scheme://host                */
    case '/':
    case '?':
    case '#':
        p_url->host = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
        p_url->port = DefaultPort[p_url->scheme];
        break;
    }
analyze_file:
#ifndef SUPPORT_NETBIOS_SHARE
    if (p_url->scheme == SCM_LOCAL && p_url->user == NULL &&
        p_url->host != NULL && *p_url->host != '\0' &&
        strcmp(p_url->host, "localhost"))
    {
        p_url->scheme = SCM_FTP; /* ftp://host/... */
        if (p_url->port == 0)
            p_url->port = DefaultPort[SCM_FTP];
    }
#endif
    if ((*p == '\0' || *p == '#' || *p == '?') && p_url->host == NULL)
    {
        p_url->file = "";
        goto do_query;
    }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (p_url->scheme == SCM_LOCAL)
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
    if (p_url->scheme == SCM_GOPHER)
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
        p_url->file = DefaultFile(p_url->scheme);
        goto do_query;
    }
#ifdef USE_GOPHER
    if (p_url->scheme == SCM_GOPHER && *p == 'R')
    {
        p++;
        tmp = Strnew();
        tmp->Push(*(p++));
        while (*p && *p != '/')
            p++;
        tmp->Push(p);
        while (*p)
            p++;
        p_url->file = copyPath(tmp->ptr, -1, COPYPATH_SPC_IGNORE);
    }
    else
#endif /* USE_GOPHER */
    {
        char *cgi = strchr(p, '?');
    again:
        while (*p && *p != '#' && p != cgi)
            p++;
        if (*p == '#' && p_url->scheme == SCM_LOCAL)
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
        if (p_url->scheme == SCM_LOCAL || p_url->scheme == SCM_MISSING)
            p_url->file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
        else
            p_url->file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    }

do_query:
    if (*p == '?')
    {
        q = ++p;
        while (*p && *p != '#')
            p++;
        p_url->query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    }
do_label:
    if (p_url->scheme == SCM_MISSING)
    {
        p_url->scheme = SCM_LOCAL;
        p_url->file = allocStr(p, -1);
        p_url->label = NULL;
    }
    else if (*p == '#')
        p_url->label = allocStr(p + 1, -1);
    else
        p_url->label = NULL;
}

#define initParsedURL(p) bzero(p, sizeof(ParsedURL))
#define ALLOC_STR(s) ((s) == NULL ? NULL : allocStr(s, -1))

void copyParsedURL(ParsedURL *p, ParsedURL *q)
{
    p->scheme = q->scheme;
    p->port = q->port;
    p->is_nocache = q->is_nocache;
    p->user = ALLOC_STR(q->user);
    p->pass = ALLOC_STR(q->pass);
    p->host = ALLOC_STR(q->host);
    p->file = ALLOC_STR(q->file);
    p->real_file = ALLOC_STR(q->real_file);
    p->label = ALLOC_STR(q->label);
    p->query = ALLOC_STR(q->query);
}

void parseURL2(char *url, ParsedURL *pu, ParsedURL *current)
{
    char *p;
    Str tmp;
    int relative_uri = FALSE;

    parseURL(url, pu, current);
#ifndef USE_W3MMAILER
    if (pu->scheme == SCM_MAILTO)
        return;
#endif
    if (pu->scheme == SCM_DATA)
        return;
    if (pu->scheme == SCM_NEWS || pu->scheme == SCM_NEWS_GROUP)
    {
        if (pu->file && !strchr(pu->file, '@') &&
            (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') ||
             *(p + 1) == '\0'))
            pu->scheme = SCM_NEWS_GROUP;
        else
            pu->scheme = SCM_NEWS;
        return;
    }
    if (pu->scheme == SCM_NNTP || pu->scheme == SCM_NNTP_GROUP)
    {
        if (pu->file && *pu->file == '/')
            pu->file = allocStr(pu->file + 1, -1);
        if (pu->file && !strchr(pu->file, '@') &&
            (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') ||
             *(p + 1) == '\0'))
            pu->scheme = SCM_NNTP_GROUP;
        else
            pu->scheme = SCM_NNTP;
        if (current && (current->scheme == SCM_NNTP ||
                        current->scheme == SCM_NNTP_GROUP))
        {
            if (pu->host == NULL)
            {
                pu->host = current->host;
                pu->port = current->port;
            }
        }
        return;
    }
    if (pu->scheme == SCM_LOCAL)
    {
        char *q = expandName(file_unquote(pu->file));
#ifdef SUPPORT_DOS_DRIVE_PREFIX
        Str drive;
        if (IS_ALPHA(q[0]) && q[1] == ':')
        {
            drive = Strnew_charp_n(q, 2);
            drive->Push(file_quote(q + 2));
            pu->file = drive->ptr;
        }
        else
#endif
            pu->file = file_quote(q);
    }

    if (current && (pu->scheme == current->scheme || (pu->scheme == SCM_FTP && current->scheme == SCM_FTPDIR) || (pu->scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI)) && pu->host == NULL)
    {
        /* Copy omitted element from the current URL */
        pu->user = current->user;
        pu->pass = current->pass;
        pu->host = current->host;
        pu->port = current->port;
        if (pu->file && *pu->file)
        {
#ifdef USE_EXTERNAL_URI_LOADER
            if (pu->scheme == SCM_UNKNOWN && strchr(pu->file, ':') == NULL && current && (p = strchr(current->file, ':')) != NULL)
            {
                pu->file = Sprintf("%s:%s",
                                   allocStr(current->file,
                                            p - current->file),
                                   pu->file)
                               ->ptr;
            }
            else
#endif
                if (
#ifdef USE_GOPHER
                    pu->scheme != SCM_GOPHER &&
#endif /* USE_GOPHER */
                    pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
                    && !(pu->scheme == SCM_LOCAL && IS_ALPHA(pu->file[0]) && pu->file[1] == ':')
#endif
                )
            {
                /* file is relative [process 1] */
                p = pu->file;
                if (current->file)
                {
                    tmp = Strnew_charp(current->file);
                    while (tmp->Size() > 0)
                    {
                        if (tmp->Back() == '/')
                            break;
                        tmp->Pop(1);
                    }
                    tmp->Push(p);
                    pu->file = tmp->ptr;
                    relative_uri = TRUE;
                }
            }
#ifdef USE_GOPHER
            else if (pu->scheme == SCM_GOPHER && pu->file[0] == '/')
            {
                p = pu->file;
                pu->file = allocStr(p + 1, -1);
            }
#endif /* USE_GOPHER */
        }
        else
        { /* scheme:[?query][#label] */
            pu->file = current->file;
            if (!pu->query)
                pu->query = current->query;
        }
        /* comment: query part need not to be completed
	 * from the current URL. */
    }
    if (pu->file)
    {
#ifdef __EMX__
        if (pu->scheme == SCM_LOCAL)
        {
            if (strncmp(pu->file, "/$LIB/", 6))
            {
                char abs[_MAX_PATH];

                _abspath(abs, file_unquote(pu->file), _MAX_PATH);
                pu->file = file_quote(cleanupName(abs));
            }
        }
#else
        if (pu->scheme == SCM_LOCAL && pu->file[0] != '/' &&
#ifdef SUPPORT_DOS_DRIVE_PREFIX /* for 'drive:' */
            !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':') &&
#endif
            strcmp(pu->file, "-"))
        {
            /* local file, relative path */
            tmp = Strnew_charp(CurrentDir);
            if (tmp->Back() != '/')
                tmp->Push('/');
            tmp->Push(file_unquote(pu->file));
            pu->file = file_quote(cleanupName(tmp->ptr));
        }
#endif
        else if (pu->scheme == SCM_HTTP
#ifdef USE_SSL
                 || pu->scheme == SCM_HTTPS
#endif
        )
        {
            if (relative_uri)
            {
                /* In this case, pu->file is created by [process 1] above.
		 * pu->file may contain relative path (for example, 
		 * "/foo/../bar/./baz.html"), cleanupName() must be applied.
		 * When the entire abs_path is given, it still may contain
		 * elements like `//', `..' or `.' in the pu->file. It is 
		 * server's responsibility to canonicalize such path.
		 */
                pu->file = cleanupName(pu->file);
            }
        }
        else if (
#ifdef USE_GOPHER
            pu->scheme != SCM_GOPHER &&
#endif /* USE_GOPHER */
            pu->file[0] == '/')
        {
            /*
	     * this happens on the following conditions:
	     * (1) ftp scheme (2) local, looks like absolute path.
	     * In both case, there must be no side effect with
	     * cleanupName(). (I hope so...)
	     */
            pu->file = cleanupName(pu->file);
        }
        if (pu->scheme == SCM_LOCAL)
        {
#ifdef SUPPORT_NETBIOS_SHARE
            if (pu->host && strcmp(pu->host, "localhost") != 0)
            {
                Str tmp = Strnew_charp("//");
                Strcat_m_charp(tmp, pu->host,
                               cleanupName(file_unquote(pu->file)), NULL);
                pu->real_file = tmp->ptr;
            }
            else
#endif
                pu->real_file = cleanupName(file_unquote(pu->file));
        }
    }
}

Str parsedURL2Str(ParsedURL *pu, bool pass)
{
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

    if (pu->scheme == SCM_MISSING)
    {
        return Strnew_charp("???");
    }
    else if (pu->scheme == SCM_UNKNOWN)
    {
        return Strnew_charp(pu->file);
    }
    if (pu->host == NULL && pu->file == NULL && pu->label != NULL)
    {
        /* local label */
        return Sprintf("#%s", pu->label);
    }
    if (pu->scheme == SCM_LOCAL && !strcmp(pu->file, "-"))
    {
        tmp = Strnew_charp("-");
        if (pu->label)
        {
            tmp->Push('#');
            tmp->Push(pu->label);
        }
        return tmp;
    }
    tmp = Strnew_charp(scheme_str[pu->scheme]);
    tmp->Push(':');
#ifndef USE_W3MMAILER
    if (pu->scheme == SCM_MAILTO)
    {
        tmp->Push(pu->file);
        if (pu->query)
        {
            tmp->Push('?');
            tmp->Push(pu->query);
        }
        return tmp;
    }
#endif
    if (pu->scheme == SCM_DATA)
    {
        tmp->Push(pu->file);
        return tmp;
    }
#ifdef USE_NNTP
    if (pu->scheme != SCM_NEWS && pu->scheme != SCM_NEWS_GROUP)
#endif /* USE_NNTP */
    {
        tmp->Push("//");
    }
    if (pu->user)
    {
        tmp->Push(pu->user);
        if (pass && pu->pass)
        {
            tmp->Push(':');
            tmp->Push(pu->pass);
        }
        tmp->Push('@');
    }
    if (pu->host)
    {
        tmp->Push(pu->host);
        if (pu->port != DefaultPort[pu->scheme])
        {
            tmp->Push(':');
            tmp->Push(Sprintf("%d", pu->port));
        }
    }
    if (
#ifdef USE_NNTP
        pu->scheme != SCM_NEWS && pu->scheme != SCM_NEWS_GROUP &&
#endif /* USE_NNTP */
        (pu->file == NULL || (pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
                              && !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':' && pu->host == NULL)
#endif
                                  )))
        tmp->Push('/');
    tmp->Push(pu->file);
    if (pu->scheme == SCM_FTPDIR && tmp->Back() != '/')
        tmp->Push('/');
    if (pu->query)
    {
        tmp->Push('?');
        tmp->Push(pu->query);
    }
    if (pu->label)
    {
        tmp->Push('#');
        tmp->Push(pu->label);
    }
    return tmp;
}

char *
otherinfo(ParsedURL *target, ParsedURL *current, char *referer)
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

    if (target->host)
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
#ifdef USE_SSL
        if (current && current->scheme == SCM_HTTPS && target->scheme != SCM_HTTPS)
        {
            /* Don't send Referer: if https:// -> http:// */
        }
        else
#endif
            if (referer == NULL && current && current->scheme != SCM_LOCAL &&
                (current->scheme != SCM_FTP ||
                 (current->user == NULL && current->pass == NULL)))
        {
            char *p = current->label;
            s->Push("Referer: ");
            current->label = NULL;
            s->Push(parsedURL2Str(current));
            current->label = p;
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

URLFile
openURL(char *url, ParsedURL *pu, ParsedURL *current,
        URLOption *option, FormList *request, TextList *extra_header,
        HRequest *hr, unsigned char *status)
{
    Str tmp;
    int sock, scheme;
    char *p, *q, *u;
    HRequest hr0;
    SSL *sslh = NULL;

    URLFile uf;
    init_stream(&uf, SCM_MISSING, NULL);

    u = url;
    scheme = getURLScheme(&u);
    if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL)
        u = file_to_url(url); /* force to local file */
    else
        u = url;
retry:
    parseURL2(u, pu, current);
    if (pu->scheme == SCM_LOCAL && pu->file == NULL)
    {
        if (pu->label != NULL)
        {
            /* #hogege is not a label but a filename */
            Str tmp2 = Strnew_charp("#");
            tmp2->Push(pu->label);
            pu->file = tmp2->ptr;
            pu->real_file = cleanupName(file_unquote(pu->file));
            pu->label = NULL;
        }
        else
        {
            /* given URL must be null string */
#ifdef SOCK_DEBUG
            sock_log("given URL must be null string\n");
#endif
            return uf;
        }
    }

    uf.scheme = pu->scheme;
    uf.url = parsedURL2Str(pu)->ptr;
    pu->is_nocache = (option->flag & RG_NOCACHE);
    uf.ext = filename_extension(pu->file, 1);

    hr->command = HR_COMMAND_GET;
    hr->flag = 0;
    hr->referer = option->referer;
    hr->request = request;

    switch (pu->scheme)
    {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
        if (request && request->body)
            /* local CGI: POST */
            uf.stream = newFileStream(localcgi_post(pu->real_file, pu->query,
                                                    request, option->referer),
                                      (FileStreamCloseFunc)fclose);
        else
            /* lodal CGI: GET */
            uf.stream = newFileStream(localcgi_get(pu->real_file, pu->query,
                                                   option->referer),
                                      (FileStreamCloseFunc)fclose);
        if (uf.stream)
        {
            uf.is_cgi = TRUE;
            uf.scheme = pu->scheme = SCM_LOCAL_CGI;
            return uf;
        }
        examineFile(pu->real_file, &uf);
        if (uf.stream == NULL)
        {
            if (dir_exist(pu->real_file))
            {
                add_index_file(pu, &uf);
                if (uf.stream == NULL)
                    return uf;
            }
            else if (document_root != NULL)
            {
                tmp = Strnew_charp(document_root);
                if (tmp->Back() != '/' && pu->file[0] != '/')
                    tmp->Push('/');
                tmp->Push(pu->file);
                p = cleanupName(tmp->ptr);
                q = cleanupName(file_unquote(p));
                if (dir_exist(q))
                {
                    pu->file = p;
                    pu->real_file = q;
                    add_index_file(pu, &uf);
                    if (uf.stream == NULL)
                    {
                        return uf;
                    }
                }
                else
                {
                    examineFile(q, &uf);
                    if (uf.stream)
                    {
                        pu->file = p;
                        pu->real_file = q;
                    }
                }
            }
        }
        if (uf.stream == NULL && retryAsHttp && url[0] != '/')
        {
            if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN)
            {
                /* retry it as "http://" */
                u = Strnew_m_charp("http://", url, NULL)->ptr;
                goto retry;
            }
        }
        return uf;
    case SCM_FTP:
    case SCM_FTPDIR:
        if (pu->file == NULL)
            pu->file = allocStr("/", -1);
        if (non_null(FTP_proxy) &&
            !Do_not_use_proxy &&
            pu->host != NULL && !check_no_proxy(pu->host))
        {
            hr->flag |= HR_FLAG_PROXY;
            sock = openSocket(FTP_proxy_parsed.host,
                              schemetable[FTP_proxy_parsed.scheme].name.data(),
                              FTP_proxy_parsed.port);
            if (sock < 0)
                return uf;
            uf.scheme = SCM_HTTP;
            tmp = HTTPrequest(pu, current, hr, extra_header);
            write(sock, tmp->ptr, tmp->Size());
        }
        else
        {
            uf.stream = openFTPStream(pu, &uf);
            uf.scheme = pu->scheme;
            return uf;
        }
        break;
    case SCM_HTTP:

    case SCM_HTTPS:

        if (pu->file == NULL)
            pu->file = allocStr("/", -1);
        if (request && request->method == FORM_METHOD_POST && request->body)
            hr->command = HR_COMMAND_POST;
        if (request && request->method == FORM_METHOD_HEAD)
            hr->command = HR_COMMAND_HEAD;
        if ((

                (pu->scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) :

                                          non_null(HTTP_proxy)) &&
            !Do_not_use_proxy &&
            pu->host != NULL && !check_no_proxy(pu->host))
        {
            hr->flag |= HR_FLAG_PROXY;
            if (pu->scheme == SCM_HTTPS && *status == HTST_CONNECT)
            {
                // https proxy の時に通る？
                assert(false);
                return uf;
                // sock = ssl_socket_of(ouf->stream);
                // if (!(sslh = openSSLHandle(sock, pu->host,
                //                            &uf.ssl_certificate)))
                // {
                //     *status = HTST_MISSING;
                //     return uf;
                // }
            }
            else if (pu->scheme == SCM_HTTPS)
            {
                sock = openSocket(HTTPS_proxy_parsed.host,
                                  schemetable[HTTPS_proxy_parsed.scheme].name.data(), HTTPS_proxy_parsed.port);
                sslh = NULL;
            }
            else
            {
                sock = openSocket(HTTP_proxy_parsed.host,
                                  schemetable[HTTP_proxy_parsed.scheme].name.data(), HTTP_proxy_parsed.port);
                sslh = NULL;
            }

            if (sock < 0)
            {
#ifdef SOCK_DEBUG
                sock_log("Can't open socket\n");
#endif
                return uf;
            }
            if (pu->scheme == SCM_HTTPS)
            {
                if (*status == HTST_NORMAL)
                {
                    hr->command = HR_COMMAND_CONNECT;
                    tmp = HTTPrequest(pu, current, hr, extra_header);
                    *status = HTST_CONNECT;
                }
                else
                {
                    hr->flag |= HR_FLAG_LOCAL;
                    tmp = HTTPrequest(pu, current, hr, extra_header);
                    *status = HTST_NORMAL;
                }
            }
            else
            {
                tmp = HTTPrequest(pu, current, hr, extra_header);
                *status = HTST_NORMAL;
            }
        }
        else
        {
            sock = openSocket(pu->host,
                              schemetable[pu->scheme].name.data(), pu->port);
            if (sock < 0)
            {
                *status = HTST_MISSING;
                return uf;
            }
            if (pu->scheme == SCM_HTTPS)
            {
                if (!(sslh = openSSLHandle(sock, pu->host,
                                           &uf.ssl_certificate)))
                {
                    *status = HTST_MISSING;
                    return uf;
                }
            }
            hr->flag |= HR_FLAG_LOCAL;
            tmp = HTTPrequest(pu, current, hr, extra_header);
            *status = HTST_NORMAL;
        }
        if (pu->scheme == SCM_HTTPS)
        {
            uf.stream = newSSLStream(sslh, sock);
            if (sslh)
                SSL_write(sslh, tmp->ptr, tmp->Size());
            else
                write(sock, tmp->ptr, tmp->Size());
            if (w3m_reqlog)
            {
                FILE *ff = fopen(w3m_reqlog, "a");
                if (sslh)
                    fputs("HTTPS: request via SSL\n", ff);
                else
                    fputs("HTTPS: request without SSL\n", ff);
                fwrite(tmp->ptr, sizeof(char), tmp->Size(), ff);
                fclose(ff);
            }
            if (hr->command == HR_COMMAND_POST &&
                request->enctype == FORM_ENCTYPE_MULTIPART)
            {
                if (sslh)
                    SSL_write_from_file(sslh, request->body);
                else
                    write_from_file(sock, request->body);
            }
            return uf;
        }
        else
        {
            write(sock, tmp->ptr, tmp->Size());
            if (w3m_reqlog)
            {
                FILE *ff = fopen(w3m_reqlog, "a");
                fwrite(tmp->ptr, sizeof(char), tmp->Size(), ff);
                fclose(ff);
            }
            if (hr->command == HR_COMMAND_POST &&
                request->enctype == FORM_ENCTYPE_MULTIPART)
                write_from_file(sock, request->body);
        }
        break;
    case SCM_NNTP:
    case SCM_NNTP_GROUP:
    case SCM_NEWS:
    case SCM_NEWS_GROUP:
        if (pu->scheme == SCM_NNTP || pu->scheme == SCM_NEWS)
            uf.scheme = SCM_NEWS;
        else
            uf.scheme = SCM_NEWS_GROUP;
        uf.stream = openNewsStream(pu);
        return uf;
    case SCM_DATA:
        if (pu->file == NULL)
            return uf;
        p = Strnew_charp(pu->file)->ptr;
        q = strchr(p, ',');
        if (q == NULL)
            return uf;
        *q++ = '\0';
        tmp = Strnew_charp(q);
        q = strrchr(p, ';');
        if (q != NULL && !strcmp(q, ";base64"))
        {
            *q = '\0';
            uf.encoding = ENC_BASE64;
        }
        else
            tmp = tmp->UrlDecode(FALSE, FALSE);
        uf.stream = newStrStream(tmp);
        uf.guess_type = (*p != '\0') ? p : (char *)"text/plain";
        return uf;
    case SCM_UNKNOWN:
    default:
        return uf;
    }
    uf.stream = newInputStream(sock);
    return uf;
}

/* add index_file if exists */
static void
add_index_file(ParsedURL *pu, URLFile *uf)
{
    char *p, *q;
    TextList *index_file_list = NULL;
    TextListItem *ti;

    if (non_null(index_file))
        index_file_list = make_domain_list(index_file);
    if (index_file_list == NULL)
    {
        uf->stream = NULL;
        return;
    }
    for (ti = index_file_list->first; ti; ti = ti->next)
    {
        p = Strnew_m_charp(pu->file, "/", file_quote(ti->ptr), NULL)->ptr;
        p = cleanupName(p);
        q = cleanupName(file_unquote(p));
        examineFile(q, uf);
        if (uf->stream != NULL)
        {
            pu->file = p;
            pu->real_file = q;
            return;
        }
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
        um[i].item2 = Strnew_charp(p)->ptr;
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
    url = parsedURL2Str(pu);
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
