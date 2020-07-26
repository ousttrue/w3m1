#pragma once
#include <Str.h>

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
    SchemaTypes scheme;
    char *user;
    char *pass;
    char *host;
    int port;
    char *file;
    char *real_file;
    char *query;
    char *label;
    int is_nocache;
};

const char *filename_extension(const char *patch, int is_url);
void initURIMethods();
Str searchURIMethods(ParsedURL *pu);
ParsedURL *schemeToProxy(int scheme);
Str parsedURL2Str(ParsedURL *pu, bool pass = false);
char *otherinfo(ParsedURL *target, ParsedURL *current, char *referer);
SchemaTypes getURLScheme(char **url);
void parseURL2(const char *url, ParsedURL *pu, ParsedURL *current);
inline void parseURL2(const std::string &url, ParsedURL *pu, ParsedURL *current)
{
    parseURL2(url.c_str(), pu, current);
}

struct SchemeKeyValue
{
    std::string_view name;
    SchemaTypes schema;
};
SchemeKeyValue &GetScheme(int index);
