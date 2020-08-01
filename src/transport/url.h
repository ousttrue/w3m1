#pragma once
#include <Str.h>
#include <string>
#include <string_view>
#include "ces.h"

enum SchemaTypes
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

struct ParsedURL
{
    SchemaTypes scheme = SCM_MISSING;
    std::string user;
    std::string pass;
    std::string host;
    int port = 0;
    std::string file;
    std::string real_file;
    std::string query;
    std::string label;
    int is_nocache = 0;

    void Parse(std::string_view url, const ParsedURL *current);
    void Parse2(std::string_view url, const ParsedURL *current);
    operator bool() const
    {
        return scheme != SCM_MISSING;
    }
    Str ToStr(bool usePass = false, bool useLabel = true) const;
};

const char *filename_extension(const char *patch, int is_url);

ParsedURL *schemeToProxy(int scheme);
char *otherinfo(const ParsedURL *target, const ParsedURL *current, char *referer);
SchemaTypes getURLScheme(char **url);
char *mybasename(std::string_view s);
char *url_unquote_conv(std::string_view url, CharacterEncodingScheme charset);

struct SchemeKeyValue
{
    std::string_view name;
    SchemaTypes schema;
};
SchemeKeyValue &GetScheme(int index);
