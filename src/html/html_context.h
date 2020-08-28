#pragma once
#include <wc.h>
#include "frontend/buffer.h"
#include "stream/input_stream.h"
#include "html/html.h"
#include "html/form.h"
#include "html/readbuffer.h"
#include "html/table.h"


std::shared_ptr<struct Buffer> loadHTMLStream(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset, bool internal = false);
