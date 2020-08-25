#pragma once
#include <wc.h>
#include <string_view>
#include <memory>
#include "stream/input_stream.h"
#include "stream/url.h"

std::shared_ptr<struct Buffer> doExternal(const URL &url, const InputStreamPtr &stream, std::string_view type);
void initMailcap();
bool is_dump_text_type(std::string_view type);
