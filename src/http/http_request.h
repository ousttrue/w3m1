#pragma once

enum HttpMethod
{
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST = 1,
    HTTP_METHOD_CONNECT = 2,
    HTTP_METHOD_HEAD = 3,
};

enum class HttpReferrerPolicy
{
    NoReferer, // not send
    StrictOriginWhenCrossOrigin, // send if same origin, send origin if not same origin
};

struct URL;
struct FormList;
struct TextList;
struct HttpRequest
{
    HttpMethod method = HTTP_METHOD_GET;

public:
    HttpReferrerPolicy referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin;
    FormList *request = nullptr;

    HttpRequest(HttpReferrerPolicy _referer, FormList *_request)
        : referer(_referer), request(_request)
    {
    }

    Str Method() const;
    Str URI(const URL &url, bool isLocal = false) const;
    Str ToStr(const URL &url, const URL *current, const TextList *extra) const;
};
