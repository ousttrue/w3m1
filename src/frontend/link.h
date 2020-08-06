#pragma once
#include <string_view>
#include <string>
#include <wc.h>
#include "transport/url.h"
#include "html/html.h"

enum LinkTypes : char
{
    LINK_TYPE_NONE = 0,
    LINK_TYPE_REL = 1,
    LINK_TYPE_REV = 2,
};

class Link
{
    std::string m_url;
    std::string m_title;               /* Next, Contents, ... */
    std::string m_ctype;               /* Content-Type */
    LinkTypes m_type = LINK_TYPE_NONE; /* Rel, Rev */

public:
    static Link create(const parsed_tag &tag, CharacterEncodingScheme ces);

    std::string_view url() const
    {
        return m_url;
    }

    std::string_view title() const
    {
        return m_title.size() ? m_title : "(empty)";
    }

    std::string_view type() const
    {
        if (m_type == LINK_TYPE_REL)
            return " [Rel] ";
        else if (m_type == LINK_TYPE_REV)
            return " [Rev] ";
        else
            return " ";
    }

    std::string toHtml(const URL &baseUrl, CharacterEncodingScheme ces) const;
};
