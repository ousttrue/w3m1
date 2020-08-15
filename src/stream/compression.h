#pragma once
#include <memory>
#include <string_view>

enum CompressionTypes
{
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
};

const char *compress_application_type(CompressionTypes compression);
char *uncompress_stream(const std::shared_ptr<struct URLFile> &uf, bool useRealFile);
const char *uncompressed_file_type(const char *path, const char **ext);
void check_compression(std::string_view path, const std::shared_ptr<struct URLFile> &uf);
char *acceptableEncoding();
CompressionTypes get_compression_type(std::string_view value);
