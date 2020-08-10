#include "html/html_context.h"
#include "fm.h"
#include "indep.h"
#include "w3m.h"
#include "frontend/terms.h"

HtmlContext::HtmlContext()
{
    if (fmInitialized && graph_ok())
    {
        symbol_width = symbol_width0 = 1;
    }
    else
    {
        symbol_width0 = 0;
        get_symbol(w3mApp::Instance().DisplayCharset, &symbol_width0);
        symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    }
}

HtmlContext::~HtmlContext()
{
}

Str HtmlContext::GetLinkNumberStr(int correction)
{
    return Sprintf("[%d]", cur_hseq + correction);
}

Str HtmlContext::TitleOpen(struct parsed_tag *tag)
{
    cur_title = Strnew();
    return NULL;
}

void HtmlContext::TitleContent(const char *str)
{
    if (!cur_title)
    {
        // not open
        assert(cur_title);
        return;
    }

    while (*str)
    {
        if (*str == '&')
        {
            auto [pos, cmd] = getescapecmd(str, w3mApp::Instance().InnerCharset);
            str = pos;
            cur_title->Push(cmd);
        }
        else if (*str == '\n' || *str == '\r')
        {
            cur_title->Push(' ');
            str++;
        }
        else
        {
            cur_title->Push(*(str++));
        }
    }
}

Str HtmlContext::TitleClose(struct parsed_tag *tag)
{
    if (!cur_title)
        return NULL;
    Strip(cur_title);
    auto tmp = Strnew_m_charp("<title_alt title=\"",
                              html_quote(cur_title),
                              "\">");
    cur_title = NULL;
    return tmp;
}
