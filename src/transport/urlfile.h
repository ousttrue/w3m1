#pragma once
#include "transport/url.h"
#include "http/http_request.h"
#include <time.h>

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

enum LoadFlags
{
    RG_NONE = 0,
    RG_NOCACHE = 1,
    RG_FRAME = 2,
    RG_FRAME_SRC = 4,
};

union InputStream;
struct FormList;
struct TextList;
struct HttpRequest;
struct URLOption;
struct URLFile
{
    URLSchemeTypes scheme = SCM_MISSING;
    char is_cgi = 0;
    EncodingTypes encoding = ENC_7BIT;
    InputStream *stream = nullptr;
    const char *ext = nullptr;
    CompressionTypes compression = CMP_NOCOMPRESS;
    CompressionTypes content_encoding = CMP_NOCOMPRESS;
    const char *guess_type = nullptr;
    char *ssl_certificate = nullptr;
    char *url = nullptr;
    time_t modtime = -1;

    URLFile()
        : scheme(SCM_MISSING), stream(nullptr)
    {
    }
    URLFile(URLFile &&rhs) noexcept
    {
        scheme = rhs.scheme;
        ssl_certificate = rhs.ssl_certificate;
        // not close
        stream = rhs.stream;
        rhs.stream = nullptr;
    }
    URLFile &operator=(URLFile &&rhs) noexcept
    {
        if (this != &rhs)
        {
            scheme = rhs.scheme;
            ssl_certificate = rhs.ssl_certificate;
            // not close
            stream = rhs.stream;
            rhs.stream = nullptr;
        }
        return *this;
    }

    URLFile(URLSchemeTypes scheme, InputStream *stream);
    ~URLFile();
    void Close();

    static URLFile OpenHttp(const URL &url, const URL *current,
                            HttpReferrerPolicy referer, LoadFlags flag, FormList *request, TextList *extra_header,
                            HttpRequest *hr, unsigned char *status);

    void openURL(const URL &url, const URL *current,
                 HttpReferrerPolicy referer, LoadFlags flag, FormList *request, TextList *extra_header,
                 HttpRequest *hr, unsigned char *status);
    int DoFileSave(const char *defstr, long long content_length);

    // open stream to local path
    void examineFile(std::string_view path);
};

int save2tmp(const URLFile &uf, char *tmpf);
