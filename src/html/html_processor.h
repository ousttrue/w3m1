#pragma once
#include <memory>
// #include "html/html.h"
#include "stream/url.h"
#include "stream/istream.h"
#define MAX_ENV_LEVEL 20

struct Buffer;
std::shared_ptr<Buffer> loadHTMLStream(const URL &url, const InputStreamPtr &stream, bool internal);
inline std::shared_ptr<Buffer> loadHTMLBuffer(const URL &url, const InputStreamPtr &stream)
{
    return loadHTMLStream(url, stream, false);
}
std::shared_ptr<Buffer> loadHTMLString(const URL &url, Str page);
