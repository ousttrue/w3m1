#pragma once
#include "html.h"
#include <vector>
#include <string>
#include <gc_cpp.h>

struct HtmlTag : public gc_cleanup
{
    HtmlTags tagid = HTML_UNKNOWN;
    std::vector<unsigned char> attrid;
    std::vector<char *> value;
    std::vector<unsigned char> map;
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

    HtmlTag(HtmlTags tag)
        : tagid(tag)
    {
    }

    std::string_view parse(std::string_view s, bool internal);

    int ul_type(int default_type = 0) const;

private:
    std::tuple<std::string_view, bool> parse_attr(std::string_view s, int nattr, bool internal);
};
using HtmlTagPtr = HtmlTag *;

std::tuple<std::string_view, HtmlTagPtr> parse_tag(std::string_view s, bool internal);
