#pragma once
#include <memory>
// #include "html/html.h"
#include "stream/url.h"
#include "stream/istream.h"
#define MAX_ENV_LEVEL 20

struct Buffer;
std::shared_ptr<Buffer> loadHTMLStream(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset, bool internal = false);
std::shared_ptr<Buffer> loadHTMLString(const URL &url, std::string_view page, CharacterEncodingScheme content_charset = WC_CES_UTF_8);
