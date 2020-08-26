/*
 * References for version 0 cookie:                                  
 *   [NETACAPE] http://www.netscape.com/newsref/std/cookie_spec.html
 *
 * References for version 1 cookie:                                  
 *   [RFC 2109] http://www.ics.uci.edu/pub/ietf/http/rfc2109.txt
 *   [DRAFT 12] http://www.ics.uci.edu/pub/ietf/http/draft-ietf-http-state-man-mec-12.txt
 */

#include <sstream>
#include <list>
#include <string_view_util.h>
#include "stream/cookie.h"
#include "stream/network.h"
#include "indep.h"
#include "rc.h"
#include "commands.h"
#include "mytime.h"
#include "regex.h"
#include "myctype.h"
#include "frontend/buffer.h"
#include "file.h"
#include "frontend/display.h"
#include "frontend/terminal.h"
#include "html/parsetag.h"
#include "html/html_context.h"

#include <time.h>
#ifdef INET6
#include <sys/socket.h>
#endif /* INET6 */
#ifndef __MINGW32_VERSION
#include <netdb.h>
#else
#include <winsock.h>
#endif

#define COO_USE 1
#define COO_SECURE 2
#define COO_DOMAIN 4
#define COO_PATH 8
#define COO_DISCARD 16
#define COO_OVERRIDE 32 /* user chose to override security checks */

#define COO_OVERRIDE_OK 32                  /* flag to specify that an error is overridable */
                                            /* version 0 refers to the original cookie_spec.html */
                                            /* version 1 refers to RFC 2109 */
                                            /* version 1' refers to the Internet draft to obsolete RFC 2109 */
#define COO_EINTERNAL (1)                   /* unknown error; probably forgot to convert "return 1" in cookie.c */
#define COO_ETAIL (2 | COO_OVERRIDE_OK)     /* tail match failed (version 0) */
#define COO_ESPECIAL (3)                    /* special domain check failed (version 0) */
#define COO_EPATH (4)                       /* Path attribute mismatch (version 1 case 1) */
#define COO_ENODOT (5 | COO_OVERRIDE_OK)    /* no embedded dots in Domain (version 1 case 2.1) */
#define COO_ENOTV1DOM (6 | COO_OVERRIDE_OK) /* Domain does not start with a dot (version 1 case 2.2) */
#define COO_EDOM (7 | COO_OVERRIDE_OK)      /* domain-match failed (version 1 case 3) */
#define COO_EBADHOST (8 | COO_OVERRIDE_OK)  /* dot in matched host name in FQDN (version 1 case 4) */
#define COO_EPORT (9)                       /* Port match failed (version 1' case 5) */
#define COO_EMAX COO_EPORT

static int
total_dot_number(const char *p, const char *ep, int max_count)
{
    int count = 0;
    if (!ep)
        ep = p + strlen(p);

    for (; p < ep && count < max_count; p++)
    {
        if (*p == '.')
            count++;
    }
    return count;
}

#define contain_no_dots(p, ep) (total_dot_number((p), (ep), 1) == 0)

static std::string domain_match(const std::string &host, const std::string &domain)
{
    /* [RFC 2109] s. 2, "domain-match", case 1
     * (both are IP and identical)
     */
    regexCompile("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+", 0);
    auto m0 = regexMatch(const_cast<char *>(host.c_str()), -1, 1);
    auto m1 = regexMatch(const_cast<char *>(domain.c_str()), -1, 1);
    if (m0 && m1)
    {
        if (host == domain)
            return host;
    }
    else if (!m0 && !m1)
    {
        /*
        * "." match all domains (w3m only),
        * and ".local" match local domains ([DRAFT 12] s. 2)
        */
        if (domain == "." || domain == ".local")
        {
            auto offset = host.size();
            auto domain_p = &host[offset];
            if (domain[1] == '\0' || contain_no_dots(host.c_str(), domain_p))
                return domain_p;
        }
        /*
        * special case for domainName = .hostName
        * see nsCookieService.cpp in Firefox.
        */
        else if (domain[0] == '.' && host == &domain[1])
        {
            return host;
        }
        /* [RFC 2109] s. 2, cases 2, 3 */
        else
        {
            auto offset = (domain[0] != '.') ? 0 : host.size() - domain.size();
            auto domain_p = &host[offset];
            if (offset >= 0 && strcasecmp(domain_p, domain.c_str()) == 0)
                return domain_p;
        }
    }
    return "";
}

