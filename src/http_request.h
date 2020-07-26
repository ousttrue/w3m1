#pragma once
#include "Str.h"

enum HttpMethod
{
    HR_COMMAND_GET = 0,
    HR_COMMAND_POST = 1,
    HR_COMMAND_CONNECT = 2,
    HR_COMMAND_HEAD = 3,
};

enum HttpRequestFlags
{
    HR_FLAG_LOCAL = 1,
    HR_FLAG_PROXY = 2,
};

struct ParsedURL;
struct FormList;
struct TextList;
struct HRequest
{
    char command;
    char flag;
    char *referer;
    FormList *request;

    Str Method() const;
    Str URI(ParsedURL *pu) const;
};

Str HTTPrequestURI(ParsedURL *pu, HRequest *hr);
Str HTTPrequest(ParsedURL *pu, const ParsedURL *current, HRequest *hr, TextList *extra);
