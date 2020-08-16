#pragma once
#include "stream/http.h"
#include <time.h>
#include <memory>

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
    // CompressionTypes content_encoding = CMP_NOCOMPRESS;
    std::string guess_type;
    char *ssl_certificate = nullptr;
    char *url = nullptr;
    time_t modtime = -1;

private:
    URLFile(URLSchemeTypes scheme, InputStreamPtr stream);
    URLFile(const URLFile &) = delete;
    URLFile &operator=(const URLFile &) = delete;

public:
    ~URLFile();

    static InputStreamPtr OpenHttpAndSendRest(const std::shared_ptr<HttpRequest> &request);

    static std::shared_ptr<URLFile> FromStream(URLSchemeTypes scheme, InputStreamPtr stream);

    static std::shared_ptr<URLFile> OpenFile(std::string_view path);

    int DoFileSave(const char *defstr, long long content_length);

    // open stream to local path
    void examineFile(std::string_view path);
};
using URLFilePtr = std::shared_ptr<URLFile>;

int save2tmp(const URLFilePtr &uf, char *tmpf);
int dir_exist(const char *path);
