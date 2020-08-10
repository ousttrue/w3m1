#include "html/html_context.h"
#include "html/tokenizer.h"
#include "html/tagstack.h"
#include "html/image.h"
#include "http/compression.h"
#include "fm.h"
#include "indep.h"
#include "html/form.h"
#include "gc_helper.h"
#include "w3m.h"
#include "frontend/terms.h"
#include "myctype.h"
#include "file.h"

#define MAX_SELECT 10 /* max number of <select>..</select> \
                       * within one document */

#define MAX_TEXTAREA 10 /* max number of <textarea>..</textarea> \
                         * within one document */

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

    form_sp = -1;
    form_max = -1;
    forms_size = 0;
    forms = nullptr;

    n_select = -1;
    max_select = MAX_SELECT;
    select_option = New_N(FormSelectOption, max_select);
    cur_select = nullptr;

    n_textarea = -1;
    max_textarea = MAX_TEXTAREA;
    textarea_str = New_N(Str, max_textarea);
    cur_textarea = nullptr;
}

HtmlContext::~HtmlContext()
{
}

void HtmlContext::print_internal(TextLineList *tl)
{
    if (n_select > 0)
    {
        FormSelectOptionItem *ip;
        for (int i = 0; i < n_select; i++)
        {
            auto s = Sprintf("<select_int selectnumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            for (ip = select_option[i].first; ip; ip = ip->next)
            {
                s = Sprintf("<option_int value=\"%s\" label=\"%s\"%s>",
                            html_quote(ip->value ? ip->value->ptr : ip->label->ptr),
                            html_quote(ip->label->ptr),
                            ip->checked ? " selected" : "");
                pushTextLine(tl, newTextLine(s, 0));
            }
            s = Strnew("</select_int>");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
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

static char *check_accept_charset(char *ac)
{
    char *s = ac;
    while (*s)
    {
        while (*s && (IS_SPACE(*s) || *s == ','))
            s++;
        if (!*s)
            break;
        auto e = s;
        while (*e && !(IS_SPACE(*e) || *e == ','))
            e++;
        if (wc_guess_charset(Strnew_charp_n(s, e - s)->ptr, WC_CES_NONE))
            return ac;
        s = e;
    }
    return NULL;
}
static char *check_charset(char *p)
{
    return wc_guess_charset(p, WC_CES_NONE) ? p : NULL;
}
Str HtmlContext::FormOpen(struct parsed_tag *tag, int fid)
{
    auto p = "get";
    tag->TryGetAttributeValue(ATTR_METHOD, &p);
    auto q = "!CURRENT_URL!";
    tag->TryGetAttributeValue(ATTR_ACTION, &q);
    char *r = nullptr;
    if (tag->TryGetAttributeValue(ATTR_ACCEPT_CHARSET, &r))
        r = check_accept_charset(r);
    if (!r && tag->TryGetAttributeValue(ATTR_CHARSET, &r))
        r = check_charset(r);

    char *s = nullptr;
    tag->TryGetAttributeValue(ATTR_ENCTYPE, &s);
    char *tg = nullptr;
    tag->TryGetAttributeValue(ATTR_TARGET, &tg);
    char *n = nullptr;
    tag->TryGetAttributeValue(ATTR_NAME, &n);

    if (fid < 0)
    {
        form_max++;
        form_sp++;
        fid = form_max;
    }
    else
    { /* <form_int> */
        if (form_max < fid)
            form_max = fid;
        form_sp = fid;
    }
    if (forms_size == 0)
    {
        forms_size = INITIAL_FORM_SIZE;
        forms = New_N(FormList *, forms_size);
        form_stack = NewAtom_N(int, forms_size);
    }
    else if (forms_size <= form_max)
    {
        forms_size += form_max;
        forms = New_Reuse(FormList *, forms, forms_size);
        form_stack = New_Reuse(int, form_stack, forms_size);
    }
    form_stack[form_sp] = fid;

    if (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)
    {
        Str tmp = Sprintf("<form_int fid=\"%d\" action=\"%s\" method=\"%s\"",
                          fid, html_quote(q), html_quote(p));
        if (s)
            tmp->Push(Sprintf(" enctype=\"%s\"", html_quote(s)));
        if (tg)
            tmp->Push(Sprintf(" target=\"%s\"", html_quote(tg)));
        if (n)
            tmp->Push(Sprintf(" name=\"%s\"", html_quote(n)));
#ifdef USE_M17N
        if (r)
            tmp->Push(Sprintf(" accept-charset=\"%s\"", html_quote(r)));
#endif
        tmp->Push(">");
        return tmp;
    }

    forms[fid] = newFormList(q, p, r, s, tg, n, nullptr);
    return nullptr;
}

FormList *HtmlContext::FormEnd()
{
    for (int form_id = 1; form_id <= form_max; form_id++)
        forms[form_id]->next = forms[form_id - 1];
    return (form_max >= 0) ? forms[form_max] : nullptr;
}

void HtmlContext::FormSelectGrow(int selectnumber)
{
    if (selectnumber >= max_select)
    {
        max_select = 2 * selectnumber;
        select_option = New_Reuse(FormSelectOption,
                                  select_option,
                                  max_select);
    }
}

FormSelectOption *HtmlContext::FormSelect(int n)
{
    return &select_option[n];
}

void HtmlContext::FormSetSelect(int n)
{
    n_select = n;
    select_option[n_select].first = nullptr;
    select_option[n_select].last = nullptr;
}

std::pair<int, FormSelectOption *> HtmlContext::FormSelectCurrent()
{
    if (n_select < 0)
    {
        return {};
    }
    return {n_select, &select_option[n_select]};
}

Str HtmlContext::process_select(struct parsed_tag *tag)
{
    Str tmp = nullptr;
    if (cur_form_id() < 0)
    {
        auto s = "<form_int method=internal action=none>";
        tmp = FormOpen(parse_tag(&s, TRUE));
    }

    auto p = "";
    tag->TryGetAttributeValue(ATTR_NAME, &p);
    cur_select = Strnew(p);
    select_is_multiple = tag->HasAttribute(ATTR_MULTIPLE);

    if (!select_is_multiple)
    {
        select_str = Strnew("<pre_int>");
        if (displayLinkNumber)
            select_str->Push(GetLinkNumberStr(0));
        select_str->Push(Sprintf("[<input_alt hseq=\"%d\" "
                                 "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
                                 Increment(), cur_form_id(), html_quote(p), n_select));
        select_str->Push(">");
        if (n_select == max_select)
        {
            max_select *= 2;
            select_option =
                New_Reuse(FormSelectOption, select_option, max_select);
        }
        select_option[n_select].first = nullptr;
        select_option[n_select].last = nullptr;
        cur_option_maxwidth = 0;
    }
    else
    {
        select_str = Strnew();
    }

    cur_option = nullptr;
    cur_status = R_ST_NORMAL;
    n_selectitem = 0;
    return tmp;
}

void HtmlContext::feed_select(const char *str)
{
    Str tmp = Strnew();
    int prev_status = cur_status;
    static int prev_spaces = -1;

    if (cur_select == nullptr)
        return;
    while (read_token(tmp, const_cast<char **>(&str), &cur_status, 0, 0))
    {
        if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
            continue;
        const char *p = tmp->ptr;
        if (tmp->ptr[0] == '<' && tmp->Back() == '>')
        {
            struct parsed_tag *tag;
            char *q;
            if (!(tag = parse_tag(&p, FALSE)))
                continue;
            switch (tag->tagid)
            {
            case HTML_OPTION:
                process_option();
                cur_option = Strnew();
                if (tag->TryGetAttributeValue(ATTR_VALUE, &q))
                    cur_option_value = Strnew(q);
                else
                    cur_option_value = nullptr;
                if (tag->TryGetAttributeValue(ATTR_LABEL, &q))
                    cur_option_label = Strnew(q);
                else
                    cur_option_label = nullptr;
                cur_option_selected = tag->HasAttribute(ATTR_SELECTED);
                prev_spaces = -1;
                break;
            case HTML_N_OPTION:
                /* do nothing */
                break;
            default:
                /* never happen */
                break;
            }
        }
        else if (cur_option)
        {
            while (*p)
            {
                if (IS_SPACE(*p) && prev_spaces != 0)
                {
                    p++;
                    if (prev_spaces > 0)
                        prev_spaces++;
                }
                else
                {
                    if (IS_SPACE(*p))
                        prev_spaces = 1;
                    else
                        prev_spaces = 0;
                    if (*p == '&')
                    {
                        auto [pos, cmd] = getescapecmd(p, w3mApp::Instance().InnerCharset);
                        p = const_cast<char *>(pos);
                        cur_option->Push(cmd);
                    }
                    else
                        cur_option->Push(*(p++));
                }
            }
        }
    }
}

Str HtmlContext::process_n_select()
{
    if (cur_select == nullptr)
        return nullptr;
    process_option();

    if (!select_is_multiple)
    {
        if (select_option[n_select].first)
        {
            FormItemList sitem;
            chooseSelectOption(&sitem, select_option[n_select].first);
            select_str->Push(textfieldrep(sitem.label, cur_option_maxwidth));
        }
        select_str->Push("</input_alt>]</pre_int>");
        n_select++;
    }
    else
    {
        select_str->Push("<br>");
    }

    cur_select = nullptr;
    n_selectitem = 0;
    return select_str;
}

void HtmlContext::process_option()
{
    char begin_char = '[', end_char = ']';
    int len;

    if (cur_select == nullptr || cur_option == nullptr)
        return;
    while (cur_option->Size() > 0 && IS_SPACE(cur_option->Back()))
        cur_option->Pop(1);
    if (cur_option_value == nullptr)
        cur_option_value = cur_option;
    if (cur_option_label == nullptr)
        cur_option_label = cur_option;

    if (!select_is_multiple)
    {
        len = get_Str_strwidth(cur_option_label);
        if (len > cur_option_maxwidth)
            cur_option_maxwidth = len;
        addSelectOption(&select_option[n_select],
                        cur_option_value,
                        cur_option_label, cur_option_selected);
        return;
    }

    if (!select_is_multiple)
    {
        begin_char = '(';
        end_char = ')';
    }
    select_str->Push(Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                             "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                             begin_char, Increment(), cur_form_id(),
                             select_is_multiple ? "checkbox" : "radio",
                             html_quote(cur_select->ptr),
                             html_quote(cur_option_value->ptr)));
    if (cur_option_selected)
        select_str->Push(" checked>*</input_alt>");
    else
        select_str->Push("> </input_alt>");
    select_str->Push(end_char);
    select_str->Push(html_quote(cur_option_label->ptr));
    select_str->Push("</pre_int>");
    n_selectitem++;
}

Str HtmlContext::process_input(struct parsed_tag *tag)
{
    Str tmp = nullptr;
    if (cur_form_id() < 0)
    {
        const char *s = "<form_int method=internal action=none>";
        tmp = FormOpen(parse_tag(&s, TRUE));
    }
    if (tmp == nullptr)
    {
        tmp = Strnew();
    }

    int i, w, v, x, y, z, iw, ih;
    const char *qq = "";
    int qlen = 0;

    auto p = "text";
    tag->TryGetAttributeValue(ATTR_TYPE, &p);
    const char *q = nullptr;
    tag->TryGetAttributeValue(ATTR_VALUE, &q);
    auto r = "";
    tag->TryGetAttributeValue(ATTR_NAME, &r);
    w = 20;
    tag->TryGetAttributeValue(ATTR_SIZE, &w);
    i = 20;
    tag->TryGetAttributeValue(ATTR_MAXLENGTH, &i);
    char *p2 = nullptr;
    tag->TryGetAttributeValue(ATTR_ALT, &p2);
    x = tag->HasAttribute(ATTR_CHECKED);
    y = tag->HasAttribute(ATTR_ACCEPT);
    z = tag->HasAttribute(ATTR_READONLY);

    v = formtype(p);
    if (v == FORM_UNKNOWN)
        return nullptr;

    if (!q)
    {
        switch (v)
        {
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            q = "SUBMIT";
            break;
        case FORM_INPUT_RESET:
            q = "RESET";
            break;
            /* if no VALUE attribute is specified in 
             * <INPUT TYPE=CHECKBOX> tag, then the value "on" is used 
             * as a default value. It is not a part of HTML4.0 
             * specification, but an imitation of Netscape behaviour. 
             */
        case FORM_INPUT_CHECKBOX:
            q = "on";
        }
    }
    /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
    if (v == FORM_INPUT_FILE)
        q = nullptr;
    if (q)
    {
        qq = html_quote(q);
        qlen = get_strwidth(q);
    }

    tmp->Push("<pre_int>");
    switch (v)
    {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
        if (displayLinkNumber)
            tmp->Push(GetLinkNumberStr(0));
        tmp->Push('[');
        break;
    case FORM_INPUT_RADIO:
        if (displayLinkNumber)
            tmp->Push(GetLinkNumberStr(0));
        tmp->Push('(');
    }

    tmp->Push(Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=%s "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      Increment(), cur_form_id(), p, html_quote(r), w, i, qq));

    if (x)
        tmp->Push(" checked");
    if (y)
        tmp->Push(" accept");
    if (z)
        tmp->Push(" readonly");
    tmp->Push('>');

    if (v == FORM_INPUT_HIDDEN)
        tmp->Push("</input_alt></pre_int>");
    else
    {
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            tmp->Push("<u>");
            break;
        case FORM_INPUT_IMAGE:
        {
            char *s = nullptr;
            tag->TryGetAttributeValue(ATTR_SRC, &s);
            if (s)
            {
                tmp->Push(Sprintf("<img src=\"%s\"", html_quote(s)));
                if (p2)
                    tmp->Push(Sprintf(" alt=\"%s\"", html_quote(p2)));
                if (tag->TryGetAttributeValue(ATTR_WIDTH, &iw))
                    tmp->Push(Sprintf(" width=\"%d\"", iw));
                if (tag->TryGetAttributeValue(ATTR_HEIGHT, &ih))
                    tmp->Push(Sprintf(" height=\"%d\"", ih));
                tmp->Push(" pre_int>");
                tmp->Push("</input_alt></pre_int>");
                return tmp;
            }
        }
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            if (displayLinkNumber)
                tmp->Push(GetLinkNumberStr(-1));
            tmp->Push("[");
            break;
        }
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
            i = 0;
            if (q)
            {
                for (; i < qlen && i < w; i++)
                    tmp->Push('*');
            }
            for (; i < w; i++)
                tmp->Push(' ');
            break;
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            if (q)
                tmp->Push(textfieldrep(Strnew(q), w));
            else
            {
                for (i = 0; i < w; i++)
                    tmp->Push(' ');
            }
            break;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            if (p2)
                tmp->Push(html_quote(p2));
            else
                tmp->Push(qq);
            break;
        case FORM_INPUT_RESET:
            tmp->Push(qq);
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (x)
                tmp->Push('*');
            else
                tmp->Push(' ');
            break;
        }
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            tmp->Push("</u>");
            break;
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            tmp->Push("]");
        }
        tmp->Push("</input_alt>");
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
        case FORM_INPUT_CHECKBOX:
            tmp->Push(']');
            break;
        case FORM_INPUT_RADIO:
            tmp->Push(')');
        }
        tmp->Push("</pre_int>");
    }
    return tmp;
}

