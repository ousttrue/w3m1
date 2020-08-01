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
    HR_FLAG_NONE = 0,
    HR_FLAG_LOCAL = 1,
    HR_FLAG_PROXY = 2,
};
#include "enum_bit_operator.h"

struct URL;
struct FormList;
struct TextList;
struct HRequest
{
    HttpMethod command = HR_COMMAND_GET;
    HttpRequestFlags flag = HR_FLAG_NONE;
    char *referer = nullptr;
    FormList *request = nullptr;

    HRequest(char *_referer, FormList *_request)
        : referer(_referer), request(_request)
    {
    }

    Str Method() const;
    Str URI(const URL &url) const;
    Str ToStr(const URL &url, const URL *current, const TextList *extra) const;
};
