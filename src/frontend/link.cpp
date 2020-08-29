#include <sstream>
#include "link.h"
#include "indep.h"
#include "w3m.h"

Link Link::create(const HtmlTagPtr &tag, CharacterEncodingScheme ces)
{
    auto href = tag->GetAttributeValue(ATTR_HREF);
    if (href.size())
        href = wc_conv_strict(remove_space(href.data()), w3mApp::Instance().InnerCharset, ces)->ptr;

    auto title = tag->GetAttributeValue(ATTR_TITLE);
    auto ctype = tag->GetAttributeValue(ATTR_TYPE);
    auto rel = tag->GetAttributeValue(ATTR_REL);

    LinkTypes type = LINK_TYPE_NONE;
    if (rel.size())
    {
        /* forward link type */
        type = LINK_TYPE_REL;
        if (title.empty())
            title = rel;
    }

    auto rev = tag->GetAttributeValue(ATTR_REV);
    if (rev.size())
    {
        /* reverse link type */
        type = LINK_TYPE_REV;
        if (title.size())
            title = rev;
    }

    Link link;
    link.m_url = href;
    link.m_title = title;
    link.m_ctype = ctype;
    link.m_type = type;
    return link;
}

std::string Link::toHtml(const URL &baseUrl, CharacterEncodingScheme ces) const
{
    // html quoted url
    std::string_view url;
    if (m_url.size())
    {
        auto pu = URL::Parse(m_url, &baseUrl);
        url = html_quote(pu.ToStr()->ptr);
    }
    else
        url = "(empty)";

    std::stringstream ss;
    ss << "<tr valign=top><td><a href=\""
       << url
       << "\">"
       << (m_title.size()
               ? html_quote(m_title)
               : "(empty)")
       << "</a><td>"
       << type();

    if (m_url.empty())
        url = "(empty)";
    else if (w3mApp::Instance().DecodeURL)
        url = html_quote(url_unquote_conv(m_url, ces));
    else
        url = html_quote(m_url);
    ss << "<td>" << url;
    if (m_ctype.size())
    {
        ss << " (" << html_quote(m_ctype) << ")";
    }
    ss << "\n";
    return ss.str();
}
