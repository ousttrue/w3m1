#pragma once
#include <string_view>

void initMimeTypes();
const char *guessContentType(std::string_view filename);
