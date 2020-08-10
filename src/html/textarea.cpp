#include "html/html_processor.h"
#include "gc_helper.h"
#include "textlist.h"
#include "Str.h"
#include "indep.h"
#include "w3m.h"
#include <wc.h>

void HtmlTextArea::clear(int n)
{
    n_textarea = n;
    if (!max_textarea)
    { /* halfload */
        max_textarea = MAX_TEXTAREA;
        textarea_str = New_N(Str, max_textarea);
    }

    // n_textarea = 0;
    // cur_textarea = nullptr;
    // max_textarea = MAX_TEXTAREA;
    // textarea_str = New_N(Str, max_textarea);
}

void HtmlTextArea::grow(int textareanumber)
{
    if (textareanumber >= max_textarea)
    {
        max_textarea = 2 * textareanumber;
        textarea_str = New_Reuse(Str, textarea_str,
                                 max_textarea);
    }
}

void HtmlTextArea::set(int n, Str str)
{
    n_textarea = n;
    textarea_str[n_textarea] = str;
}

Str HtmlTextArea::get(int n) const
{
    return textarea_str[n];
}

std::pair<int, Str> HtmlTextArea::getCurrent() const
{
    if (n_textarea < 0)
    {
        return {-1, nullptr};
    }
    return {n_textarea, textarea_str[n_textarea]};
}

void HtmlTextArea::print_internal(TextLineList *tl)
{
    if (n_textarea > 0)
    {
        for (int i = 0; i < n_textarea; i++)
        {
            auto s = Sprintf("<textarea_int textareanumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            s = Strnew(html_quote(textarea_str[i]->ptr));
            s->Push("</textarea_int>");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
}

// push text to current_textarea
void HtmlTextArea::feed_textarea(const char *str)
{
    if (cur_textarea == nullptr)
        return;

    if (ignore_nl_textarea)
    {
        if (*str == '\r')
            str++;
        if (*str == '\n')
            str++;
    }
    ignore_nl_textarea = false;

    while (*str)
    {
        if (*str == '&')
        {
            auto [pos, cmd] = getescapecmd(str, w3mApp::Instance().InnerCharset);
            str = const_cast<char *>(pos);
            textarea_str[n_textarea]->Push(cmd);
        }
        else if (*str == '\n')
        {
            textarea_str[n_textarea]->Push("\r\n");
            str++;
        }
        else if (*str != '\r')
            textarea_str[n_textarea]->Push(*(str++));
    }
}

Str HtmlTextArea::process_textarea(struct parsed_tag *tag, int width)
{
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

    Str tmp = nullptr;
    if (cur_form_id() < 0)
    {
        auto s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }

    auto p = "";
    tag->TryGetAttributeValue(ATTR_NAME, &p);
    cur_textarea = Strnew(p);
    cur_textarea_size = 20;
    if (tag->TryGetAttributeValue(ATTR_COLS, &p))
    {
        cur_textarea_size = atoi(p);
        if (p[strlen(p) - 1] == '%')
            cur_textarea_size = width * cur_textarea_size / 100 - 2;
        if (cur_textarea_size <= 0)
        {
            cur_textarea_size = 20;
        }
        else if (cur_textarea_size > TEXTAREA_ATTR_COL_MAX)
        {
            cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
        }
    }
    cur_textarea_rows = 1;
    if (tag->TryGetAttributeValue(ATTR_ROWS, &p))
    {
        cur_textarea_rows = atoi(p);
        if (cur_textarea_rows <= 0)
        {
            cur_textarea_rows = 1;
        }
        else if (cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX)
        {
            cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
        }
    }
    cur_textarea_readonly = tag->HasAttribute(ATTR_READONLY);
    if (n_textarea >= max_textarea)
    {
        max_textarea *= 2;
        textarea_str = New_Reuse(Str, textarea_str, max_textarea);
    }
    textarea_str[n_textarea] = Strnew();
    ignore_nl_textarea = TRUE;

    return tmp;
}

extern int cur_hseq;

Str HtmlTextArea::process_n_textarea(void)
{
    if (cur_textarea == nullptr)
        return nullptr;

    auto tmp = Strnew();
    tmp->Push(Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      cur_hseq, cur_form_id(),
                      html_quote(cur_textarea->ptr),
                      cur_textarea_size, cur_textarea_rows,
                      cur_textarea_rows - 1, n_textarea));
    if (cur_textarea_readonly)
        tmp->Push(" readonly");
    tmp->Push("><u>");
    for (int i = 0; i < cur_textarea_size; i++)
        tmp->Push(' ');
    tmp->Push("</u></input_alt>]</pre_int>\n");
    cur_hseq++;
    n_textarea++;
    cur_textarea = nullptr;

    return tmp;
}
