#pragma once
#include <string_view>
#include <assert.h>

enum HttpMethod
{
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_CONNECT,
    HTTP_METHOD_HEAD,
};

// https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Referrer-Policy
enum class HttpReferrerPolicy
{
    NoReferer,                   // not send
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

    std::string_view Method() const
    {
        switch (method)
        {
        case HTTP_METHOD_CONNECT:
            return "CONNECT";
        case HTTP_METHOD_POST:
            return "POST";
            break;
        case HTTP_METHOD_HEAD:
            return "HEAD";
            break;
        case HTTP_METHOD_GET:
            return "GET";
        }

        assert(false);
        return "";
    }

    Str URI(const URL &url, bool isLocal = false) const;
    Str ToStr(const URL &url, const URL *current, const TextList *extra) const;
};