#define IMG_SYMBOL UL_SYMBOL(12)
Str HtmlContext::process_img(struct parsed_tag *tag, int width)
{
    char *p, *q, *r, *r2 = nullptr, *t;
#ifdef USE_IMAGE
    int w, i, nw, ni = 1, n, w0 = -1, i0 = -1;
    int align, xoffset, yoffset, top, bottom, ismap = 0;
    int use_image = w3mApp::Instance().activeImage && w3mApp::Instance().displayImage;
#else
    int w, i, nw, n;
#endif
    int pre_int = FALSE, ext_pre_int = FALSE;
    Str tmp = Strnew();

    if (!tag->TryGetAttributeValue(ATTR_SRC, &p))
        return tmp;
    p = remove_space(p);
    q = nullptr;
    tag->TryGetAttributeValue(ATTR_ALT, &q);
    if (!pseudoInlines && (q == nullptr || (*q == '\0' && ignore_null_img_alt)))
        return tmp;
    t = q;
    tag->TryGetAttributeValue(ATTR_TITLE, &t);
    w = -1;
    if (tag->TryGetAttributeValue(ATTR_WIDTH, &w))
    {
        if (w < 0)
        {
            if (width > 0)
                w = (int)(-width * w3mApp::Instance().pixel_per_char * w / 100 + 0.5);
            else
                w = -1;
        }
#ifdef USE_IMAGE
        if (use_image)
        {
            if (w > 0)
            {
                w = (int)(w * w3mApp::Instance().image_scale / 100 + 0.5);
                if (w == 0)
                    w = 1;
                else if (w > MAX_IMAGE_SIZE)
                    w = MAX_IMAGE_SIZE;
            }
        }
#endif
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        i = -1;
        if (tag->TryGetAttributeValue(ATTR_HEIGHT, &i))
        {
            if (i > 0)
            {
                i = (int)(i * w3mApp::Instance().image_scale / 100 + 0.5);
                if (i == 0)
                    i = 1;
                else if (i > MAX_IMAGE_SIZE)
                    i = MAX_IMAGE_SIZE;
            }
            else
            {
                i = -1;
            }
        }
        align = -1;
        tag->TryGetAttributeValue(ATTR_ALIGN, &align);
        ismap = 0;
        if (tag->HasAttribute(ATTR_ISMAP))
            ismap = 1;
    }
    else
#endif
        tag->TryGetAttributeValue(ATTR_HEIGHT, &i);
    r = nullptr;
    tag->TryGetAttributeValue(ATTR_USEMAP, &r);
    if (tag->HasAttribute(ATTR_PRE_INT))
        ext_pre_int = TRUE;

    tmp = Strnew_size(128);
#ifdef USE_IMAGE
    if (use_image)
    {
        switch (align)
        {
        case ALIGN_LEFT:
            tmp->Push("<div_int align=left>");
            break;
        case ALIGN_CENTER:
            tmp->Push("<div_int align=center>");
            break;
        case ALIGN_RIGHT:
            tmp->Push("<div_int align=right>");
            break;
        }
    }
#endif
    if (r)
    {
        r2 = strchr(r, '#');
        auto s = "<form_int method=internal action=map>";
        auto tmp2 = FormOpen(parse_tag(&s, TRUE));
        if (tmp2)
            tmp->Push(tmp2);
        tmp->Push(Sprintf("<input_alt fid=\"%d\" "
                          "type=hidden name=link value=\"",
                          cur_form_id()));
        tmp->Push(html_quote((r2) ? r2 + 1 : r));
        tmp->Push(Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                          "type=submit no_effect=true>",
                          Increment(), cur_form_id()));
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        w0 = w;
        i0 = i;
        if (w < 0 || i < 0)
        {
            Image image;
            URL u;

            u.Parse2(wc_conv(p, w3mApp::Instance().InnerCharset, CES())->ptr, GetCurBaseUrl());
            image.url = u.ToStr()->ptr;
            if (!uncompressed_file_type(u.file.c_str(), &image.ext))
                image.ext = filename_extension(u.file.c_str(), TRUE);
            image.cache = nullptr;
            image.width = w;
            image.height = i;

            image.cache = getImage(&image, GetCurBaseUrl(), IMG_FLAG_SKIP);
            if (image.cache && image.cache->width > 0 &&
                image.cache->height > 0)
            {
                w = w0 = image.cache->width;
                i = i0 = image.cache->height;
            }
            if (w < 0)
                w = 8 * w3mApp::Instance().pixel_per_char;
            if (i < 0)
                i = w3mApp::Instance().pixel_per_line;
        }
        nw = (w > 3) ? (int)((w - 3) / w3mApp::Instance().pixel_per_char + 1) : 1;
        ni = (i > 3) ? (int)((i - 3) / w3mApp::Instance().pixel_per_line + 1) : 1;
        tmp->Push(
            Sprintf("<pre_int><img_alt hseq=\"%d\" src=\"", cur_iseq++));
        pre_int = TRUE;
    }
    else
