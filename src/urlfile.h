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

union InputStream;
struct URLFile
{
    SchemaTypes scheme;
    char is_cgi;
    EncodingTypes encoding;
    InputStream *stream;
    const char *ext;
    CompressionTypes compression;
    CompressionTypes content_encoding;
    const char *guess_type;
    char *ssl_certificate;
    char *url;
    time_t modtime;
};

void init_stream(URLFile *uf, SchemaTypes scheme, InputStream *stream);
