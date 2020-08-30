#include "formdata.h"
#include "myctype.h"
#include "indep.h"
#include <wc.h>
#include <w3m.h>

static std::string_view check_accept_charset(std::string_view ac)
{
    auto s = ac;
    while (s.size())
    {
        while (s.size() && (IS_SPACE(s[0]) || s[0] == ','))
            s.remove_prefix(1);
        if (s.empty())
            break;
        auto e = s;
        while (e[0] && !(IS_SPACE(e[0]) || e[0] == ','))
            e.remove_prefix(1);
        if (wc_guess_charset(Strnew_charp_n(s.data(), e.data() - s.data())->ptr, WC_CES_NONE))
            return ac;
        s = e;
    }
    return "";
}

static std::string_view check_charset(std::string_view p)
{
    return wc_guess_charset(p.data(), WC_CES_NONE) ? p : "";
}

Str FormData::FormOpen(HtmlTagPtr tag, int fid)
{
    auto p = tag->GetAttributeValue(ATTR_METHOD, "get");
    auto q = tag->GetAttributeValue(ATTR_ACTION, "!CURRENT_URL!");
    auto r = tag->GetAttributeValue(ATTR_ACCEPT_CHARSET);
    if (r.size())
    {
        r = check_accept_charset(r);
    }
    if (r.empty())
    {
        r = tag->GetAttributeValue(ATTR_CHARSET);
        if (r.size())
        {
            r = check_charset(r);
        }
    }

    auto s = tag->GetAttributeValue(ATTR_ENCTYPE);
    auto tg = tag->GetAttributeValue(ATTR_TARGET);
    auto n = tag->GetAttributeValue(ATTR_NAME);

    if (fid < 0)
    {
        fid = forms.size();
        forms.push_back(nullptr);
    }
    else
    { /* <form_int> */
        if (fid >= forms.size())
            forms.resize(fid + 1);
    }
    // if (forms_size == 0)
    // {
    //     forms_size = INITIAL_FORM_SIZE;
    //     forms = New_N(FormPtr , forms_size);
    //     form_stack = NewAtom_N(int, forms_size);
    // }
    // else if (forms_size <= form_max)
    // {
    //     forms_size += form_max;
    //     forms = New_Reuse(FormPtr , forms, forms_size);
    //     form_stack = New_Reuse(int, form_stack, forms_size);
    // }

    forms[fid] = Form::Create(q, p, r, s, tg, n);
    form_stack.push_back(fid);

    return nullptr;
}

// push text to current_textarea
void FormData::feed_textarea(const char *str)
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

Str FormData::process_textarea(HtmlTagPtr tag, int width)
{
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

    Str tmp = nullptr;
    if (cur_form_id() < 0)
    {
        auto s = "<form_int method=internal action=none>";
        auto [pos, tag] = HtmlTag::parse(s, true);
        tmp = FormOpen(tag);
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
    if (n_textarea >= textarea_str.size())
    {
        textarea_str.resize(n_textarea + 1);
    }
    textarea_str[n_textarea] = Strnew();
    ignore_nl_textarea = true;

    return tmp;
}
