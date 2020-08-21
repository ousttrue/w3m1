#include <string_view_util.h>
#include "html/html_context.h"
#include "html/tokenizer.h"
#include "html/tagstack.h"
#include "html/image.h"
#include "html/frame.h"
#include "html/form.h"
#include "html/maparea.h"
#include "stream/compression.h"
#include "frontend/terminal.h"
#include "indep.h"
#include "gc_helper.h"
#include "w3m.h"
#include "myctype.h"
#include "file.h"
#include "commands.h"
#include "textlist.h"

#define MAX_SELECT 10 /* max number of <select>..</select> \
                       * within one document */

#define MAX_TEXTAREA 10 /* max number of <textarea>..</textarea> \
                         * within one document */

static int currentLn(const BufferPtr &buf)
{
    if (buf->CurrentLine())
        /*     return buf->currentLine->real_linenumber + 1;      */
        return buf->CurrentLine()->linenumber + 1;
    else
        return 1;
}

HtmlContext::HtmlContext()
{
    doc_charset = w3mApp::Instance().DocumentCharset;

    if (Terminal::graph_ok())
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

    cur_select = nullptr;

    n_textarea = -1;
    max_textarea = MAX_TEXTAREA;
    textarea_str = New_N(Str, max_textarea);
    cur_textarea = nullptr;
}

HtmlContext::~HtmlContext()
{
}

void HtmlContext::SetMetaCharset(CharacterEncodingScheme ces)
{
    if (cur_document_charset == 0 && w3mApp::Instance().UseContentCharset)
    {
        cur_document_charset = ces;
    }
}

// from HTTP header
//
// Content-Type: text/html; charset=utf-8
//
void HtmlContext::Initialize(const BufferPtr &newBuf, CharacterEncodingScheme content_charset)
{
    // if (newBuf)
    // {
    //     if (newBuf->bufferprop & BP_FRAME)
    //         charset = w3mApp::Instance().InnerCharset;
    //     else if (newBuf->document_charset)
    //         charset = doc_charset = newBuf->document_charset;
    // }

    // if (content_charset && w3mApp::Instance().UseContentCharset)
    doc_charset = content_charset;
    // else if (f->guess_type == "application/xhtml+xml")
    //     doc_charset = WC_CES_UTF_8;
}

void HtmlContext::print_internal_information(struct html_feed_environ *henv)
{
    // TDOO:
    //     TextLineList *tl = newTextLineList();

    //     {
    //         auto s = Strnew("<internal>");
    //         pushTextLine(tl, newTextLine(s, 0));
    //         if (henv->title)
    //         {
    //             s = Strnew_m_charp("<title_alt title=\"",
    //                                html_quote(henv->title), "\">");
    //             pushTextLine(tl, newTextLine(s, 0));
    //         }
    //     }

    //     get_formselect()->print_internal(tl);
    //     get_textarea()->print_internal(tl);

    //     {
    //         auto s = Strnew("</internal>");
    //         pushTextLine(tl, newTextLine(s, 0));
    //     }

    //     if (henv->buf)
    //     {
    //         appendTextLineList(henv->buf, tl);
    //     }
    //     else if (henv->f)
    //     {
    //         TextLineListItem *p;
    //         for (p = tl->first; p; p = p->next)
    //             fprintf(henv->f, "%s\n", Str_conv_to_halfdump(p->ptr->line)->ptr);
    //     }
}