#endif
    {
        if (w < 0)
            w = 12 * w3mApp::Instance().pixel_per_char;
        nw = w ? (int)((w - 1) / w3mApp::Instance().pixel_per_char + 1) : 1;
        if (r)
        {
            tmp->Push("<pre_int>");
            pre_int = TRUE;
        }
        tmp->Push("<img_alt src=\"");
    }
    tmp->Push(html_quote(p));
    tmp->Push("\"");
    if (t)
    {
        tmp->Push(" title=\"");
        tmp->Push(html_quote(t));
        tmp->Push("\"");
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        if (w0 >= 0)
            tmp->Push(Sprintf(" width=%d", w0));
        if (i0 >= 0)
            tmp->Push(Sprintf(" height=%d", i0));
        switch (align)
        {
        case ALIGN_TOP:
            top = 0;
            bottom = ni - 1;
            yoffset = 0;
            break;
        case ALIGN_MIDDLE:
            top = ni / 2;
            bottom = top;
            if (top * 2 == ni)
                yoffset = (int)(((ni + 1) * w3mApp::Instance().pixel_per_line - i) / 2);
            else
                yoffset = (int)((ni * w3mApp::Instance().pixel_per_line - i) / 2);
            break;
        case ALIGN_BOTTOM:
            top = ni - 1;
            bottom = 0;
            yoffset = (int)(ni * w3mApp::Instance().pixel_per_line - i);
            break;
        default:
            top = ni - 1;
            bottom = 0;
            if (ni == 1 && ni * w3mApp::Instance().pixel_per_line > i)
                yoffset = 0;
            else
            {
                yoffset = (int)(ni * w3mApp::Instance().pixel_per_line - i);
                if (yoffset <= -2)
                    yoffset++;
            }
            break;
        }
        xoffset = (int)((nw * w3mApp::Instance().pixel_per_char - w) / 2);
        if (xoffset)
            tmp->Push(Sprintf(" xoffset=%d", xoffset));
        if (yoffset)
            tmp->Push(Sprintf(" yoffset=%d", yoffset));
        if (top)
            tmp->Push(Sprintf(" top_margin=%d", top));
        if (bottom)
            tmp->Push(Sprintf(" bottom_margin=%d", bottom));
        if (r)
        {
            tmp->Push(" usemap=\"");
            tmp->Push(html_quote((r2) ? r2 + 1 : r));
            tmp->Push("\"");
        }
        if (ismap)
            tmp->Push(" ismap");
    }
