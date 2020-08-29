#pragma once
#include "html.h"
#include <memory>
#include <vector>
#include <string>

// #define HTML_TAG_USE_STRING 1

class HtmlTag
{
    std::vector<unsigned char> attrid;
#ifdef HTML_TAG_USE_STRING
    std::vector<std::string> value;
#else
    std::vector<char *> value;
#endif
    std::vector<unsigned char> map;

    HtmlTag(HtmlTags tag)
        : tagid(tag)
    {
    }
    std::string_view _parse(std::string_view s, bool internal);
    std::tuple<std::string_view, bool> parse_attr(std::string_view s, int nattr, bool internal);

public:
    static std::tuple<std::string_view, std::shared_ptr<HtmlTag>> parse(std::string_view s, bool internal);
    HtmlTags tagid = HTML_UNKNOWN;
    bool need_reconstruct = false;

    bool CanAcceptAttribute(HtmlTagAttributes id) const;
    bool HasAttribute(HtmlTagAttributes id) const;
    bool TryGetAttributeValue(HtmlTagAttributes id, void *value) const;
    bool SetAttributeValue(HtmlTagAttributes id, const char *value);
    std::string_view GetAttributeValue(HtmlTagAttributes id, std::string_view defaultValue = "") const;
    int GetAttributeValue(HtmlTagAttributes id, int defaultValue) const;
    std::string ToStr() const;
    int ul_type(int default_type = 0) const;
};
using HtmlTagPtr = std::shared_ptr<HtmlTag>;
