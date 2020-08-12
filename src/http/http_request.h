#pragma once

enum HttpMethod
{
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST = 1,
    HTTP_METHOD_CONNECT = 2,
    HTTP_METHOD_HEAD = 3,
};

struct URL;
struct FormList;
struct TextList;
struct HttpRequest
{
    HttpMethod method = HTTP_METHOD_GET;

public:
    char *referer = nullptr;
    FormList *request = nullptr;

    HttpRequest(char *_referer, FormList *_request)
        : referer(_referer), request(_request)
    {
    }

    Str Method() const;
    Str URI(const URL &url, bool isLocal = false) const;
    Str ToStr(const URL &url, const URL *current, const TextList *extra) const;
};