#endif
    tmp->Push(">");
    if (q != nullptr && *q == '\0' && ignore_null_img_alt)
        q = nullptr;
    if (q != nullptr)
    {
        n = get_strwidth(q);
#ifdef USE_IMAGE
        if (use_image)
        {
            if (n > nw)
            {
                char *r;
                for (r = q, n = 0; r; r += get_mclen(r), n += get_mcwidth(r))
                {
                    if (n + get_mcwidth(r) > nw)
                        break;
                }
                tmp->Push(html_quote(Strnew_charp_n(q, r - q)->ptr));
            }
            else
                tmp->Push(html_quote(q));
        }
        else
#endif
            tmp->Push(html_quote(q));
        goto img_end;
    }
    if (w > 0 && i > 0)
    {
        /* guess what the image is! */
        if (w < 32 && i < 48)
        {
            /* must be an icon or space */
            n = 1;
            if (strcasestr(p, "space") || strcasestr(p, "blank"))
                tmp->Push("_");
            else
            {
                if (w * i < 8 * 16)
                    tmp->Push("*");
                else
                {
                    if (!pre_int)
                    {
                        tmp->Push("<pre_int>");
                        pre_int = TRUE;
                    }
                    push_symbol(tmp, IMG_SYMBOL, SymbolWidth(), 1);
                    n = SymbolWidth();
                }
            }
            goto img_end;
        }
        if (w > 200 && i < 13)
        {
            /* must be a horizontal line */
            if (!pre_int)
            {
                tmp->Push("<pre_int>");
                pre_int = TRUE;
            }
            w = w / w3mApp::Instance().pixel_per_char / SymbolWidth();
            if (w <= 0)
                w = 1;
            push_symbol(tmp, HR_SYMBOL, SymbolWidth(), w);
            n = w * SymbolWidth();
            goto img_end;
        }
    }
    for (q = p; *q; q++)
        ;
    while (q > p && *q != '/')
        q--;
    if (*q == '/')
        q++;
    tmp->Push('[');
    n = 1;
    p = q;
    for (; *q; q++)
    {
        if (!IS_ALNUM(*q) && *q != '_' && *q != '-')
        {
            break;
        }
        tmp->Push(*q);
        n++;
        if (n + 1 >= nw)
            break;
    }
    tmp->Push(']');
    n++;
