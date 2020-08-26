#pragma once
#include <memory>
#include "stream/url.h"
#include "stream/input_stream.h"

std::shared_ptr<struct Buffer> loadHTMLStream(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset, bool internal = false);
