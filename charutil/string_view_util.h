#pragma once
#include <string_view>
#include "myctype.h"

namespace svu
{

inline std::string_view strip_left(std::string_view src)
{
    while (src.size() && IS_SPACE(src.front()))
    {
        src.remove_prefix(1);
    }
    return src;
}

inline std::string_view strip_right(std::string_view src)
{
    while (src.size() && IS_SPACE(src.back()))
    {
        src.remove_suffix(1);
    }
    return src;
}

inline std::string_view strip(std::string_view src)
{
    return strip_left(strip_right(src));
}

inline std::tuple<std::string_view, std::string_view> split(std::string_view src, char delemeter)
{
    auto pos = src.find(delemeter);
    if (pos == std::string::npos)
    {
        return {};
    }

    auto key = strip(src.substr(0, pos));
    auto value = strip(src.substr(pos + 1));

    return {key, value};
}

// ignore case equal
inline bool ic_eq(std::string_view l, std::string_view r)
{
    if (l.size() != r.size())
    {
        return false;
    }
    auto rr = r.begin();
    for (auto ll = l.begin(); ll != l.end(); ++ll, ++rr)
    {
        if (tolower(*ll) != tolower(*rr))
        {
            return false;
        }
    }
    return true;
}

inline bool ic_begin_with(std::string_view src, std::string_view find)
{
    if (src.size() < find.size())
    {
        return false;
    }
    return ic_eq(src.substr(0, find.size()), find);
}

inline bool ic_ends_with(std::string_view src, std::string_view find)
{
    if (src.size() < find.size())
    {
        return false;
    }
    return ic_eq(src.substr(src.size()-find.size()), find);
}

} // namespace  svu