static bool
port_match(const std::vector<uint16_t> &l, int port)
{
    return std::find(l.begin(), l.end(), port) != l.end();
}

struct Cookie
{
    URL url;
    Str name = nullptr;
    Str value = nullptr;
    time_t expires = {};
    Str path = nullptr;
    Str domain = nullptr;
    Str comment = nullptr;
    Str commentURL = nullptr;
    std::vector<uint16_t> portl;
    char version = 0;
    char flag = 0;
    Cookie *next = nullptr;

    bool Match(const URL *pu, const std::string &domainname) const
    {
        if (domainname.empty())
        {
            return false;
        }

        if (domain_match(domainname, this->domain->ptr).empty())
            return false;
        if (strncmp(this->path->ptr, pu->path.c_str(), this->path->Size()) != 0)
            return false;
        if (this->flag & COO_SECURE && pu->scheme != SCM_HTTPS)
            return false;
        if (this->portl.size() && !port_match(this->portl, pu->port))
            return false;

        return true;
    }

    Str ToStr() const
    {
        Str tmp = name->Clone();
        tmp->Push('=');
        tmp->Push(value);
        return tmp;
    }
};
using CookiePtr = std::shared_ptr<Cookie>;
static std::list<CookiePtr> g_cookies;

static int is_saved = 1;

static std::vector<uint16_t> make_portlist(Str port)
{
    std::vector<uint16_t> l;
    Str tmp = Strnew();
    auto p = port->ptr;
    while (*p)
    {
        while (*p && !IS_DIGIT(*p))
            p++;
        tmp->Clear();
        while (*p && IS_DIGIT(*p))
            tmp->Push(*(p++));
        if (tmp->Size() == 0)
            break;

        l.push_back(atoi(tmp->ptr));
    }
    return l;
}

static std::string
portlist2str(const std::vector<uint16_t> &l)
{
    std::stringstream ss;
    auto tmp = Strnew();
    for (int i = 0; i < l.size(); ++i)
    {
        if (i)
        {
            ss << ", ";
        }
        ss << l[i];
    }
    return ss.str();
}

static void check_expired_cookies(void)
{
    if (g_cookies.empty())
        return;

    time_t now = time(NULL);
    for (auto &cookie : g_cookies)
    {
        if (cookie->expires != (time_t)-1 && cookie->expires < now)
        {
            if (!(cookie->flag & COO_DISCARD))
                is_saved = 0;
        }
    }
}

CookiePtr get_cookie_info(Str domain, Str path, Str name)
{
    for (auto &cookie : g_cookies)
    {
        if (cookie->domain->ICaseCmp(domain) == 0 &&
            cookie->path->Cmp(path) == 0 && cookie->name->ICaseCmp(name) == 0)
            return cookie;
    }
    return NULL;
}

Str find_cookie(const URL &url)
{
    auto fq_domainname = Network::Instance().FQDN(url.host);
    check_expired_cookies();
    CookiePtr fco;
    for (auto &cookie : g_cookies)
    {
        auto domainname = (cookie->version == 0) ? fq_domainname : url.host;
        if (cookie->flag & COO_USE && cookie->Match(&url, domainname))
        {
            fco = cookie;
        }
    }
    if (!fco)
        return NULL;

    // Cookie *p, *p1;
    auto tmp = Strnew();
    int version = 0;
    if (version > 0)
        tmp->Push(Sprintf("$Version=\"%d\"; ", version));

    tmp->Push(fco->ToStr());
    // for (p1 = fco->next; p1; p1 = p1->next)
    {
        tmp->Push("; ");
        tmp->Push(fco->ToStr());
        if (version > 0)
        {
            // if (p1->flag & COO_PATH)
            //     tmp->Push(Sprintf("; $Path=\"%s\"", p1->path->ptr));
            // if (p1->flag & COO_DOMAIN)
            //     tmp->Push(Sprintf("; $Domain=\"%s\"", p1->domain->ptr));
            // if (p1->portl.size())
            //     tmp->Push(Sprintf("; $Port=\"%s\"", portlist2str(p1->portl)));
        }
    }
    return tmp;
}

