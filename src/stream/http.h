#pragma once
#include <string_view>
#include <memory>
#include <assert.h>
#include "url.h"

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
    URL url;
    std::vector<std::string> lines;

public:
    static std::shared_ptr<HttpRequest> Create(const URL &url, struct FormList *form);

    HttpReferrerPolicy referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin;
    FormList *form = nullptr;

    HttpRequest() = default;

    HttpRequest(HttpReferrerPolicy _referer, FormList *_request)
        : referer(_referer), form(_request)
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

};
using HttpRequestPtr = std::shared_ptr<HttpRequest>;

enum class HttpResponseStatusCode
{
    NONE = 0,
    OK = 200,

    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    TEMPORARY_REDIRECT = 307,
};

enum EncodingTypes : char
{
    ENC_7BIT = 0,
    ENC_BASE64 = 1,
    ENC_QUOTE = 2,
    ENC_UUENCODE = 3,
};

enum CompressionTypes
{
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
};

struct HttpResponse
{
    int version_major = -1;
    int version_minor = -1;
    HttpResponseStatusCode status_code = HttpResponseStatusCode::NONE;
    std::vector<std::string> lines;
    CompressionTypes content_encoding = CMP_NOCOMPRESS;
    std::string conten_type;
    CharacterEncodingScheme content_charset = WC_CES_US_ASCII;

    static std::shared_ptr<HttpResponse> Read(const std::shared_ptr<class InputStream> &stream);

    bool PushIsEndHeader(std::string_view line);

    const bool HasRedirectionStatus() const
    {
        return (int)status_code >= 300 && (int)status_code < 400;
    }

    std::string_view FindHeader(std::string_view key) const;
};
using HttpResponsePtr = std::shared_ptr<HttpResponse>;

struct HttpExchange
{
    HttpRequestPtr request;
    HttpResponsePtr response;
};

class HttpClient
{
    // Usually there is a single exchange, but when redirected, multiple exchanges occur
    std::vector<HttpExchange> exchanges;

public:
    std::shared_ptr<struct Buffer> Request(const URL &url, const URL *base, HttpReferrerPolicy referer, struct FormList *form);
};
