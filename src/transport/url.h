#pragma once
#include <Str.h>
#include <string>
#include <string_view>
#include <wc.h>

enum URLSchemeTypes
{
    SCM_UNKNOWN = 255,
    SCM_MISSING = 254,
    SCM_HTTP = 0,
    SCM_GOPHER = 1,
    SCM_FTP = 2,
    SCM_FTPDIR = 3,
    SCM_LOCAL = 4,
    SCM_LOCAL_CGI = 5,
    SCM_EXEC = 6,
    SCM_NNTP = 7,
    SCM_NNTP_GROUP = 8,
    SCM_NEWS = 9,
    SCM_NEWS_GROUP = 10,
    SCM_DATA = 11,
    SCM_MAILTO = 12,
    SCM_HTTPS = 13,
};

struct URLScheme
{
    const std::string_view name;
    const URLSchemeTypes type;
    const int port;

    static std::tuple<std::string_view, URLSchemeTypes> Parse(std::string_view url);
};
const URLScheme *GetScheme(URLSchemeTypes index);

//
// scheme://userinfo@host:port/path?query#fragment
//
struct URL
{
    URLSchemeTypes scheme = SCM_MISSING;
    struct Userinfo
    {
        std::string name;
        std::string pass;

        bool empty() const
        {
            return name.empty() && pass.empty();
        }
    };
    Userinfo userinfo;
    std::string host;
    int port = 0;
    std::string path;
    std::string query;
    std::string fragment;

private:
    const char *ParseUserinfoHostPort(const char *p);
    const char *ParsePath(const char *p);
    const char *ParseQuery(const char *p);
    void ParseFragment(const char *p);

public:
    std::string real_file;
    int is_nocache = 0;

    static inline URL StdIn()
    {
        return {
            scheme : SCM_LOCAL,
            path : "-",
        };
    }
    bool IsStdin() const
    {
        return scheme == SCM_LOCAL && path == "-";
    }

    void Parse(std::string_view url, const URL *current);

    operator bool() const
    {
        return scheme != SCM_MISSING;
    }
    Str ToStr(bool usePass = false, bool useLabel = true) const;
};

const char *filename_extension(const char *patch, int is_url);

URL *schemeToProxy(int scheme);
std::tuple<const char *, URLSchemeTypes> getURLScheme(const char *url);
char *mybasename(std::string_view s);
char *url_unquote_conv(std::string_view url, CharacterEncodingScheme charset);
// char *url_quote(std::string_view str);
std::string url_quote(std::string_view p);