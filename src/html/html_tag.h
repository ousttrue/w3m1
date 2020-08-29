#pragma once
#include "html.h"
#include <vector>
#include <string>
#include <gc_cpp.h>

class HtmlTag : public gc_cleanup
{
    std::vector<unsigned char> attrid;
    std::vector<char *> value;
    std::vector<unsigned char> map;

    HtmlTag(HtmlTags tag)
        : tagid(tag)
    {
    }
    std::string_view _parse(std::string_view s, bool internal);
    std::tuple<std::string_view, bool> parse_attr(std::string_view s, int nattr, bool internal);
public:
    static std::tuple<std::string_view, HtmlTag*> parse(std::string_view s, bool internal);
    HtmlTags tagid = HTML_UNKNOWN;
    bool need_reconstruct = false;

    bool CanAcceptAttribute(HtmlTagAttributes id) const;
    bool HasAttribute(HtmlTagAttributes id) const;
    bool TryGetAttributeValue(HtmlTagAttributes id, void *value) const;
    bool SetAttributeValue(HtmlTagAttributes id, const char *value);
    std::string_view GetAttributeValue(HtmlTagAttributes id) const
    {
        char *value = nullptr;
        TryGetAttributeValue(id, &value);
        if (!value)
        {
            return "";
        }
        return value;
    }
    std::string ToStr() const;
    int ul_type(int default_type = 0) const;
};
using HtmlTagPtr = HtmlTag *;
