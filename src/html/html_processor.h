#pragma once
#include <memory>
#include "stream/url.h"
#include "stream/input_stream.h"

struct Buffer;
std::shared_ptr<Buffer> loadHTMLStream(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset, bool internal = false);
std::shared_ptr<Buffer> loadHTMLString(const URL &url, std::string_view page, CharacterEncodingScheme content_charset = WC_CES_UTF_8);
