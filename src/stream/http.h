#pragma once
#include <string_view>
#include <memory>
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

enum class HttpResponseStatusCode
{
    NONE = 0,
    OK = 200,

    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    TEMPORARY_REDIRECT = 307,
};

struct HttpResponse
{
    int version_major = -1;
    int version_minor = -1;
    HttpResponseStatusCode status_code = HttpResponseStatusCode::NONE;
    std::vector<std::string> lines;

    static std::shared_ptr<HttpResponse> Read(const std::shared_ptr<class InputStream> &stream);

    bool PushIsEndHeader(std::string_view line);

    const bool HasRedirectionStatus() const
    {
        return (int)status_code >= 300 && (int)status_code < 400;
    }

    std::string_view FindHeader(std::string_view key) const;
};
