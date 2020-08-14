#pragma once
#include "stream/url.h"
#include "stream/http.h"
#include <time.h>
#include <memory>

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

class InputStream;
using InputStreamPtr = std::shared_ptr<InputStream>;
struct FormList;
struct TextList;
struct HttpRequest;
struct URLOption;
struct URLFile : std::enable_shared_from_this<URLFile>
{
    URLSchemeTypes scheme = SCM_MISSING;
    char is_cgi = 0;
    EncodingTypes encoding = ENC_7BIT;
    InputStreamPtr stream = nullptr;
    const char *ext = nullptr;
    CompressionTypes compression = CMP_NOCOMPRESS;
    CompressionTypes content_encoding = CMP_NOCOMPRESS;
    const char *guess_type = nullptr;
    char *ssl_certificate = nullptr;
    char *url = nullptr;
    time_t modtime = -1;

private:
    URLFile(URLSchemeTypes scheme, InputStreamPtr stream);
    URLFile(const URLFile &) = delete;
    URLFile &operator=(const URLFile &) = delete;

public:
    ~URLFile();

    static std::shared_ptr<URLFile> OpenHttpAndSendRest(const std::shared_ptr<HttpRequest> &request);

    static std::shared_ptr<URLFile> OpenHttp(const URL &url, const URL *current,
                                             HttpReferrerPolicy referer, FormList *form,
                                             HttpRequest *hr);

    static std::shared_ptr<URLFile> OpenStream(URLSchemeTypes scheme, InputStreamPtr stream);

    static std::shared_ptr<URLFile> OpenFile(std::string_view path);

    int DoFileSave(const char *defstr, long long content_length);

    // open stream to local path
    void examineFile(std::string_view path);
};
using URLFilePtr = std::shared_ptr<URLFile>;

int save2tmp(const URLFilePtr &uf, char *tmpf);
int dir_exist(const char *path);