img_end:
    if (use_image)
    {
        for (; n < nw; n++)
            tmp->Push(' ');
    }

    tmp->Push("</img_alt>");
    if (pre_int && !ext_pre_int)
        tmp->Push("</pre_int>");
    if (r)
    {
        tmp->Push("</input_alt>");
        FormClose();
    }

    if (use_image)
    {
        switch (align)
        {
        case ALIGN_RIGHT:
        case ALIGN_CENTER:
        case ALIGN_LEFT:
            tmp->Push("</div_int>");
            break;
        }
    }

    return tmp;
}

Str HtmlContext::process_anchor(struct parsed_tag *tag, char *tagbuf)
{
    if (tag->need_reconstruct)
    {
        tag->SetAttributeValue(ATTR_HSEQ, Sprintf("%d", Increment())->ptr);
        return tag->ToStr();
    }
    else
    {
        Str tmp = Sprintf("<a hseq=\"%d\"", Increment());
        tmp->Push(tagbuf + 2);
        return tmp;
    }
}

void HtmlContext::TextareaGrow(int textareanumber)
{
    if (textareanumber >= max_textarea)
    {
        max_textarea = 2 * textareanumber;
        textarea_str = New_Reuse(Str, textarea_str,
                                 max_textarea);
    }
}

void HtmlContext::Textarea(int n, Str str)
{
    n_textarea = n;
    textarea_str[n_textarea] = str;
}

