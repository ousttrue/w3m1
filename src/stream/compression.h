#pragma once
#include <memory>
#include <string_view>

#define SAVE_BUF_SIZE 1536

enum CompressionTypes
{
    CMP_NOCOMPRESS,
    CMP_COMPRESS,
    CMP_GZIP,
    CMP_BZIP2,
    CMP_DEFLATE,
};

CompressionTypes get_compression_type(std::string_view value);
std::string_view compress_application_type(CompressionTypes compression);
std::tuple<std::string_view, std::string_view> uncompressed_file_type(std::string_view path);

// char *uncompress_stream(const std::shared_ptr<struct URLFile> &uf, bool useRealFile);
// void check_compression(std::string_view path, const std::shared_ptr<struct URLFile> &uf);
char *acceptableEncoding();
std::shared_ptr<class InputStream> decompress(const std::shared_ptr<class InputStream> &stream, CompressionTypes type);
