#pragma once
#include "html.h"

struct HtmlTag : gc_cleanup
{
    HtmlTags tagid = HTML_UNKNOWN;
    unsigned char *attrid = nullptr;
    char **value;
    unsigned char *map;
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
    Str ToStr() const;

    HtmlTag(HtmlTags tag)
        : tagid(tag)
    {
    }

    std::string_view parse(std::string_view s, bool internal);

    int ul_type(int default_type = 0)
    {
        char *p;
        if (TryGetAttributeValue(ATTR_TYPE, &p))
        {
            if (!strcasecmp(p, "disc"))
                return (int)'d';
            else if (!strcasecmp(p, "circle"))
                return (int)'c';
            else if (!strcasecmp(p, "square"))
                return (int)'s';
        }
        return default_type;
    }

private:
    std::tuple<std::string_view, bool> parse_attr(std::string_view s, int nattr, bool internal);
};
using HtmlTagPtr = HtmlTag *;

std::tuple<std::string_view, HtmlTagPtr> parse_tag(std::string_view s, bool internal);