void HtmlContext::print_internal(TextLineList *tl)
{
    if (select_option.size())
    {
        int i = 0;
        for (auto &so : select_option)
        {
            auto s = Sprintf("<select_int selectnumber=%d>", i++);
            pushTextLine(tl, newTextLine(s, 0));
            for (auto ip = so.first; ip; ip = ip->next)
            {
                s = Sprintf("<option_int value=\"%s\" label=\"%s\"%s>",
                            html_quote(ip->value.size() ? ip->value : ip->label),
                            html_quote(ip->label),
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

    forms[fid] = FormList::Create(q, p,
                                  r ? r : "",
                                  s ? s : "",
                                  tg ? tg : "",
                                  n ? n : "");
    return nullptr;
}

FormList *HtmlContext::FormEnd()
{
    // for (int form_id = 1; form_id <= form_max; form_id++)
    //     forms[form_id]->next = forms[form_id - 1];
    return (form_max >= 0) ? forms[form_max] : nullptr;
}

void HtmlContext::FormSelectGrow(int selectnumber)
{
    // if (selectnumber >= max_select)
    // {
    //     max_select = 2 * selectnumber;
    //     select_option = New_Reuse(FormSelectOptionList,
    //                               select_option,
    //                               max_select);
    // }
}

FormSelectOptionList *HtmlContext::FormSelect(int n)
{
    return &select_option[n];
}

void HtmlContext::FormSetSelect(int n)
{
    if (n >= select_option.size())
    {
        select_option.resize(n + 1);
    }
    select_option[n] = {};
    n_select = n;
}

std::pair<int, FormSelectOptionList *> HtmlContext::FormSelectCurrent()
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
        tmp = FormOpen(parse_tag(&s, true));
    }

    auto p = "";
    tag->TryGetAttributeValue(ATTR_NAME, &p);
    cur_select = Strnew(p);
    select_is_multiple = tag->HasAttribute(ATTR_MULTIPLE);

    if (!select_is_multiple)
    {
        select_str = Strnew("<pre_int>");
        if (w3mApp::Instance().displayLinkNumber)
            select_str->Push(GetLinkNumberStr(0));
        select_str->Push(Sprintf("[<input_alt hseq=\"%d\" "
                                 "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
                                 Increment(), cur_form_id(), html_quote(p), n_select));
        select_str->Push(">");
        select_option[n_select] = {};
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
            if (!(tag = parse_tag(&p, false)))
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
            select_str->Push(textfieldrep(Strnew(sitem.label), cur_option_maxwidth));
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
    if (cur_select == nullptr || cur_option == nullptr)
        return;

    char begin_char = '[', end_char = ']';
    while (cur_option->Size() > 0 && IS_SPACE(cur_option->Back()))
        cur_option->Pop(1);
    if (cur_option_value == nullptr)
        cur_option_value = cur_option;
    if (cur_option_label == nullptr)
        cur_option_label = cur_option;

    if (!select_is_multiple)
    {
        int len = get_Str_strwidth(cur_option_label);
        if (len > cur_option_maxwidth)
            cur_option_maxwidth = len;
        select_option[n_select].addSelectOption(cur_option_value->ptr, cur_option_label->ptr, cur_option_selected);
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
        tmp = FormOpen(parse_tag(&s, true));
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
        if (w3mApp::Instance().displayLinkNumber)
            tmp->Push(GetLinkNumberStr(0));
        tmp->Push('[');
        break;
    case FORM_INPUT_RADIO:
        if (w3mApp::Instance().displayLinkNumber)
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
            if (w3mApp::Instance().displayLinkNumber)
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
    int pre_int = false, ext_pre_int = false;
    Str tmp = Strnew();

    if (!tag->TryGetAttributeValue(ATTR_SRC, &p))
        return tmp;
    p = remove_space(p);
    q = nullptr;
    tag->TryGetAttributeValue(ATTR_ALT, &q);
    if (!w3mApp::Instance().pseudoInlines && (q == nullptr || (*q == '\0' && w3mApp::Instance().ignore_null_img_alt)))
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
        ext_pre_int = true;

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
        auto tmp2 = FormOpen(parse_tag(&s, true));
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

    if (use_image)
    {
        w0 = w;
        i0 = i;
        if (w < 0 || i < 0)
        {
            auto u = URL::Parse(wc_conv(p, w3mApp::Instance().InnerCharset, CES())->ptr, nullptr);
            Image image;
            image.url = u.ToStr()->ptr;
            auto [t, ext] = uncompressed_file_type(u.path);
            if (t.size())
            {
                image.ext = ext.data();
            }
            else
            {
                image.ext = filename_extension(u.path.c_str(), true);
            }
            image.cache = nullptr;
            image.width = w;
            image.height = i;

            image.cache = getImage(&image, nullptr, IMG_FLAG_SKIP);
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
        pre_int = true;
    }
    else
    {
        if (w < 0)
            w = 12 * w3mApp::Instance().pixel_per_char;
        nw = w ? (int)((w - 1) / w3mApp::Instance().pixel_per_char + 1) : 1;
        if (r)
        {
            tmp->Push("<pre_int>");
            pre_int = true;
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
    if (q != nullptr && *q == '\0' && w3mApp::Instance().ignore_null_img_alt)
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
                        pre_int = true;
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
                pre_int = true;
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

Str HtmlContext::process_anchor(struct parsed_tag *tag, const char *tagbuf)
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
        tmp = FormOpen(parse_tag(&s, true));
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
    ignore_nl_textarea = true;

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

/* end of processing for one line */
bool HtmlContext::EndLineAddBuffer()
{
    if (internal == HTML_UNKNOWN)
    {
        // add non html line
        return true;
    }

    // not add html line
    if (internal == HTML_N_INTERNAL)
    {
        // exit internal
        internal = HTML_UNKNOWN;
    }
    return false;
}

void HtmlContext::Process(parsed_tag *tag, BufferPtr buf, int pos, const char *str)
{
    switch (tag->tagid)
    {
    case HTML_B:
        this->effect |= PE_BOLD;
        break;
    case HTML_N_B:
        this->effect &= ~PE_BOLD;
        break;
    case HTML_I:
        this->ex_effect |= PE_EX_ITALIC;
        break;
    case HTML_N_I:
        this->ex_effect &= ~PE_EX_ITALIC;
        break;
    case HTML_INS:
        this->ex_effect |= PE_EX_INSERT;
        break;
    case HTML_N_INS:
        this->ex_effect &= ~PE_EX_INSERT;
        break;
    case HTML_U:
        this->effect |= PE_UNDER;
        break;
    case HTML_N_U:
        this->effect &= ~PE_UNDER;
        break;
    case HTML_S:
        this->ex_effect |= PE_EX_STRIKE;
        break;
    case HTML_N_S:
        this->ex_effect &= ~PE_EX_STRIKE;
        break;
    case HTML_A:
    {
        char *p;
        if (renderFrameSet &&
            tag->TryGetAttributeValue(ATTR_FRAMENAME, &p))
        {
            p = wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
            if (!this->idFrame || strcmp(this->idFrame->body->name, p))
            {
                this->idFrame = search_frame(renderFrameSet, p);
                if (this->idFrame && this->idFrame->body->attr != F_BODY)
                    this->idFrame = nullptr;
            }
        }
        p = nullptr;
        auto q = Strnew(buf->baseTarget)->ptr;
        char *id = nullptr;
        if (tag->TryGetAttributeValue(ATTR_NAME, &id))
        {
            id = wc_conv_strict(id, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
            buf->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
        }
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                               buf->document_charset)
                    ->ptr;
        if (tag->TryGetAttributeValue(ATTR_TARGET, &q))
            q = wc_conv_strict(q, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
        char *r = nullptr;
        if (tag->TryGetAttributeValue(ATTR_REFERER, &r))
            r = wc_conv_strict(r, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
        char *s = nullptr;
        tag->TryGetAttributeValue(ATTR_TITLE, &s);
        auto t = "";
        tag->TryGetAttributeValue(ATTR_ACCESSKEY, &t);
        auto hseq = 0;
        tag->TryGetAttributeValue(ATTR_HSEQ, &hseq);
        if (hseq > 0)
            buf->putHmarker(currentLn(buf), pos, hseq - 1);
        else if (hseq < 0)
        {
            int h = -hseq - 1;
            if (buf->hmarklist.size() &&
                h < buf->hmarklist.size() &&
                buf->hmarklist[h].invalid)
            {
                buf->hmarklist[h].pos = pos;
                buf->hmarklist[h].line = currentLn(buf);
                buf->hmarklist[h].invalid = 0;
                hseq = -hseq;
            }
        }
        if (id && this->idFrame)
        {
            auto a = Anchor::CreateName(id, currentLn(buf), pos);
            this->idFrame->body->nameList.Put(a);
        }
        if (p)
        {
            // TODO:
            // r ? r : ""
            HttpReferrerPolicy referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin;

            this->effect |= PE_ANCHOR;
            this->a_href = buf->href.Put(Anchor::CreateHref(p,
                                                            q ? q : "",
                                                            referer,
                                                            s ? s : "",
                                                            *t, currentLn(buf), pos));
            this->a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
            this->a_href->slave = (hseq > 0) ? false : true;
        }
        break;
    }
    case HTML_N_A:
        this->effect &= ~PE_ANCHOR;
        if (this->a_href)
        {
            this->a_href->end.line = currentLn(buf);
            this->a_href->end.pos = pos;
            if (this->a_href->start == this->a_href->end)
            {
                if (buf->hmarklist.size() &&
                    this->a_href->hseq < buf->hmarklist.size())
                    buf->hmarklist[this->a_href->hseq].invalid = 1;
                this->a_href->hseq = -1;
            }
            this->a_href = nullptr;
        }
        break;

    case HTML_LINK:
        buf->linklist.push_back(Link::create(*tag, buf->document_charset));
        break;

    case HTML_IMG_ALT:
    {
        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_SRC, &p))
        {
            int w = -1, h = -1, iseq = 0, ismap = 0;
            int xoffset = 0, yoffset = 0, top = 0, bottom = 0;
            tag->TryGetAttributeValue(ATTR_HSEQ, &iseq);
            tag->TryGetAttributeValue(ATTR_WIDTH, &w);
            tag->TryGetAttributeValue(ATTR_HEIGHT, &h);
            tag->TryGetAttributeValue(ATTR_XOFFSET, &xoffset);
            tag->TryGetAttributeValue(ATTR_YOFFSET, &yoffset);
            tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &top);
            tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &bottom);
            if (tag->HasAttribute(ATTR_ISMAP))
                ismap = 1;
            char *q = nullptr;
            tag->TryGetAttributeValue(ATTR_USEMAP, &q);
            if (iseq > 0)
            {
                buf->putHmarker(currentLn(buf), pos, iseq - 1);
            }

            char *s = nullptr;
            tag->TryGetAttributeValue(ATTR_TITLE, &s);
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                               buf->document_charset)
                    ->ptr;
            this->a_img = buf->img.Put(Anchor::CreateImage(
                p,
                s ? s : "",
                currentLn(buf), pos));

            this->a_img->hseq = iseq;
            this->a_img->image = nullptr;
            if (iseq > 0)
            {
                auto u = URL::Parse(this->a_img->url, nullptr);
                Image *image;
                this->a_img->image = image = New(Image);
                image->url = u.ToStr()->ptr;
                auto [t, ext] = uncompressed_file_type(u.path);
                if (t.size())
                {
                    image->ext = ext.data();
                }
                else
                {
                    image->ext = filename_extension(u.path.c_str(), true);
                }
                image->cache = nullptr;
                image->width =
                    (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
                image->height =
                    (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
                image->xoffset = xoffset;
                image->yoffset = yoffset;
                image->y = currentLn(buf) - top;
                if (image->xoffset < 0 && pos == 0)
                    image->xoffset = 0;
                if (image->yoffset < 0 && image->y == 1)
                    image->yoffset = 0;
                image->rows = 1 + top + bottom;
                image->map = q;
                image->ismap = ismap;
                image->touch = 0;
                image->cache = getImage(image, nullptr, IMG_FLAG_SKIP);
            }
            else if (iseq < 0)
            {
                auto a = buf->img.RetrieveAnchor(buf->imarklist[-iseq - 1]);
                if (a)
                {
                    this->a_img->url = a->url;
                    this->a_img->image = a->image;
                }
            }
        }
        this->effect |= PE_IMAGE;
        break;
    }
    case HTML_N_IMG_ALT:
        this->effect &= ~PE_IMAGE;
        if (this->a_img)
        {
            this->a_img->end.line = currentLn(buf);
            this->a_img->end.pos = pos;
        }
        this->a_img = nullptr;
        break;
    case HTML_INPUT_ALT:
    {
        auto hseq = 0;
        tag->TryGetAttributeValue(ATTR_HSEQ, &hseq);
        auto form_id = -1;
        tag->TryGetAttributeValue(ATTR_FID, &form_id);
        int top = 0, bottom = 0;
        tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &top);
        tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &bottom);

        auto form = FormCurrent(form_id);
        if (!form)
        {
            break;
        }

        if (hseq > 0)
        {
            int hpos = pos;
            if (*str == '[')
                hpos++;
            buf->putHmarker(currentLn(buf), hpos, hseq - 1);
        }
        if (form->target.empty())
            form->target = buf->baseTarget;

        int textareanumber = -1;
        if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &textareanumber))
        {
            TextareaGrow(textareanumber);
        }

        int selectnumber = -1;
        if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &selectnumber))
        {
            FormSelectGrow(selectnumber);
        }

        auto fi = formList_addInput(form, tag, this);
        if (fi)
        {
            Anchor a;
            a.target = form->target;
            a.item = fi;
            BufferPoint bp = {
                line : currentLn(buf),
                pos : pos
            };
            a.start = bp;
            a.end = bp;
            this->a_form = buf->formitem.Put(a);
        }
        else
        {
            this->a_form = nullptr;
        }

        if (textareanumber >= 0)
        {
            if (a_textarea.size() < textareanumber + 1)
            {
                a_textarea.resize(textareanumber + 1);
            }
            a_textarea[textareanumber] = this->a_form;
        }

        if (selectnumber >= 0)
        {
            if (a_select.size() < selectnumber + 1)
            {
                a_select.resize(selectnumber + 1);
            }
            a_select[selectnumber] = this->a_form;
        }

        if (this->a_form)
        {
            this->a_form->hseq = hseq - 1;
            this->a_form->y = currentLn(buf) - top;
            this->a_form->rows = 1 + top + bottom;
            if (!tag->HasAttribute(ATTR_NO_EFFECT))
                this->effect |= PE_FORM;
            break;
        }
    }
    case HTML_N_INPUT_ALT:
        this->effect &= ~PE_FORM;
        if (this->a_form)
        {
            this->a_form->end.line = currentLn(buf);
            this->a_form->end.pos = pos;
            if (this->a_form->start.line == this->a_form->end.line &&
                this->a_form->start.pos == this->a_form->end.pos)
                this->a_form->hseq = -1;
        }
        this->a_form = nullptr;
        break;
    case HTML_MAP:
    {
        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_NAME, &p))
        {
            MapList *m = New(MapList);
            m->name = Strnew(p);
            m->area = newGeneralList();
            m->next = buf->maplist;
            buf->maplist = m;
        }
        break;
    }
    case HTML_N_MAP:
        /* nothing to do */
        break;
    case HTML_AREA:
    {
        if (buf->maplist == nullptr) /* outside of <map>..</map> */
            break;

        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
        {
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                               buf->document_charset)
                    ->ptr;
            char *t = nullptr;
            tag->TryGetAttributeValue(ATTR_TARGET, &t);
            auto q = "";
            tag->TryGetAttributeValue(ATTR_ALT, &q);
            char *r = nullptr;
            tag->TryGetAttributeValue(ATTR_SHAPE, &r);
            char *s = nullptr;
            tag->TryGetAttributeValue(ATTR_COORDS, &s);
            auto a = newMapArea(p, t, q, r, s);
            pushValue(buf->maplist->area, (void *)a);
        }
        break;
    }

    //
    // frame
    //
    case HTML_FRAMESET:
    {
        auto frame = newFrameSet(tag);
        if (frame)
        {
            auto sp = this->frameset_s.size();
            this->frameset_s.push_back(frame);
            if (sp == 0)
            {
                if (buf->frameset == nullptr)
                {
                    buf->frameset = frame;
                }
                else
                {
                    pushFrameTree(&(buf->frameQ), frame, nullptr);
                }
            }
            else
            {
                addFrameSetElement(this->frameset_s[sp - 1], *(union frameset_element *)frame);
            }
        }
        break;
    }
    case HTML_N_FRAMESET:
        if (!this->frameset_s.empty())
        {
            this->frameset_s.pop_back();
        }
        break;
    case HTML_FRAME:
        if (!this->frameset_s.empty())
        {
            union frameset_element element;
            element.body = newFrame(tag, buf);
            addFrameSetElement(this->frameset_s.back(), element);
        }
        break;

    case HTML_BASE:
    {
        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
        {
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                               buf->document_charset)
                    ->ptr;
            buf->baseURL = URL::Parse(p, nullptr);
        }
        if (tag->TryGetAttributeValue(ATTR_TARGET, &p))
            buf->baseTarget =
                wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
        break;
    }
    case HTML_META:
    {
        char *p = nullptr;
        tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &p);
        char *q = nullptr;
        tag->TryGetAttributeValue(ATTR_CONTENT, &q);
        if (p && q && !strcasecmp(p, "refresh") && w3mApp::Instance().MetaRefresh)
        {
            Str tmp = nullptr;
            int refresh_interval = getMetaRefreshParam(q, &tmp);

            if (tmp)
            {
                p = wc_conv_strict(remove_space(tmp->ptr), w3mApp::Instance().InnerCharset,
                                   buf->document_charset)
                        ->ptr;
                buf->event = setAlarmEvent(buf->event,
                                           refresh_interval,
                                           AL_IMPLICIT_ONCE,
                                           &gorURL, p);
            }
            else if (refresh_interval > 0)
                buf->event = setAlarmEvent(buf->event,
                                           refresh_interval,
                                           AL_IMPLICIT,
                                           &reload, nullptr);
        }
        break;
    }
    case HTML_INTERNAL:
        this->internal = HTML_INTERNAL;
        break;
    case HTML_N_INTERNAL:
        this->internal = HTML_N_INTERNAL;
        break;
    case HTML_FORM_INT:
    {
        int form_id;
        if (tag->TryGetAttributeValue(ATTR_FID, &form_id))
            FormOpen(tag, form_id);
        break;
    }
    case HTML_TEXTAREA_INT:
    {
        int n_textarea = -1;
        if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &n_textarea))
        {
            Textarea(n_textarea, Strnew());
        }
        break;
    }
    case HTML_N_TEXTAREA_INT:
    {
        auto [n, t] = TextareaCurrent();
        auto anchor = a_textarea[n];
        if (anchor)
        {
            FormItemList *item = anchor->item;
            item->init_value = item->value = t->ptr;
        }
        break;
    }
    case HTML_SELECT_INT:
    {
        int n_select = -1;
        if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &n_select))
        {
            FormSetSelect(n_select);
        }
        break;
    }
    case HTML_N_SELECT_INT:
    {
        auto [n_select, select] = FormSelectCurrent();
        if (select)
        {
            FormItemList *item = a_select[n_select]->item;
            item->select_option = select->first;
            chooseSelectOption(item, item->select_option);
            item->init_selected = item->selected;
            item->init_value = item->value;
            item->init_label = item->label;
        }
        break;
    }
    case HTML_OPTION_INT:
    {
        auto [n_select, select] = FormSelectCurrent();
        if (select)
        {
            auto q = "";
            tag->TryGetAttributeValue(ATTR_LABEL, &q);
            auto p = q;
            tag->TryGetAttributeValue(ATTR_VALUE, &p);
            auto selected = tag->HasAttribute(ATTR_SELECTED);
            select->addSelectOption(p, q, selected);
        }
        break;
    }
    case HTML_TITLE_ALT:
    {
        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
            buf->buffername = html_unquote(p, w3mApp::Instance().InnerCharset);
        break;
    }
    case HTML_SYMBOL:
    {
        this->effect |= PC_SYMBOL;
        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
            this->symbol = (char)atoi(p);
        break;
    }
    case HTML_N_SYMBOL:
        this->effect &= ~PC_SYMBOL;
        break;
    }

    {
        char *id = nullptr;
        if (tag->TryGetAttributeValue(ATTR_ID, &id))
        {
            id = wc_conv_strict(id, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
            buf->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
        }
        char *p = nullptr;
        if (renderFrameSet &&
            tag->TryGetAttributeValue(ATTR_FRAMENAME, &p))
        {
            p = wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
            if (!this->idFrame || strcmp(this->idFrame->body->name, p))
            {
                this->idFrame = search_frame(renderFrameSet, p);
                if (this->idFrame && this->idFrame->body->attr != F_BODY)
                    this->idFrame = nullptr;
            }
        }
        if (id && this->idFrame)
        {
            auto a = Anchor::CreateName(id, currentLn(buf), pos);
            this->idFrame->body->nameList.Put(a);
        }
    }
}

