#pragma once
#include "config.h"
#include "transport/urlfile.h"

/* *INDENT-OFF* */
static struct compression_decoder
{
    CompressionTypes type;
    const char *ext;
    const char *mime_type;
    int auxbin_p;
    const char *cmd;
    const char *name;
    const char *encoding;
    const char *encodings[4];
} compression_decoders[] = {
    {CMP_COMPRESS, ".gz", "application/x-gzip", 0, GUNZIP_CMDNAME, GUNZIP_NAME, "gzip", {"gzip", "x-gzip", NULL}},
    {CMP_COMPRESS, ".Z", "application/x-compress", 0, GUNZIP_CMDNAME, GUNZIP_NAME, "compress", {"compress", "x-compress", NULL}},
    {CMP_BZIP2, ".bz2", "application/x-bzip", 0, BUNZIP2_CMDNAME, BUNZIP2_NAME, "bzip, bzip2", {"x-bzip", "bzip", "bzip2", NULL}},
    {CMP_DEFLATE, ".deflate", "application/x-deflate", 1, INFLATE_CMDNAME, INFLATE_NAME, "deflate", {"deflate", "x-deflate", NULL}},
    {CMP_NOCOMPRESS, NULL, NULL, 0, NULL, NULL, NULL, {NULL}},
};
/* *INDENT-ON* */

const char *compress_application_type(CompressionTypes compression);
char *uncompress_stream(URLFile *uf, bool useRealFile);
const char *uncompressed_file_type(const char *path, const char **ext);
void check_compression(char *path, URLFile *uf);
char *acceptableEncoding();