CookieManager::CookieManager()
{
}

CookieManager::~CookieManager()
{
    save_cookies();
}

CookieManager &CookieManager::Instance()
{
    static CookieManager cm;
    return cm;
}

const char *special_domain[] = {
    ".com", ".edu", ".gov", ".mil", ".net", ".org", ".int", NULL};

int CookieManager::check_avoid_wrong_number_of_dots_domain(Str domain)
{
    // TextListItem *tl;
    int avoid_wrong_number_of_dots_domain = false;

    for (auto &d : this->Cookie_avoid_wrong_number_of_dots_domains)
    {
        if (domain_match(domain->ptr, d).size())
        {
            avoid_wrong_number_of_dots_domain = true;
            break;
        }
    }

    if (avoid_wrong_number_of_dots_domain == true)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int CookieManager::add_cookie(const URL &pu, Str name, Str value,
                              time_t expires, Str domain, Str path,
                              int flag, Str comment, bool version2, Str port, Str commentURL)
{

    auto domainname = !version2 ? Network::Instance().FQDN(pu.host) : pu.host;
    Str odomain = domain, opath = path;
    std::vector<uint16_t> portlist;
    int use_security = !(flag & COO_OVERRIDE);

#define COOKIE_ERROR(err)                         \
    if (!((err)&COO_OVERRIDE_OK) || use_security) \
    return (err)

#ifdef DEBUG
    fprintf(stderr, "host: [%s, %s] %d\n", pu->host, pu->file, flag);
    fprintf(stderr, "cookie: [%s=%s]\n", name->ptr, value->ptr);
    fprintf(stderr, "expires: [%s]\n", asctime(gmtime(&expires)));
    if (domain)
        fprintf(stderr, "domain: [%s]\n", domain->ptr);
    if (path)
        fprintf(stderr, "path: [%s]\n", path->ptr);
    fprintf(stderr, "version: [%d]\n", version);
    if (port)
        fprintf(stderr, "port: [%s]\n", port->ptr);
#endif /* DEBUG */
    /* [RFC 2109] s. 4.3.2 case 2; but this (no request-host) shouldn't happen */
    if (domainname.empty())
        return COO_ENODOT;

    if (domain)
    {
        /* [DRAFT 12] s. 4.2.2 (does not apply in the case that
        * host name is the same as domain attribute for version 0
        * cookie)
        * I think that this rule has almost the same effect as the
        * tail match of [NETSCAPE].
        */
        if (domain->ptr[0] != '.' &&
            (version2 || domainname == domain->ptr))
            domain = Sprintf(".%s", domain->ptr);

        if (!version2)
        {
            /* [NETSCAPE] rule */
            int n = total_dot_number(domain->ptr,
                                     domain->ptr + domain->Size(),
                                     3);
            if (n < 2)
            {
                if (!check_avoid_wrong_number_of_dots_domain(domain))
                {
                    COOKIE_ERROR(COO_ESPECIAL);
                }
            }
            else if (n == 2)
            {
                char **sdomain;
                int ok = 0;
                for (sdomain = (char **)special_domain; !ok && *sdomain; sdomain++)
                {
                    int offset = domain->Size() - strlen(*sdomain);
                    if (offset >= 0 &&
                        strcasecmp(*sdomain, &domain->ptr[offset]) == 0)
                        ok = 1;
                }
                if (!ok && !check_avoid_wrong_number_of_dots_domain(domain))
                {
                    COOKIE_ERROR(COO_ESPECIAL);
                }
            }
        }
        else
        {
            /* [DRAFT 12] s. 4.3.2 case 2 */
            if (strcasecmp(domain->ptr, ".local") != 0 &&
                contain_no_dots(&domain->ptr[1], &domain->ptr[domain->Size()]))
                COOKIE_ERROR(COO_ENODOT);
        }

        /* [RFC 2109] s. 4.3.2 case 3 */
        auto dp = domain_match(domainname, domain->ptr);
        if (!dp.empty())
            COOKIE_ERROR(COO_EDOM);
        /* [RFC 2409] s. 4.3.2 case 4 */
        /* Invariant: dp contains matched domain */
        if (version2 && !contain_no_dots(domainname.c_str(), dp.c_str()))
            COOKIE_ERROR(COO_EBADHOST);
    }
    if (path)
    {
        /* [RFC 2109] s. 4.3.2 case 1 */
        if (version2 && strncmp(path->ptr, pu.path.c_str(), path->Size()) != 0)
            COOKIE_ERROR(COO_EPATH);
    }
    if (port)
    {
        /* [DRAFT 12] s. 4.3.2 case 5 */
        portlist = make_portlist(port);
        if (portlist.size() && !port_match(portlist, pu.port))
            COOKIE_ERROR(COO_EPORT);
    }

    if (!domain)
        domain = Strnew(domainname);
    if (!path)
    {
        path = Strnew(pu.path);
        while (path->Size() > 0 && path->Back() != '/')
            path->Pop(1);
        if (path->Back() == '/')
            path->Pop(1);
    }

    auto p = get_cookie_info(domain, path, name);
    if (!p)
    {
        g_cookies.push_back({});
        p = g_cookies.back();
        p->flag = 0;
        if (this->default_use_cookie)
            p->flag |= COO_USE;
        // p->next = First_cookie;
        // First_cookie = p;
    }

    p->url = pu;
    p->name = name;
    p->value = value;
    p->expires = expires;
    p->domain = domain;
    p->path = path;
    p->comment = comment;
    p->version = version2 ? 2 : 1;
    p->portl = portlist;
    p->commentURL = commentURL;

    if (flag & COO_SECURE)
        p->flag |= COO_SECURE;
    else
        p->flag &= ~COO_SECURE;
    if (odomain)
        p->flag |= COO_DOMAIN;
    else
        p->flag &= ~COO_DOMAIN;
    if (opath)
        p->flag |= COO_PATH;
    else
        p->flag &= ~COO_PATH;
    if (flag & COO_DISCARD || p->expires == (time_t)-1)
    {
        p->flag |= COO_DISCARD;
    }
    else
    {
        p->flag &= ~COO_DISCARD;
        is_saved = 0;
    }

    check_expired_cookies();
    return 0;
}

static CookiePtr nth_cookie(int n)
{
    int i;
    for (auto &cookie : g_cookies)
    {
        if (i++ == n)
            return cookie;
    }
    return NULL;
}

#define str2charp(str) ((str) ? (str)->ptr : "")

void CookieManager::save_cookies()
{
    if (g_cookies.empty() || is_saved || w3mApp::Instance().no_rc_dir)
        return;

    check_expired_cookies();

    auto cookie_file = rcFile(COOKIE_FILE);
    FILE *fp;
    if (!(fp = fopen(cookie_file, "w")))
        return;

    for (auto &cookie : g_cookies)
    {
        if (!(cookie->flag & COO_USE) || cookie->flag & COO_DISCARD)
            continue;
        fprintf(fp, "%s\t%s\t%s\t%ld\t%s\t%s\t%d\t%d\t%s\t%s\t%s\n",
                cookie->url.ToStr()->ptr,
                cookie->name->ptr, cookie->value->ptr, cookie->expires,
                cookie->domain->ptr, cookie->path->ptr, cookie->flag,
                cookie->version, str2charp(cookie->comment),
                portlist2str(cookie->portl).c_str(),
                str2charp(cookie->commentURL));
    }
    fclose(fp);
    chmod(cookie_file, S_IRUSR | S_IWUSR);
}

static Str
readcol(char **p)
{
    Str tmp = Strnew();
    while (**p && **p != '\n' && **p != '\r' && **p != '\t')
        tmp->Push(*((*p)++));
    if (**p == '\t')
        (*p)++;
    return tmp;
}

void load_cookies(void)
{
    FILE *fp;
    if (!(fp = fopen(rcFile(COOKIE_FILE), "r")))
        return;

    for (;;)
    {
        auto line = Strfgets(fp);

        if (line->Size() == 0)
            break;
        auto str = line->ptr;

        auto cookie = std::make_shared<Cookie>();
        g_cookies.push_back(cookie);
        cookie->flag = 0;
        cookie->version = 0;
        cookie->expires = (time_t)-1;
        cookie->comment = NULL;
        cookie->commentURL = NULL;
        cookie->url = URL::Parse(readcol(&str)->ptr, nullptr);
        if (!*str)
            return;
        cookie->name = readcol(&str);
        if (!*str)
            return;
        cookie->value = readcol(&str);
        if (!*str)
            return;
        cookie->expires = (time_t)atol(readcol(&str)->ptr);
        if (!*str)
            return;
        cookie->domain = readcol(&str);
        if (!*str)
            return;
        cookie->path = readcol(&str);
        if (!*str)
            return;
        cookie->flag = atoi(readcol(&str)->ptr);
        if (!*str)
            return;
        cookie->version = atoi(readcol(&str)->ptr);
        if (!*str)
            return;
        cookie->comment = readcol(&str);
        if (cookie->comment->Size() == 0)
            cookie->comment = NULL;
        if (!*str)
            return;
        cookie->portl = make_portlist(readcol(&str));
        if (!*str)
            return;
        cookie->commentURL = readcol(&str);
        if (cookie->commentURL->Size() == 0)
            cookie->commentURL = NULL;
    }

    fclose(fp);
}

void initCookie(void)
{
    load_cookies();
    check_expired_cookies();
}

BufferPtr CookieManager::cookie_list_panel()
{
    if (!use_cookie || g_cookies.empty())
        return NULL;

    /* FIXME: gettextize? */
    Str src = Strnew("<html><head><title>Cookies</title></head>"
                     "<body><center><b>Cookies</b></center>"
                     "<p><form method=internal action=cookie>");

    int i;
    char *tmp, tmp2[80];
    src->Push("<ol>");
    for (auto &p : g_cookies)
    {
        tmp = html_quote(p->url.ToStr()->ptr);
        if (p->expires != (time_t)-1)
        {
#ifdef HAVE_STRFTIME
            strftime(tmp2, 80, "%a, %d %b %Y %H:%M:%S GMT",
                     gmtime(&p->expires));
#else  /* not HAVE_STRFTIME */
            struct tm *gmt;
            static char *dow[] = {
                "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri ", "Sat "};
            static char *month[] = {
                "Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ",
                "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec "};
            gmt = gmtime(&p->expires);
            strcpy(tmp2, dow[gmt->tm_wday]);
            sprintf(&tmp2[4], "%02d ", gmt->tm_mday);
            strcpy(&tmp2[7], month[gmt->tm_mon]);
            if (gmt->tm_year < 1900)
                sprintf(&tmp2[11], "%04d %02d:%02d:%02d GMT",
                        (gmt->tm_year) + 1900, gmt->tm_hour, gmt->tm_min,
                        gmt->tm_sec);
            else
                sprintf(&tmp2[11], "%04d %02d:%02d:%02d GMT",
                        gmt->tm_year, gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
#endif /* not HAVE_STRFTIME */
        }
        else
            tmp2[0] = '\0';
        src->Push("<li>");
        src->Push("<h1><a href=\"");
        src->Push(tmp);
        src->Push("\">");
        src->Push(tmp);
        src->Push("</a></h1>");

        src->Push("<table cellpadding=0>");
        if (!(p->flag & COO_SECURE))
        {
            src->Push("<tr><td width=\"80\"><b>Cookie:</b></td><td>");
            src->Push(html_quote(p->ToStr()));
            src->Push("</td></tr>");
        }
        if (p->comment)
        {
            src->Push("<tr><td width=\"80\"><b>Comment:</b></td><td>");
            src->Push(html_quote(p->comment->ptr));
            src->Push("</td></tr>");
        }
        if (p->commentURL)
        {
            src->Push("<tr><td width=\"80\"><b>CommentURL:</b></td><td>");
            src->Push("<a href=\"");
            src->Push(html_quote(p->commentURL->ptr));
            src->Push("\">");
            src->Push(html_quote(p->commentURL->ptr));
            src->Push("</a>");
            src->Push("</td></tr>");
        }
        if (tmp2[0])
        {
            src->Push("<tr><td width=\"80\"><b>Expires:</b></td><td>");
            src->Push(tmp2);
            if (p->flag & COO_DISCARD)
                src->Push(" (Discard)");
            src->Push("</td></tr>");
        }
        src->Push("<tr><td width=\"80\"><b>Version:</b></td><td>");
        src->Push(Sprintf("%d", p->version)->ptr);
        src->Push("</td></tr><tr><td>");
        if (p->domain)
        {
            src->Push("<tr><td width=\"80\"><b>Domain:</b></td><td>");
            src->Push(html_quote(p->domain->ptr));
            src->Push("</td></tr>");
        }
        if (p->path)
        {
            src->Push("<tr><td width=\"80\"><b>Path:</b></td><td>");
            src->Push(html_quote(p->path->ptr));
            src->Push("</td></tr>");
        }
        if (p->portl.size())
        {
            src->Push("<tr><td width=\"80\"><b>Port:</b></td><td>");
            src->Push(html_quote(portlist2str(p->portl)));
            src->Push("</td></tr>");
        }
        src->Push("<tr><td width=\"80\"><b>Secure:</b></td><td>");
        src->Push((p->flag & COO_SECURE) ? (char *)"Yes" : (char *)"No");
        src->Push("</td></tr><tr><td>");

        src->Push(Sprintf("<tr><td width=\"80\"><b>Use:</b></td><td>"
                          "<input type=radio name=\"%d\" value=1%s>Yes"
                          "&nbsp;&nbsp;"
                          "<input type=radio name=\"%d\" value=0%s>No",
                          i, (p->flag & COO_USE) ? " checked" : "",
                          i, (!(p->flag & COO_USE)) ? " checked" : ""));
        src->Push("</td></tr><tr><td><input type=submit value=\"OK\"></table><p>");
    }
    src->Push("</ol></form></body></html>");
    return loadHTMLStream(URL::Parse("w3m://cookie"), StrStream::Create(src->ptr), WC_CES_UTF_8, true);
}

void set_cookie_flag(tcb::span<parsed_tagarg> _arg)
{
    for (auto &arg : _arg)
    {
        auto n = atoi(arg.arg);
        auto v = atoi(arg.value);
        if (auto p = nth_cookie(n))
        {
            if (v && !(p->flag & COO_USE))
                p->flag |= COO_USE;
            else if (!v && p->flag & COO_USE)
                p->flag &= ~COO_USE;
            if (!(p->flag & COO_DISCARD))
                is_saved = 0;
        }
    }
    backBf(&w3mApp::Instance(), {});
}

bool CookieManager::check_cookie_accept_domain(const std::string &domain)
{
    if (domain.empty())
    {
        return false;
    }

    for (auto &d : this->Cookie_accept_domains)
    {
        if (domain_match(domain, d).size())
        {
            return true;
        }
    }

    for (auto &d : this->Cookie_reject_domains)
    {
        if (domain_match(domain, d).size())
        {
            return false;
        }
    }

    return true;
}

/* This array should be somewhere else */
/* FIXME: gettextize? */
const char *violations[COO_EMAX] = {
    "internal error",
    "tail match failed",
    "wrong number of dots",
    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1",
    "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",
    "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"};

void CookieManager::ProcessHttpHeader(const URL &url, bool version2, std::string_view line)
{
    if (!use_cookie)
    {
        return;
    }
    if (!accept_cookie)
    {
        return;
    }
    if (!check_cookie_accept_domain(url.host))
    {
        return;
    }

    auto p = line.data();
    auto name = Strnew();
    while (*p != '=' && !IS_ENDT(*p))
        name->Push(*(p++));
    StripRight(name);

    auto value = Strnew();
    if (*p == '=')
    {
        p++;
        SKIP_BLANKS(&p);
        int quoted = 0;
        const char *q = nullptr;
        while (!IS_ENDL(*p) && (quoted || *p != ';'))
        {
            if (!IS_SPACE(*p))
                q = p;
            if (*p == '"')
                quoted = (quoted) ? 0 : 1;
            value->Push(*(p++));
        }
        if (q)
            value->Pop(p - q - 1);
    }

    Str domain = NULL;
    Str path = NULL;
    int flag = 0;
    Str comment = NULL;
    Str commentURL = NULL;
    Str port = NULL;
    time_t expires = (time_t)-1;
    while (*p == ';')
    {
        p++;
        SKIP_BLANKS(&p);
        Str tmp2;
        if (matchattr(p, "expires", 7, &tmp2))
        {
            /* version 0 */
            expires = mymktime(tmp2->ptr);
        }
        else if (matchattr(p, "max-age", 7, &tmp2))
        {
            /* XXX Is there any problem with max-age=0? (RFC 2109 ss. 4.2.1, 4.2.2 */
            expires = time(NULL) + atol(tmp2->ptr);
        }
        else if (matchattr(p, "domain", 6, &tmp2))
        {
            domain = tmp2;
        }
        else if (matchattr(p, "path", 4, &tmp2))
        {
            path = tmp2;
        }
        else if (matchattr(p, "secure", 6, NULL))
        {
            flag |= COO_SECURE;
        }
        else if (matchattr(p, "comment", 7, &tmp2))
        {
            comment = tmp2;
        }
        else if (matchattr(p, "version", 7, &tmp2))
        {
            assert(false);
            // version = (CookieVersions)atoi(tmp2->ptr);
        }
        else if (matchattr(p, "port", 4, &tmp2))
        {
            /* version 1, Set-Cookie2 */
            port = tmp2;
        }
        else if (matchattr(p, "commentURL", 10, &tmp2))
        {
            /* version 1, Set-Cookie2 */
            commentURL = tmp2;
        }
        else if (matchattr(p, "discard", 7, NULL))
        {
            /* version 1, Set-Cookie2 */
            flag |= COO_DISCARD;
        }

        int quoted = 0;
        while (!IS_ENDL(*p) && (quoted || *p != ';'))
        {
            if (*p == '"')
                quoted = (quoted) ? 0 : 1;
            p++;
        }
    }

    if (name->Size() > 0)
    {
        int err;
        if (this->show_cookie)
        {
            if (flag & COO_SECURE)
                disp_message_nsec("Received a secured cookie", false, 1,
                                  true, false);
            else
                disp_message_nsec(Sprintf("Received cookie: %s=%s",
                                          name->ptr, value->ptr)
                                      ->ptr,
                                  false, 1, true, false);
        }
        err =
            add_cookie(url, name, value, expires, domain, path, flag,
                       comment, version2, port, commentURL);
        if (err)
        {
            const char *ans = (this->accept_bad_cookie == ACCEPT_BAD_COOKIE_ACCEPT)
                                  ? "y"
                                  : NULL;
            if ((err & COO_OVERRIDE_OK) &&
                this->accept_bad_cookie == ACCEPT_BAD_COOKIE_ASK)
            {
                Str msg = Sprintf("Accept bad cookie from %s for %s?",
                                  url.host,
                                  ((domain && domain->ptr)
                                       ? domain->ptr
                                       : "<localdomain>"));
                if (msg->Size() > ::Terminal::columns() - 10)
                    msg->Pop(msg->Size() - (::Terminal::columns() - 10));
                msg->Push(" (y/n)");
                ans = inputAnswer(msg->ptr);
            }
            if (ans == NULL || TOLOWER(*ans) != 'y' ||
                (err =
                     add_cookie(url, name, value, expires, domain, path,
                                flag | COO_OVERRIDE, comment, version2,
                                port, commentURL)))
            {
                err = (err & ~COO_OVERRIDE_OK) - 1;
                const char *emsg;
                if (err >= 0 && err < COO_EMAX)
                    emsg = Sprintf("This cookie was rejected "
                                   "to prevent security violation. [%s]",
                                   violations[err])
                               ->ptr;
                else
                    emsg =
                        "This cookie was rejected to prevent security violation.";
                record_err_message(emsg);
                if (this->show_cookie)
                    disp_message_nsec(emsg, false, 1, true, false);
            }
            else if (this->show_cookie)
                disp_message_nsec(Sprintf("Accepting invalid cookie: %s=%s",
                                          name->ptr, value->ptr)
                                      ->ptr,
                                  false,
                                  1, true, false);
        }
    }
}

std::vector<std::string> _make_domain_list(std::string_view src)
{
    auto splitter = svu::splitter(src, [](char c) -> bool {
        return IS_SPACE(c) || c == ',';
    });
    std::vector<std::string> list;
    for (auto s : splitter)
    {
        list.push_back(std::string(s));
    }
    return list;
}

void CookieManager::Initialize()
{
    this->Cookie_reject_domains = _make_domain_list(this->cookie_reject_domains);
    this->Cookie_accept_domains = _make_domain_list(this->cookie_accept_domains);
    this->Cookie_avoid_wrong_number_of_dots_domains = _make_domain_list(this->cookie_avoid_wrong_number_of_dots);
}