///
/// 1行ごとに Line の構築と html タグを解釈する
///
void HtmlContext::BufferFromLines(BufferPtr buf, const FeedFunc &feed)
{
    //
    // each line
    //
    Str line = nullptr;
    for (int nlines = 1;; ++nlines)
    {
        if (!line)
        {
            // new line
            line = feed();
            if (!line)
            {
                break;
            }

            auto [n, t] = TextareaCurrent();
            if (n >= 0 && *(line->ptr) != '<')
            { /* halfload */
                t->Push(line);
                continue;
            }

            StripRight(line);
        }

        //
        // each char
        //
        const char *str = line->ptr;
        auto endp = str + line->Size();
        PropertiedString out;
        while (str < endp)
        {
            auto mode = get_mctype(*str);
            if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<')
            {
                // symbol
                auto p = get_width_symbol(SymbolWidth0(), symbol);
                // assert(p.size() > 0);
                int len = get_mclen(p.data());
                mode = get_mctype(p[0]);

                out.push(mode | effect | ex_efct(ex_effect), p[0]);
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    for (int i = 1; len--; ++i)
                    {
                        out.push(mode | effect | ex_efct(ex_effect), p[i]);
                    }
                }
                str += SymbolWidth();
            }
            else if (mode == PC_CTRL || mode == PC_UNDEF)
            {
                // control
                out.push(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str++;
            }
            else if (mode & PC_UNKNOWN)
            {
                // unknown
                out.push(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str += get_mclen(str);
            }
            else if (*str != '<' && *str != '&')
            {
                // multibyte char ?
                int len = get_mclen(str);
                out.push(mode | effect | ex_efct(ex_effect), *(str++));
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--)
                    {
                        out.push(mode | effect | ex_efct(ex_effect), *(str++));
                    }
                }
            }
            else if (*str == '&')
            {
                /* 
                 * & escape processing
                 */
                char *p;
                {
                    auto [pos, view] = getescapecmd(str, w3mApp::Instance().InnerCharset);
                    str = const_cast<char *>(pos);
                    p = const_cast<char *>(view.data());
                }

                while (*p)
                {
                    mode = get_mctype(*p);
                    if (mode == PC_CTRL || mode == PC_UNDEF)
                    {
                        out.push(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p++;
                    }
                    else if (mode & PC_UNKNOWN)
                    {
                        out.push(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p += get_mclen(p);
                    }
                    else
                    {
                        int len = get_mclen(p);
                        out.push(mode | effect | ex_efct(ex_effect), *(p++));
                        if (--len)
                        {
                            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                            while (len--)
                            {
                                out.push(mode | effect | ex_efct(ex_effect), *(p++));
                            }
                        }
                    }
                }
            }
            else
            {
                /* tag processing */
                auto tag = parse_tag(&str, true);
                if (!tag)
                    continue;

                Process(tag, buf, out.len(), str);
            }
        }

        if (EndLineAddBuffer())
        {
            buf->AddNewLine(out, nlines);
        }

        if (str != endp)
        {
            // advance line
            line = line->Substr(str - line->ptr, endp - str);
        }
        else
        {
            // clear for next line
            line = nullptr;
        }
    }

    buf->formlist = FormEnd();

    addMultirowsForm(buf, buf->formitem);
    addMultirowsImg(buf, buf->img);
}

void FormSelectOptionList::addSelectOption(std::string_view value, std::string_view label, bool chk)
{
    auto o = new FormSelectOptionItem;
    if (value.empty())
        value = label;
    o->value = value;
    o->label = svu::strip(label);
    o->checked = chk;
    o->next = NULL;
    if (this->first == NULL)
        this->first = this->last = o;
    else
    {
        this->last->next = o;
        this->last = o;
    }
}
