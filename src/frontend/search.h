#pragma once
#include <memory>
#include <wc.h>

/* Search Result */
enum SearchResultTypes
{
    SR_NONE = 0,
    SR_FOUND = 0x1,
    SR_NOTFOUND = 0x2,
    SR_WRAPPED = 0x4,
};

using SearchFunc = SearchResultTypes (*)(const std::shared_ptr<struct Buffer> &buf, std::string_view str);
SearchResultTypes forwardSearch(const std::shared_ptr<struct Buffer> &buf, std::string_view str);
SearchResultTypes backwardSearch(const std::shared_ptr<struct Buffer> &buf, std::string_view str);
std::string conv_search_string(std::string_view str, CharacterEncodingScheme f_ces);
