#pragma once
#include <string_view>

void initMimeTypes();
const char *guessContentType(std::string_view filename);

bool is_text_type(std::string_view type);
bool is_html_type(std::string_view type);
