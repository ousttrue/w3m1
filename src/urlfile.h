#pragma once
#include "url.h"
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

struct URLOption
{
    char *referer;
    int flag;
};

union InputStream;
struct FormList;
struct TextList;
struct HRequest;
struct URLFile
{
    SchemaTypes scheme = SCM_MISSING;
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

    URLFile(SchemaTypes scheme, InputStream *stream);
    ~URLFile();
    void Close();
    void HalfClose();
    int FileNo() const;
    int Read(Str buf, int len);
    int Getc();
    int UndoGetc();
    void openURL(char *url, ParsedURL *pu, const ParsedURL *current,
                 URLOption *option, FormList *request, TextList *extra_header,
                 HRequest *hr, unsigned char *status);
};