Str HtmlContext::Textarea(int n) const
{
    return textarea_str[n];
}

std::pair<int, Str> HtmlContext::TextareaCurrent() const
{
    if (n_textarea < 0)
    {
        return {-1, nullptr};
    }
    return {n_textarea, textarea_str[n_textarea]};
}


// push text to current_textarea
void HtmlContext::feed_textarea(const char *str)
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

Str HtmlContext::process_textarea(struct parsed_tag *tag, int width)
{
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

    Str tmp = nullptr;
    if (cur_form_id() < 0)
    {
        auto s = "<form_int method=internal action=none>";
        tmp = FormOpen(parse_tag(&s, TRUE));
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

Str HtmlContext::process_n_textarea()
{
    if (cur_textarea == nullptr)
        return nullptr;

    auto tmp = Strnew();
    tmp->Push(Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      Get(), cur_form_id(),
                      html_quote(cur_textarea->ptr),
                      cur_textarea_size, cur_textarea_rows,
                      cur_textarea_rows - 1, n_textarea));
    if (cur_textarea_readonly)
        tmp->Push(" readonly");
    tmp->Push("><u>");
    for (int i = 0; i < cur_textarea_size; i++)
        tmp->Push(' ');
    tmp->Push("</u></input_alt>]</pre_int>\n");
    Increment();
    n_textarea++;
    cur_textarea = nullptr;

    return tmp;
}
