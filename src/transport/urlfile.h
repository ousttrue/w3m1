#pragma once
#include "transport/url.h"
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

    URLFile(URLSchemeTypes scheme, InputStream *stream);
    ~URLFile();
    void Close();
    void HalfClose();
    int FileNo() const;
    int Read(Str buf, int len);
    int Getc();
    int UndoGetc();
    void openURL(std::string_view url, URL *pu, const URL *current,
                 const char* referer, LoadFlags flag, FormList *request, TextList *extra_header,
                 HttpRequest *hr, unsigned char *status);
    int DoFileSave(const char *defstr, long long content_length);
    Str StrmyISgets();

    // open stream to local path
    void examineFile(std::string_view path);
};

char *file_to_url(std::string_view file);
int save2tmp(URLFile uf, char *tmpf);
