#pragma once
#include <wc.h>
#include <memory>

std::shared_ptr<struct Buffer> loadHTMLStream(const struct URL &url,
                                              const std::shared_ptr<class InputStream> &stream,
                                              CharacterEncodingScheme content_charset,
                                              bool internal = false);
