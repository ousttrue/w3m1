#include <string_view_util.h>
#include "config.h"
#include "html/html_context.h"
#include "ctrlcode.h"
#include "html/tagstack.h"
#include "html/image.h"
#include "html/table.h"
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
#include "stream/network.h"
#include "entity.h"

#define MAX_TABLE 20 /* maximum nest level of table */
static struct table *tables[MAX_TABLE];
struct table_mode table_mode[MAX_TABLE];
static int
table_width(html_feed_environ *h_env, int table_level)
{
    int width;
    if (table_level < 0)
        return 0;
    width = tables[table_level]->total_width;
    if (table_level > 0 || width > 0)
        return width;
    return h_env->limit - h_env->envs.back().indent;
}

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

    cur_select = nullptr;
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
            for (auto ip : so)
            {
                s = Sprintf("<option_int value=\"%s\" label=\"%s\"%s>",
                            html_quote(ip.value.size() ? ip.value : ip.label),
                            html_quote(ip.label),
                            ip.checked ? " selected" : "");
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

    forms[fid] = Form::Create(q, p,
                              r ? r : "",
                              s ? s : "",
                              tg ? tg : "",
                              n ? n : "");
    form_stack.push_back(fid);

    return nullptr;
}

std::vector<FormPtr> &HtmlContext::FormEnd()
{
    return forms;
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
        if (select_option[n_select].size())
        {
            auto sitem = std::make_shared<FormItem>();
            sitem->select_option = select_option[n_select];
            sitem->chooseSelectOption();
            select_str->Push(textfieldrep(Strnew(sitem->label), cur_option_maxwidth));
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
        select_option[n_select].push_back({cur_option_value->ptr,
                                           cur_option_label->ptr,
                                           cur_option_selected});
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
    int use_image = ImageManager::Instance().activeImage && ImageManager::Instance().displayImage;
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
                w = (int)(-width * ImageManager::Instance().pixel_per_char * w / 100 + 0.5);
            else
                w = -1;
        }
#ifdef USE_IMAGE
        if (use_image)
        {
            if (w > 0)
            {
                w = (int)(w * ImageManager::Instance().image_scale / 100 + 0.5);
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
                i = (int)(i * ImageManager::Instance().image_scale / 100 + 0.5);
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
                w = 8 * ImageManager::Instance().pixel_per_char;
            if (i < 0)
                i = ImageManager::Instance().pixel_per_line;
        }
        nw = (w > 3) ? (int)((w - 3) / ImageManager::Instance().pixel_per_char + 1) : 1;
        ni = (i > 3) ? (int)((i - 3) / ImageManager::Instance().pixel_per_line + 1) : 1;
        tmp->Push(
            Sprintf("<pre_int><img_alt hseq=\"%d\" src=\"", cur_iseq++));
        pre_int = true;
    }
    else
    {
        if (w < 0)
            w = 12 * ImageManager::Instance().pixel_per_char;
        nw = w ? (int)((w - 1) / ImageManager::Instance().pixel_per_char + 1) : 1;
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
                yoffset = (int)(((ni + 1) * ImageManager::Instance().pixel_per_line - i) / 2);
            else
                yoffset = (int)((ni * ImageManager::Instance().pixel_per_line - i) / 2);
            break;
        case ALIGN_BOTTOM:
            top = ni - 1;
            bottom = 0;
            yoffset = (int)(ni * ImageManager::Instance().pixel_per_line - i);
            break;
        default:
            top = ni - 1;
            bottom = 0;
            if (ni == 1 && ni * ImageManager::Instance().pixel_per_line > i)
                yoffset = 0;
            else
            {
                yoffset = (int)(ni * ImageManager::Instance().pixel_per_line - i);
                if (yoffset <= -2)
                    yoffset++;
            }
            break;
        }
        xoffset = (int)((nw * ImageManager::Instance().pixel_per_char - w) / 2);
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
                    push_symbol(tmp, IMG_SYMBOL, Terminal::SymbolWidth(), 1);
                    n = Terminal::SymbolWidth();
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
            w = w / ImageManager::Instance().pixel_per_char / Terminal::SymbolWidth();
            if (w <= 0)
                w = 1;
            push_symbol(tmp, HR_SYMBOL, Terminal::SymbolWidth(), w);
            n = w * Terminal::SymbolWidth();
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
    if (n_textarea >= textarea_str.size())
    {
        textarea_str.resize(n_textarea + 1);
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
        char *p = nullptr;
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
        if (p)
        {
            // TODO:
            // r ? r : ""
            HttpReferrerPolicy referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin;

            this->effect |= PE_ANCHOR;
            this->a_href = Anchor::CreateHref(p,
                                              q ? q : "",
                                              referer,
                                              s ? s : "",
                                              *t, currentLn(buf), pos);
            buf->href.Put(this->a_href);
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
            this->a_img = Anchor::CreateImage(
                p,
                s ? s : "",
                currentLn(buf), pos);
            buf->img.Put(this->a_img);

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
            // TextareaGrow(textareanumber);
        }

        int selectnumber = -1;
        if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &selectnumber))
        {
            // FormSelectGrow(selectnumber);
        }

        auto fi = formList_addInput(form, tag, this);
        if (fi)
        {
            auto a = std::make_shared<Anchor>();
            a->target = form->target;
            a->item = fi;
            BufferPoint bp = {
                line : currentLn(buf),
                pos : pos
            };
            a->start = bp;
            a->end = bp;
            this->a_form = a;
            buf->formitem.Put(a);
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
            auto m = std::make_shared<MapList>();
            buf->maplist.push_back(m);
            m->name = p;
        }
        break;
    }
    case HTML_N_MAP:
        /* nothing to do */
        break;
    case HTML_AREA:
    {
        if (buf->maplist.empty()) /* outside of <map>..</map> */
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
            buf->maplist.back()->area.push_back(a);
        }
        break;
    }

        //
        // frame
        //
        // case HTML_FRAMESET:
        // {
        //     auto frame = newFrameSet(tag);
        //     if (frame)
        //     {
        //         auto sp = this->frameset_s.size();
        //         this->frameset_s.push_back(frame);
        //         if (sp == 0)
        //         {
        //             if (buf->frameset == nullptr)
        //             {
        //                 buf->frameset = frame;
        //             }
        //             else
        //             {
        //                 pushFrameTree(&(buf->frameQ), frame, nullptr);
        //             }
        //         }
        //         else
        //         {
        //             addFrameSetElement(this->frameset_s[sp - 1], *(union frameset_element *)frame);
        //         }
        //     }
        //     break;
        // }
        // case HTML_N_FRAMESET:
        //     if (!this->frameset_s.empty())
        //     {
        //         this->frameset_s.pop_back();
        //     }
        //     break;
        // case HTML_FRAME:
        //     if (!this->frameset_s.empty())
        //     {
        //         union frameset_element element;
        //         element.body = newFrame(tag, buf);
        //         addFrameSetElement(this->frameset_s.back(), element);
        //     }
        //     break;

    case HTML_BASE:
    {
        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
        {
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
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
        if (p && q && !strcasecmp(p, "refresh") && Network::Instance().MetaRefresh)
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
            auto item = anchor->item;
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
            auto item = a_select[n_select]->item;
            item->select_option = *select;
            item->chooseSelectOption();
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
            select->push_back({
                p,
                q,
                selected,
            });
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
                auto p = get_width_symbol(Terminal::SymbolWidth0(), symbol);
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
                str += Terminal::SymbolWidth();
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

void HtmlContext::close_anchor(struct html_feed_environ *h_env, struct readbuffer *obuf)
{
    if (obuf->anchor.url.size())
    {
        int i;
        char *p = NULL;
        int is_erased = 0;

        for (i = obuf->tag_sp - 1; i >= 0; i--)
        {
            if (obuf->tag_stack[i]->cmd == HTML_A)
                break;
        }
        if (i < 0 && obuf->anchor.hseq > 0 && obuf->line->Back() == ' ')
        {
            obuf->line->Pop(1);
            obuf->pos--;
            is_erased = 1;
        }

        if (i >= 0 || (p = obuf->has_hidden_link(HTML_A)))
        {
            if (obuf->anchor.hseq > 0)
            {
                this->HTMLlineproc0(ANSP, h_env, true);
                obuf->set_space_to_prevchar();
            }
            else
            {
                if (i >= 0)
                {
                    obuf->tag_sp--;
                    bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
                          (obuf->tag_sp - i) * sizeof(struct cmdtable *));
                }
                else
                {
                    obuf->passthrough(p, 1);
                }
                obuf->anchor = {};
                return;
            }
            is_erased = 0;
        }
        if (is_erased)
        {
            obuf->line->Push(' ');
            obuf->pos++;
        }

        obuf->push_tag("</a>", HTML_N_A);
    }
    obuf->anchor = {};
}

void HtmlContext::CLOSE_P(readbuffer *obuf, html_feed_environ *h_env)
{
    if (obuf->flag & RB_P)
    {
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        obuf->RB_RESTORE_FLAG();
        obuf->flag &= ~RB_P;
    }
}

void HtmlContext::CLOSE_DT(readbuffer *obuf, html_feed_environ *h_env)
{
    if (obuf->flag & RB_IN_DT)
    {
        obuf->flag &= ~RB_IN_DT;
        this->HTMLlineproc0("</b>", h_env, true);
    }
}

void HtmlContext::completeHTMLstream(struct html_feed_environ *h_env, struct readbuffer *obuf)
{
    this->close_anchor(h_env, obuf);
    if (obuf->img_alt)
    {
        obuf->push_tag("</img_alt>", HTML_N_IMG_ALT);
        obuf->img_alt = NULL;
    }
    if (obuf->fontstat.in_bold)
    {
        obuf->push_tag("</b>", HTML_N_B);
        obuf->fontstat.in_bold = 0;
    }
    if (obuf->fontstat.in_italic)
    {
        obuf->push_tag("</i>", HTML_N_I);
        obuf->fontstat.in_italic = 0;
    }
    if (obuf->fontstat.in_under)
    {
        obuf->push_tag("</u>", HTML_N_U);
        obuf->fontstat.in_under = 0;
    }
    if (obuf->fontstat.in_strike)
    {
        obuf->push_tag("</s>", HTML_N_S);
        obuf->fontstat.in_strike = 0;
    }
    if (obuf->fontstat.in_ins)
    {
        obuf->push_tag("</ins>", HTML_N_INS);
        obuf->fontstat.in_ins = 0;
    }
    if (obuf->flag & RB_INTXTA)
        this->HTMLlineproc0("</textarea>", h_env, true);
    /* for unbalanced select tag */
    if (obuf->flag & RB_INSELECT)
        this->HTMLlineproc0("</select>", h_env, true);
    if (obuf->flag & RB_TITLE)
        this->HTMLlineproc0("</title>", h_env, true);

    /* for unbalanced table tag */
    if (obuf->table_level >= MAX_TABLE)
        obuf->table_level = MAX_TABLE - 1;

    while (obuf->table_level >= 0)
    {
        table_mode[obuf->table_level].pre_mode &= ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
        this->HTMLlineproc0("</table>", h_env, true);
    }
}

static int REAL_WIDTH(int w, int limit)
{
    return (((w) >= 0) ? (int)((w) / ImageManager::Instance().pixel_per_char) : -(w) * (limit) / 100);
}

Str HtmlContext::process_hr(struct parsed_tag *tag, int width, int indent_width)
{
    Str tmp = Strnew("<nobr>");
    int w = 0;
    int x = ALIGN_CENTER;
#define HR_ATTR_WIDTH_MAX 65535

    if (width > indent_width)
        width -= indent_width;
    if (tag->TryGetAttributeValue(ATTR_WIDTH, &w))
    {
        if (w > HR_ATTR_WIDTH_MAX)
        {
            w = HR_ATTR_WIDTH_MAX;
        }
        w = REAL_WIDTH(w, width);
    }
    else
    {
        w = width;
    }

    tag->TryGetAttributeValue(ATTR_ALIGN, &x);
    switch (x)
    {
    case ALIGN_CENTER:
        tmp->Push("<div_int align=center>");
        break;
    case ALIGN_RIGHT:
        tmp->Push("<div_int align=right>");
        break;
    case ALIGN_LEFT:
        tmp->Push("<div_int align=left>");
        break;
    }
    w /= Terminal::SymbolWidth();
    if (w <= 0)
        w = 1;
    push_symbol(tmp, HR_SYMBOL, Terminal::SymbolWidth(), w);
    tmp->Push("</div_int></nobr>");
    return tmp;
}

// HTML processing first pass
//
// * from loadHtmlStream
//
void HtmlContext::HTMLlineproc0(const char *line, struct html_feed_environ *h_env, bool internal)
{
    auto seq = this;
    Lineprop mode;
    HtmlTags cmd;
    struct readbuffer *obuf = h_env->obuf;
    int indent, delta;
    struct parsed_tag *tag;
    struct table *tbl = NULL;
    struct table_mode *tbl_mode = NULL;
    int tbl_width = 0;
    int is_hangul, prev_is_hangul = 0;

#ifdef DEBUG
    if (w3m_debug)
    {
        FILE *f = fopen("zzzproc1", "a");
        fprintf(f, "%c%c%c%c",
                (obuf->flag & RB_PREMODE) ? 'P' : ' ',
                (obuf->table_level >= 0) ? 'T' : ' ',
                (obuf->flag & RB_INTXTA) ? 'X' : ' ',
                (obuf->flag & (RB_SCRIPT | RB_STYLE)) ? 'S' : ' ');
        fprintf(f, "HTMLlineproc0(\"%s\",%d,%lx)\n", line, h_env->limit,
                (unsigned long)h_env);
        fclose(f);
    }
#endif

    auto tokbuf = Strnew();

table_start:
    if (obuf->table_level >= 0)
    {
        int level = std::min((int)obuf->table_level, (int)(MAX_TABLE - 1));
        tbl = tables[level];
        tbl_mode = &table_mode[level];
        tbl_width = table_width(h_env, level);
    }

    while (*line != '\0')
    {
        const char *str;
        int is_tag = false;
        int pre_mode = (obuf->table_level >= 0) ? tbl_mode->pre_mode : obuf->flag;
        int end_tag = (obuf->table_level >= 0) ? tbl_mode->end_tag : obuf->end_tag;

        if (*line == '<' || obuf->status != R_ST_NORMAL)
        {
            /* 
             * Tag processing
             */
            if (obuf->status == R_ST_EOL)
                obuf->status = R_ST_NORMAL;
            else
            {
                read_token(h_env->tagbuf, (char **)&line, &obuf->status,
                           pre_mode & RB_PREMODE, obuf->status != R_ST_NORMAL);
                if (obuf->status != R_ST_NORMAL)
                    return;
            }
            if (h_env->tagbuf->Size() == 0)
                continue;
            str = h_env->tagbuf->ptr;
            if (*str == '<')
            {
                if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
                    is_tag = true;
                else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT |
                                       RB_SCRIPT | RB_STYLE | RB_TITLE)))
                {
                    line = Strnew_m_charp(str + 1, line, NULL)->ptr;
                    str = "&lt;";
                }
            }
        }
        else
        {
            read_token(tokbuf, (char **)&line, &obuf->status, pre_mode & RB_PREMODE, 0);
            if (obuf->status != R_ST_NORMAL) /* R_ST_AMP ? */
                obuf->status = R_ST_NORMAL;
            str = tokbuf->ptr;
        }

        if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT |
                        RB_STYLE | RB_TITLE))
        {
            if (is_tag)
            {
                const char *p = str;
                if ((tag = parse_tag(&p, internal)))
                {
                    if (tag->tagid == end_tag ||
                        (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM) || (pre_mode & RB_TITLE && (tag->tagid == HTML_N_HEAD || tag->tagid == HTML_BODY)))
                        goto proc_normal;
                }
            }
            /* title */
            if (pre_mode & RB_TITLE)
            {
                this->TitleContent(str);
                continue;
            }
            /* select */
            if (pre_mode & RB_INSELECT)
            {
                if (obuf->table_level >= 0)
                    goto proc_normal;
                this->feed_select(str);
                continue;
            }
            if (is_tag)
            {
                char *p;
                if (strncmp(str, "<!--", 4) && (p = strchr(const_cast<char *>(str) + 1, '<')))
                {
                    str = Strnew_charp_n(str, p - str)->ptr;
                    line = Strnew_m_charp(p, line, NULL)->ptr;
                }
                is_tag = false;
            }
            if (obuf->table_level >= 0)
                goto proc_normal;
            /* textarea */
            if (pre_mode & RB_INTXTA)
            {
                this->feed_textarea(str);
                continue;
            }
            /* script */
            if (pre_mode & RB_SCRIPT)
                continue;
            /* style */
            if (pre_mode & RB_STYLE)
                continue;
        }

    proc_normal:
        if (obuf->table_level >= 0)
        {
            /* 
             * within table: in <table>..</table>, all input tokens
             * are fed to the table renderer, and then the renderer
             * makes HTML output.
             */
            switch (seq->feed_table(tbl, str, tbl_mode, tbl_width, internal))
            {
            case 0:
                /* </table> tag */
                obuf->table_level--;
                if (obuf->table_level >= MAX_TABLE - 1)
                    continue;
                tbl->end();
                if (obuf->table_level >= 0)
                {
                    struct table *tbl0 = tables[obuf->table_level];
                    str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
                    tbl0->pushTable(tbl0);
                    tbl = tbl0;
                    tbl_mode = &table_mode[obuf->table_level];
                    tbl_width = table_width(h_env, obuf->table_level);
                    seq->feed_table(tbl, str, tbl_mode, tbl_width, true);
                    continue;
                    /* continue to the next */
                }
                if (obuf->flag & RB_DEL)
                    continue;
                /* all tables have been read */
                if (tbl->vspace > 0 && !(obuf->flag & RB_IGNORE_P))
                {
                    int indent = h_env->envs.back().indent;
                    h_env->flushline(indent, 0, h_env->limit);
                    h_env->do_blankline(obuf, indent, 0, h_env->limit);
                }
                h_env->obuf->save_fonteffect();
                this->renderTable(tbl, tbl_width, h_env);
                h_env->obuf->restore_fonteffect();
                obuf->flag &= ~RB_IGNORE_P;
                if (tbl->vspace > 0)
                {
                    int indent = h_env->envs.back().indent;
                    h_env->do_blankline(obuf, indent, 0, h_env->limit);
                    obuf->flag |= RB_IGNORE_P;
                }
                obuf->set_space_to_prevchar();
                continue;
            case 1:
                /* <table> tag */
                break;
            default:
                continue;
            }
        }

        if (is_tag)
        {
            /*** Beginning of a new tag ***/
            if ((tag = parse_tag(const_cast<const char **>(&str), internal)))
                cmd = tag->tagid;
            else
                continue;
            /* process tags */
            if (seq->HTMLtagproc1(tag, h_env) == 0)
            {
                /* preserve the tag for second-stage processing */
                if (tag->need_reconstruct)
                    h_env->tagbuf = tag->ToStr();
                obuf->push_tag(h_env->tagbuf->ptr, cmd);
            }
            else
            {
                obuf->process_idattr(cmd, tag);
            }

            obuf->bp = {};
            obuf->clear_ignore_p_flag(cmd);
            if (cmd == HTML_TABLE)
                goto table_start;
            else
                continue;
        }

        if (obuf->flag & (RB_DEL | RB_S))
            continue;
        while (*str)
        {
            mode = get_mctype(*str);
            delta = get_mcwidth(str);
            if (obuf->flag & (RB_SPECIAL & ~RB_NOBR))
            {
                char ch = *str;
                if (!(obuf->flag & RB_PLAIN) && (*str == '&'))
                {
                    const char *p = str;
                    auto [pos, ech] = ucs4_from_entity(p);
                    p = pos.data();
                    if (ech == '\n' || ech == '\r')
                    {
                        ch = '\n';
                        str = p - 1;
                    }
                    else if (ech == '\t')
                    {
                        ch = '\t';
                        str = p - 1;
                    }
                }
                if (ch != '\n')
                    obuf->flag &= ~RB_IGNORE_P;
                if (ch == '\n')
                {
                    str++;
                    if (obuf->flag & RB_IGNORE_P)
                    {
                        obuf->flag &= ~RB_IGNORE_P;
                        continue;
                    }
                    if (obuf->flag & RB_PRE_INT)
                        obuf->PUSH(' ');
                    else
                        h_env->flushline(h_env->envs.back().indent, 1, h_env->limit);
                }
                else if (ch == '\t')
                {
                    do
                    {
                        obuf->PUSH(' ');
                    } while ((h_env->envs.back().indent + obuf->pos) % w3mApp::Instance().Tabstop != 0);
                    str++;
                }
                else if (obuf->flag & RB_PLAIN)
                {
                    const char *p = html_quote_char(*str);
                    if (p)
                    {
                        obuf->push_charp(1, p, PC_ASCII);
                        str++;
                    }
                    else
                    {
                        obuf->proc_mchar(1, delta, &str, mode);
                    }
                }
                else
                {
                    if (*str == '&')
                        obuf->proc_escape(&str);
                    else
                        obuf->proc_mchar(1, delta, &str, mode);
                }
                if (obuf->flag & (RB_SPECIAL & ~RB_PRE_INT))
                    continue;
            }
            else
            {
                if (!IS_SPACE(*str))
                    obuf->flag &= ~RB_IGNORE_P;
                if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str))
                {
                    if (*obuf->prevchar->ptr != ' ')
                    {
                        obuf->PUSH(' ');
                    }
                    str++;
                }
                else
                {
                    if (mode == PC_KANJI1)
                        is_hangul = wtf_is_hangul((uint8_t *)str);
                    else
                        is_hangul = 0;
                    if (!w3mApp::Instance().SimplePreserveSpace && mode == PC_KANJI1 &&
                        !is_hangul && !prev_is_hangul &&
                        obuf->pos > h_env->envs.back().indent &&
                        obuf->line->Back() == ' ')
                    {
                        while (obuf->line->Size() >= 2 &&
                               !strncmp(obuf->line->ptr + obuf->line->Size() -
                                            2,
                                        "  ", 2) &&
                               obuf->pos >= h_env->envs.back().indent)
                        {
                            obuf->line->Pop(1);
                            obuf->pos--;
                        }
                        if (obuf->line->Size() >= 3 &&
                            obuf->prev_ctype == PC_KANJI1 &&
                            obuf->line->Back() == ' ' &&
                            obuf->pos >= h_env->envs.back().indent)
                        {
                            obuf->line->Pop(1);
                            obuf->pos--;
                        }
                    }
                    prev_is_hangul = is_hangul;

                    if (*str == '&')
                        obuf->proc_escape(&str);
                    else
                        obuf->proc_mchar(obuf->flag & RB_SPECIAL, delta, &str, mode);
                }
            }
            if (h_env->need_flushline(mode))
            {
                char *bp = obuf->line->ptr + obuf->bp.len();
                char *tp = bp - obuf->bp.tlen();
                int i = 0;

                if (tp > obuf->line->ptr && tp[-1] == ' ')
                    i = 1;

                indent = h_env->envs.back().indent;
                if (obuf->bp.pos() - i > indent)
                {
                    obuf->append_tags();
                    auto line = Strnew(bp);
                    obuf->line->Pop(obuf->line->Size() - obuf->bp.len());
                    obuf->bp.back_to(obuf);
                    h_env->flushline(indent, 0, h_env->limit);
                    this->HTMLlineproc0(line->ptr, h_env, true);
                }
            }
        }
    }
    if (!(obuf->flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT)))
    {
        char *tp;
        int i = 0;

        if (obuf->bp.pos() == obuf->pos)
        {
            tp = &obuf->line->ptr[obuf->bp.len() - obuf->bp.tlen()];
        }
        else
        {
            tp = &obuf->line->ptr[obuf->line->Size()];
        }

        if (tp > obuf->line->ptr && tp[-1] == ' ')
            i = 1;
        indent = h_env->envs.back().indent;
        if (obuf->pos - i > h_env->limit)
        {
            h_env->flushline(indent, 0, h_env->limit);
        }
    }
}

static char roman_num1[] = {
    'i',
    'x',
    'c',
    'm',
    '*',
};

static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static Str romanNum2(int l, int n)
{
    Str s = Strnew();

    switch (n)
    {
    case 1:
    case 2:
    case 3:
        for (; n > 0; n--)
            s->Push(roman_num1[l]);
        break;
    case 4:
        s->Push(roman_num1[l]);
        s->Push(roman_num5[l]);
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        s->Push(roman_num5[l]);
        for (n -= 5; n > 0; n--)
            s->Push(roman_num1[l]);
        break;
    case 9:
        s->Push(roman_num1[l]);
        s->Push(roman_num1[l + 1]);
        break;
    }
    return s;
}

static Str romanNumeral(int n)
{
    Str r = Strnew();

    if (n <= 0)
        return r;
    if (n >= 4000)
    {
        r->Push("**");
        return r;
    }
    r->Push(romanNum2(3, n / 1000));
    r->Push(romanNum2(2, (n % 1000) / 100));
    r->Push(romanNum2(1, (n % 100) / 10));
    r->Push(romanNum2(0, n % 10));

    return r;
}

static Str romanAlphabet(int n)
{
    Str r = Strnew();
    int l;
    char buf[14];

    if (n <= 0)
        return r;

    l = 0;
    while (n)
    {
        buf[l++] = 'a' + (n - 1) % 26;
        n = (n - 1) / 26;
    }
    l--;
    for (; l >= 0; l--)
        r->Push(buf[l]);

    return r;
}

int HtmlContext::HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env)
{
    if (h_env->obuf->flag & RB_PRE)
    {
        switch (tag->tagid)
        {
        case HTML_NOBR:
        case HTML_N_NOBR:
        case HTML_PRE_INT:
        case HTML_N_PRE_INT:
            return 1;
        }
    }

    switch (tag->tagid)
    {
    case HTML_B:
    {
        h_env->obuf->fontstat.in_bold++;
        if (h_env->obuf->fontstat.in_bold > 1)
            return 1;
        return 0;
    }
    case HTML_N_B:
    {
        if (h_env->obuf->fontstat.in_bold == 1 && h_env->obuf->close_effect0(HTML_B))
            h_env->obuf->fontstat.in_bold = 0;
        if (h_env->obuf->fontstat.in_bold > 0)
        {
            h_env->obuf->fontstat.in_bold--;
            if (h_env->obuf->fontstat.in_bold == 0)
                return 0;
        }
        return 1;
    }
    case HTML_I:
    {
        h_env->obuf->fontstat.in_italic++;
        if (h_env->obuf->fontstat.in_italic > 1)
            return 1;
        return 0;
    }
    case HTML_N_I:
    {
        if (h_env->obuf->fontstat.in_italic == 1 && h_env->obuf->close_effect0(HTML_I))
            h_env->obuf->fontstat.in_italic = 0;
        if (h_env->obuf->fontstat.in_italic > 0)
        {
            h_env->obuf->fontstat.in_italic--;
            if (h_env->obuf->fontstat.in_italic == 0)
                return 0;
        }
        return 1;
    }
    case HTML_U:
    {
        h_env->obuf->fontstat.in_under++;
        if (h_env->obuf->fontstat.in_under > 1)
            return 1;
        return 0;
    }
    case HTML_N_U:
    {
        if (h_env->obuf->fontstat.in_under == 1 && h_env->obuf->close_effect0(HTML_U))
            h_env->obuf->fontstat.in_under = 0;
        if (h_env->obuf->fontstat.in_under > 0)
        {
            h_env->obuf->fontstat.in_under--;
            if (h_env->obuf->fontstat.in_under == 0)
                return 0;
        }
        return 1;
    }
    case HTML_EM:
    {
        this->HTMLlineproc0("<i>", h_env, true);
        return 1;
    }
    case HTML_N_EM:
    {
        this->HTMLlineproc0("</i>", h_env, true);
        return 1;
    }
    case HTML_STRONG:
    {
        this->HTMLlineproc0("<b>", h_env, true);
        return 1;
    }
    case HTML_N_STRONG:
    {
        this->HTMLlineproc0("</b>", h_env, true);
        return 1;
    }
    case HTML_Q:
    {
        this->HTMLlineproc0("`", h_env, true);
        return 1;
    }
    case HTML_N_Q:
    {
        this->HTMLlineproc0("'", h_env, true);
        return 1;
    }
    case HTML_P:
    case HTML_N_P:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 1, h_env->limit);
            h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                h_env->limit);
        }
        h_env->obuf->flag |= RB_IGNORE_P;
        if (tag->tagid == HTML_P)
        {
            h_env->obuf->set_alignment(tag);
            h_env->obuf->flag |= RB_P;
        }
        return 1;
    }
    case HTML_BR:
    {
        h_env->flushline(h_env->envs.back().indent, 1, h_env->limit);
        h_env->blank_lines = 0;
        return 1;
    }
    case HTML_H:
    {
        if (!(h_env->obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                h_env->limit);
        }
        this->HTMLlineproc0("<b>", h_env, true);
        h_env->obuf->set_alignment(tag);
        return 1;
    }
    case HTML_N_H:
    {
        this->HTMLlineproc0("</b>", h_env, true);
        if (!(h_env->obuf->flag & RB_PREMODE))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        }
        h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->RB_RESTORE_FLAG();
        this->close_anchor(h_env, h_env->obuf);
        h_env->obuf->flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_UL:
    case HTML_OL:
    case HTML_BLQ:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            if (!(h_env->obuf->flag & RB_PREMODE) &&
                (h_env->envs.empty() || tag->tagid == HTML_BLQ))
                h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                    h_env->limit);
        }
        h_env->PUSH_ENV(tag->tagid);
        if (tag->tagid == HTML_UL || tag->tagid == HTML_OL)
        {
            int count;
            if (tag->TryGetAttributeValue(ATTR_START, &count))
            {
                h_env->envs.back().count = count - 1;
            }
        }
        if (tag->tagid == HTML_OL)
        {
            h_env->envs.back().type = '1';
            char *p;
            if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
            {
                h_env->envs.back().type = (int)*p;
            }
        }
        if (tag->tagid == HTML_UL)
            h_env->envs.back().type = tag->ul_type();
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        return 1;
    }
    case HTML_N_UL:
    case HTML_N_OL:
    case HTML_N_DL:
    case HTML_N_BLQ:
    {
        this->CLOSE_DT(h_env->obuf, h_env);
        this->CLOSE_A(h_env->obuf, h_env);
        if (h_env->envs.size())
        {
            h_env->flushline(h_env->envs.back().indent, 0,
                             h_env->limit);
            h_env->POP_ENV();
            if (!(h_env->obuf->flag & RB_PREMODE) &&
                (h_env->envs.empty() || tag->tagid == HTML_N_DL || tag->tagid == HTML_N_BLQ))
            {
                h_env->do_blankline(h_env->obuf,
                                    h_env->envs.back().indent,
                                    w3mApp::Instance().IndentIncr, h_env->limit);
                h_env->obuf->flag |= RB_IGNORE_P;
            }
        }
        this->close_anchor(h_env, h_env->obuf);
        return 1;
    }
    case HTML_DL:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            if (!(h_env->obuf->flag & RB_PREMODE))
                h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                    h_env->limit);
        }
        h_env->PUSH_ENV(tag->tagid);
        if (tag->HasAttribute(ATTR_COMPACT))
            h_env->envs.back().env = HTML_DL_COMPACT;
        h_env->obuf->flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_LI:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        this->CLOSE_DT(h_env->obuf, h_env);
        if (h_env->envs.size())
        {
            Str num;
            h_env->flushline(
                h_env->envs.back().indent, 0, h_env->limit);
            h_env->envs.back().count++;
            char *p;
            if (tag->TryGetAttributeValue(ATTR_VALUE, &p))
            {
                int count = atoi(p);
                if (count > 0)
                    h_env->envs.back().count = count;
                else
                    h_env->envs.back().count = 0;
            }
            switch (h_env->envs.back().env)
            {
            case HTML_UL:
            {
                h_env->envs.back().type = tag->ul_type(h_env->envs.back().type);
                for (int i = 0; i < w3mApp::Instance().IndentIncr - 3; i++)
                    h_env->obuf->push_charp(1, NBSP, PC_ASCII);
                auto tmp = Strnew();
                switch (h_env->envs.back().type)
                {
                case 'd':
                    push_symbol(tmp, UL_SYMBOL_DISC, Terminal::SymbolWidth(), 1);
                    break;
                case 'c':
                    push_symbol(tmp, UL_SYMBOL_CIRCLE, Terminal::SymbolWidth(), 1);
                    break;
                case 's':
                    push_symbol(tmp, UL_SYMBOL_SQUARE, Terminal::SymbolWidth(), 1);
                    break;
                default:
                    push_symbol(tmp,
                                UL_SYMBOL((h_env->envs.size() - 1) % MAX_UL_LEVEL),
                                Terminal::SymbolWidth(),
                                1);
                    break;
                }
                if (Terminal::SymbolWidth() == 1)
                    h_env->obuf->push_charp(1, NBSP, PC_ASCII);
                h_env->obuf->push_str(Terminal::SymbolWidth(), tmp, PC_ASCII);
                h_env->obuf->push_charp(1, NBSP, PC_ASCII);
                h_env->obuf->set_space_to_prevchar();
                break;
            }
            case HTML_OL:
            {
                if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
                    h_env->envs.back().type = (int)*p;
                switch ((h_env->envs.back().count > 0) ? h_env->envs.back().type : '1')
                {
                case 'i':
                    num = romanNumeral(h_env->envs.back().count);
                    break;
                case 'I':
                    num = romanNumeral(h_env->envs.back().count);
                    ToUpper(num);
                    break;
                case 'a':
                    num = romanAlphabet(h_env->envs.back().count);
                    break;
                case 'A':
                    num = romanAlphabet(h_env->envs.back().count);
                    ToUpper(num);
                    break;
                default:
                    num = Sprintf("%d", h_env->envs.back().count);
                    break;
                }
                if (w3mApp::Instance().IndentIncr >= 4)
                    num->Push(". ");
                else
                    num->Push('.');
                h_env->obuf->push_spaces(1, w3mApp::Instance().IndentIncr - num->Size());
                h_env->obuf->push_str(num->Size(), num, PC_ASCII);
                if (w3mApp::Instance().IndentIncr >= 4)
                    h_env->obuf->set_space_to_prevchar();
                break;
            }
            default:
                h_env->obuf->push_spaces(1, w3mApp::Instance().IndentIncr);
                break;
            }
        }
        else
        {
            h_env->flushline(0, 0, h_env->limit);
        }
        h_env->obuf->flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_DT:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (h_env->envs.empty() ||
            (h_env->envs.back().env != HTML_DL &&
             h_env->envs.back().env != HTML_DL_COMPACT))
        {
            h_env->PUSH_ENV(HTML_DL);
        }
        if (h_env->envs.size())
        {
            h_env->flushline(
                h_env->envs.back().indent, 0, h_env->limit);
        }
        if (!(h_env->obuf->flag & RB_IN_DT))
        {
            this->HTMLlineproc0("<b>", h_env, true);
            h_env->obuf->flag |= RB_IN_DT;
        }
        h_env->obuf->flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_DD:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        this->CLOSE_DT(h_env->obuf, h_env);
        if (h_env->envs.back().env == HTML_DL_COMPACT)
        {
            if (h_env->obuf->pos > h_env->envs.back().indent)
                h_env->flushline(h_env->envs.back().indent, 0,
                                 h_env->limit);
            else
                h_env->obuf->push_spaces(1, h_env->envs.back().indent - h_env->obuf->pos);
        }
        else
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        /* h_env->obuf->flag |= RB_IGNORE_P; */
        return 1;
    }
    case HTML_TITLE:
    {
        this->close_anchor(h_env, h_env->obuf);
        this->TitleOpen(tag);
        h_env->obuf->flag |= RB_TITLE;
        h_env->obuf->end_tag = HTML_N_TITLE;
        return 1;
    }
    case HTML_N_TITLE:
    {
        if (!(h_env->obuf->flag & RB_TITLE))
            return 1;
        h_env->obuf->flag &= ~RB_TITLE;
        h_env->obuf->end_tag = 0;
        auto tmp = this->TitleClose(tag);
        if (tmp)
            this->HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    }
    case HTML_TITLE_ALT:
    {
        char *p;
        if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
            h_env->title = html_unquote(p, w3mApp::Instance().InnerCharset);
        return 0;
    }
    case HTML_FRAMESET:
    {
        h_env->PUSH_ENV(tag->tagid);
        h_env->obuf->push_charp(9, "--FRAME--", PC_ASCII);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        return 0;
    }
    case HTML_N_FRAMESET:
    {
        if (h_env->envs.size())
        {
            h_env->POP_ENV();
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        }
        return 0;
    }
    case HTML_NOFRAMES:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->flag |= (RB_NOFRAMES | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    }
    case HTML_N_NOFRAMES:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->flag &= ~RB_NOFRAMES;
        return 1;
    }
    case HTML_FRAME:
    {
        char *q;
        tag->TryGetAttributeValue(ATTR_SRC, &q);
        char *r;
        tag->TryGetAttributeValue(ATTR_NAME, &r);
        if (q)
        {
            q = html_quote(q);
            h_env->obuf->push_tag(Sprintf("<a hseq=\"%d\" href=\"%s\">", this->Increment(), q)->ptr, HTML_A);
            if (r)
                q = html_quote(r);
            h_env->obuf->push_charp(get_strwidth(q), q, PC_ASCII);
            h_env->obuf->push_tag("</a>", HTML_N_A);
        }
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        return 0;
    }
    case HTML_HR:
    {
        this->close_anchor(h_env, h_env->obuf);
        auto tmp = this->process_hr(tag, h_env->limit, h_env->envs.back().indent);
        this->HTMLlineproc0(tmp->ptr, h_env, true);
        h_env->obuf->set_space_to_prevchar();
        return 1;
    }
    case HTML_PRE:
    {
        auto x = tag->HasAttribute(ATTR_FOR_TABLE);
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            if (!x)
                h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                    h_env->limit);
        }
        else
            h_env->obuf->fillline(h_env->envs.back().indent);
        h_env->obuf->flag |= (RB_PRE | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    }
    case HTML_N_PRE:
    {
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                h_env->limit);
            h_env->obuf->flag |= RB_IGNORE_P;
            h_env->blank_lines++;
        }
        h_env->obuf->flag &= ~RB_PRE;
        this->close_anchor(h_env, h_env->obuf);
        return 1;
    }
    case HTML_PRE_INT:
    {
        auto i = h_env->obuf->line->Size();
        h_env->obuf->append_tags();
        if (!(h_env->obuf->flag & RB_SPECIAL))
        {
            h_env->obuf->bp.set(h_env->obuf, h_env->obuf->line->Size() - i);
        }
        h_env->obuf->flag |= RB_PRE_INT;
        return 0;
    }
    case HTML_N_PRE_INT:
    {
        h_env->obuf->push_tag("</pre_int>", HTML_N_PRE_INT);
        h_env->obuf->flag &= ~RB_PRE_INT;
        if (!(h_env->obuf->flag & RB_SPECIAL) && h_env->obuf->pos > h_env->obuf->bp.pos())
        {
            h_env->obuf->prevchar->CopyFrom("", 0);
            h_env->obuf->prev_ctype = PC_CTRL;
        }
        return 1;
    }
    case HTML_NOBR:
    {
        h_env->obuf->flag |= RB_NOBR;
        h_env->obuf->nobr_level++;
        return 0;
    }
    case HTML_N_NOBR:
    {
        if (h_env->obuf->nobr_level > 0)
            h_env->obuf->nobr_level--;
        if (h_env->obuf->nobr_level == 0)
            h_env->obuf->flag &= ~RB_NOBR;
        return 0;
    }
    case HTML_PRE_PLAIN:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                h_env->limit);
        }
        h_env->obuf->flag |= (RB_PRE | RB_IGNORE_P);
        return 1;
    }
    case HTML_N_PRE_PLAIN:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                h_env->limit);
            h_env->obuf->flag |= RB_IGNORE_P;
        }
        h_env->obuf->flag &= ~RB_PRE;
        return 1;
    }
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                h_env->limit);
        }
        h_env->obuf->flag |= (RB_PLAIN | RB_IGNORE_P);
        switch (tag->tagid)
        {
        case HTML_LISTING:
            h_env->obuf->end_tag = HTML_N_LISTING;
            break;
        case HTML_XMP:
            h_env->obuf->end_tag = HTML_N_XMP;
            break;
        case HTML_PLAINTEXT:
            h_env->obuf->end_tag = MAX_HTMLTAG;
            break;
        }
        return 1;
    }
    case HTML_N_LISTING:
    case HTML_N_XMP:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                h_env->limit);
            h_env->obuf->flag |= RB_IGNORE_P;
        }
        h_env->obuf->flag &= ~RB_PLAIN;
        h_env->obuf->end_tag = 0;
        return 1;
    }
    case HTML_SCRIPT:
    {
        h_env->obuf->flag |= RB_SCRIPT;
        h_env->obuf->end_tag = HTML_N_SCRIPT;
        return 1;
    }
    case HTML_STYLE:
    {
        h_env->obuf->flag |= RB_STYLE;
        h_env->obuf->end_tag = HTML_N_STYLE;
        return 1;
    }
    case HTML_N_SCRIPT:
    {
        h_env->obuf->flag &= ~RB_SCRIPT;
        h_env->obuf->end_tag = 0;
        return 1;
    }
    case HTML_N_STYLE:
    {
        h_env->obuf->flag &= ~RB_STYLE;
        h_env->obuf->end_tag = 0;
        return 1;
    }
    case HTML_A:
    {
        if (h_env->obuf->anchor.url.size())
            this->close_anchor(h_env, h_env->obuf);
        char *p;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
            h_env->obuf->anchor.url = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_TARGET, &p))
            h_env->obuf->anchor.target = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_REFERER, &p))
        {
            // TODO: noreferer
            // h_env->obuf->anchor.referer = Strnew(p)->ptr;
        }
        if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
            h_env->obuf->anchor.title = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_ACCESSKEY, &p))
            h_env->obuf->anchor.accesskey = (unsigned char)*p;

        auto hseq = 0;
        if (tag->TryGetAttributeValue(ATTR_HSEQ, &hseq))
            h_env->obuf->anchor.hseq = hseq;

        if (hseq == 0 && h_env->obuf->anchor.url.size())
        {
            h_env->obuf->anchor.hseq = this->Get();
            auto tmp = this->process_anchor(tag, h_env->tagbuf->ptr);
            h_env->obuf->push_tag(tmp->ptr, HTML_A);
            if (w3mApp::Instance().displayLinkNumber)
                this->HTMLlineproc0(this->GetLinkNumberStr(-1)->ptr, h_env, true);
            return 1;
        }
        return 0;
    }
    case HTML_N_A:
    {
        this->close_anchor(h_env, h_env->obuf);
        return 1;
    }
    case HTML_IMG:
    {
        auto tmp = this->process_img(tag, h_env->limit);
        this->HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    }
    case HTML_IMG_ALT:
    {
        char *p;
        if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            h_env->obuf->img_alt = Strnew(p);

        auto i = 0;
        if (tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &i))
        {
            if (i > h_env->obuf->top_margin)
                h_env->obuf->top_margin = i;
        }
        i = 0;
        if (tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &i))
        {
            if (i > h_env->obuf->bottom_margin)
                h_env->obuf->bottom_margin = i;
        }

        return 0;
    }
    case HTML_N_IMG_ALT:
    {
        if (h_env->obuf->img_alt)
        {
            if (!h_env->obuf->close_effect0(HTML_IMG_ALT))
                h_env->obuf->push_tag("</img_alt>", HTML_N_IMG_ALT);
            h_env->obuf->img_alt = NULL;
        }
        return 1;
    }
    case HTML_INPUT_ALT:
    {
        auto i = 0;
        if (tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &i))
        {
            if (i > h_env->obuf->top_margin)
                h_env->obuf->top_margin = i;
        }
        i = 0;
        if (tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &i))
        {
            if (i > h_env->obuf->bottom_margin)
                h_env->obuf->bottom_margin = i;
        }
        return 0;
    }
    case HTML_TABLE:
    {
        this->close_anchor(h_env, h_env->obuf);
        h_env->obuf->table_level++;
        if (h_env->obuf->table_level >= MAX_TABLE)
            break;
        auto w = BORDER_NONE;
        /* x: cellspacing, y: cellpadding */
        auto x = 2;
        auto y = 1;
        auto z = 0;
        auto width = 0;
        if (tag->HasAttribute(ATTR_BORDER))
        {
            if (tag->TryGetAttributeValue(ATTR_BORDER, &w))
            {
                if (w > 2)
                    w = BORDER_THICK;
                else if (w < 0)
                { /* weird */
                    w = BORDER_THIN;
                }
            }
            else
                w = BORDER_THIN;
        }
        int i;
        if (tag->TryGetAttributeValue(ATTR_WIDTH, &i))
        {
            if (h_env->obuf->table_level == 0)
                width = REAL_WIDTH(i, h_env->limit - h_env->envs.back().indent);
            else
                width = RELATIVE_WIDTH(i);
        }
        if (tag->HasAttribute(ATTR_HBORDER))
            w = BORDER_NOWIN;
        tag->TryGetAttributeValue(ATTR_CELLSPACING, &x);
        tag->TryGetAttributeValue(ATTR_CELLPADDING, &y);
        tag->TryGetAttributeValue(ATTR_VSPACE, &z);
        char *id;
        tag->TryGetAttributeValue(ATTR_ID, &id);
        tables[h_env->obuf->table_level] = table::begin(w, x, y, z);
        if (id != NULL)
            tables[h_env->obuf->table_level]->id = Strnew(id);

        table_mode[h_env->obuf->table_level].pre_mode = TBLM_NONE;
        table_mode[h_env->obuf->table_level].indent_level = 0;
        table_mode[h_env->obuf->table_level].nobr_level = 0;
        table_mode[h_env->obuf->table_level].caption = 0;
        table_mode[h_env->obuf->table_level].end_tag = HTML_UNKNOWN;
#ifndef TABLE_EXPAND
        tables[h_env->obuf->table_level]->total_width = width;
#else
        tables[h_env->obuf->table_level]->real_width = width;
        tables[h_env->obuf->table_level]->total_width = 0;
#endif
        return 1;
    }
    case HTML_N_TABLE:
        /* should be processed in HTMLlineproc() */
        return 1;
    case HTML_CENTER:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->RB_SAVE_FLAG();
        h_env->obuf->RB_SET_ALIGN(RB_CENTER);
        return 1;
    }
    case HTML_N_CENTER:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_PREMODE))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->RB_RESTORE_FLAG();
        return 1;
    }
    case HTML_DIV:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->set_alignment(tag);
        return 1;
    }
    case HTML_N_DIV:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->RB_RESTORE_FLAG();
        return 1;
    }
    case HTML_DIV_INT:
    {
        this->CLOSE_P(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->set_alignment(tag);
        return 1;
    }
    case HTML_N_DIV_INT:
    {
        this->CLOSE_P(h_env->obuf, h_env);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->RB_RESTORE_FLAG();
        return 1;
    }
    case HTML_FORM:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        auto tmp = this->FormOpen(tag);
        if (tmp)
            this->HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    }
    case HTML_N_FORM:
    {
        this->CLOSE_A(h_env->obuf, h_env);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->flag |= RB_IGNORE_P;
        this->FormClose();
        return 1;
    }
    case HTML_INPUT:
    {
        this->close_anchor(h_env, h_env->obuf);
        auto tmp = this->process_input(tag);
        if (tmp)
            this->HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    }
    case HTML_SELECT:
    {
        this->close_anchor(h_env, h_env->obuf);
        auto tmp = this->process_select(tag);
        if (tmp)
            this->HTMLlineproc0(tmp->ptr, h_env, true);
        h_env->obuf->flag |= RB_INSELECT;
        h_env->obuf->end_tag = HTML_N_SELECT;
        return 1;
    }
    case HTML_N_SELECT:
    {
        h_env->obuf->flag &= ~RB_INSELECT;
        h_env->obuf->end_tag = 0;
        auto tmp = this->process_n_select();
        if (tmp)
            this->HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    }
    case HTML_OPTION:
        /* nothing */
        return 1;
    case HTML_TEXTAREA:
    {
        this->close_anchor(h_env, h_env->obuf);
        auto tmp = this->process_textarea(tag, h_env->limit);
        if (tmp)
            this->HTMLlineproc0(tmp->ptr, h_env, true);
        h_env->obuf->flag |= RB_INTXTA;
        h_env->obuf->end_tag = HTML_N_TEXTAREA;
        return 1;
    }
    case HTML_N_TEXTAREA:
    {
        h_env->obuf->flag &= ~RB_INTXTA;
        h_env->obuf->end_tag = 0;
        auto tmp = this->process_n_textarea();
        if (tmp)
            this->HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    }
    case HTML_ISINDEX:
    {
        auto p = "";
        tag->TryGetAttributeValue(ATTR_PROMPT, &p);
        auto q = "!CURRENT_URL!";
        tag->TryGetAttributeValue(ATTR_ACTION, &q);
        auto tmp = Strnew_m_charp("<form method=get action=\"",
                                  html_quote(q),
                                  "\">",
                                  html_quote(p),
                                  "<input type=text name=\"\" accept></form>",
                                  NULL);
        this->HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    }
    case HTML_META:
    {
        char *p;
        tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &p);
        char *q;
        tag->TryGetAttributeValue(ATTR_CONTENT, &q);

        if (p && q && !strcasecmp(p, "Content-Type") &&
            (q = strcasestr(q, "charset")) != NULL)
        {
            q += 7;
            SKIP_BLANKS(&q);
            if (*q == '=')
            {
                q++;
                SKIP_BLANKS(&q);
                this->SetMetaCharset(wc_guess_charset(q, WC_CES_NONE));
            }
        }
        else if (p && q && !strcasecmp(p, "refresh"))
        {
            int refresh_interval;
            Str tmp = NULL;
            refresh_interval = getMetaRefreshParam(q, &tmp);
            if (tmp)
            {
                q = html_quote(tmp->ptr);
                tmp = Sprintf("Refresh (%d sec) <a href=\"%s\">%s</a>",
                              refresh_interval, q, q);
            }
            else if (refresh_interval > 0)
                tmp = Sprintf("Refresh (%d sec)", refresh_interval);
            if (tmp)
            {
                this->HTMLlineproc0(tmp->ptr, h_env, true);
                h_env->do_blankline(h_env->obuf, h_env->envs.back().indent, 0,
                                    h_env->limit);
                if (!w3mApp::Instance().is_redisplay &&
                    !((h_env->obuf->flag & RB_NOFRAMES) && w3mApp::Instance().RenderFrame))
                {
                    tag->need_reconstruct = true;
                    return 0;
                }
            }
        }
        return 1;
    }
    case HTML_BASE:
    {
        char *p = NULL;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
        {
            // *GetCurBaseUrl() = URL::Parse(p, NULL);
        }
    }
    case HTML_MAP:
    case HTML_N_MAP:
    case HTML_AREA:
        return 0;
    case HTML_DEL:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            h_env->obuf->flag |= RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->HTMLlineproc0("<U>[DEL:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            h_env->obuf->fontstat.in_strike++;
            if (h_env->obuf->fontstat.in_strike == 1)
            {
                h_env->obuf->push_tag("<s>", HTML_S);
            }
            break;
        }
        return 1;
    }
    case HTML_N_DEL:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            h_env->obuf->flag &= ~RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->HTMLlineproc0("<U>:DEL]</U>", h_env, true);
        case DISPLAY_INS_DEL_FONTIFY:
            if (h_env->obuf->fontstat.in_strike == 0)
                return 1;
            if (h_env->obuf->fontstat.in_strike == 1 && h_env->obuf->close_effect0(HTML_S))
                h_env->obuf->fontstat.in_strike = 0;
            if (h_env->obuf->fontstat.in_strike > 0)
            {
                h_env->obuf->fontstat.in_strike--;
                if (h_env->obuf->fontstat.in_strike == 0)
                {
                    h_env->obuf->push_tag("</s>", HTML_N_S);
                }
            }
            break;
        }
        return 1;
    }
    case HTML_S:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            h_env->obuf->flag |= RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->HTMLlineproc0("<U>[S:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            h_env->obuf->fontstat.in_strike++;
            if (h_env->obuf->fontstat.in_strike == 1)
            {
                h_env->obuf->push_tag("<s>", HTML_S);
            }
            break;
        }
        return 1;
    }
    case HTML_N_S:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            h_env->obuf->flag &= ~RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->HTMLlineproc0("<U>:S]</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (h_env->obuf->fontstat.in_strike == 0)
                return 1;
            if (h_env->obuf->fontstat.in_strike == 1 && h_env->obuf->close_effect0(HTML_S))
                h_env->obuf->fontstat.in_strike = 0;
            if (h_env->obuf->fontstat.in_strike > 0)
            {
                h_env->obuf->fontstat.in_strike--;
                if (h_env->obuf->fontstat.in_strike == 0)
                {
                    h_env->obuf->push_tag("</s>", HTML_N_S);
                }
            }
        }
        return 1;
    }
    case HTML_INS:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->HTMLlineproc0("<U>[INS:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            h_env->obuf->fontstat.in_ins++;
            if (h_env->obuf->fontstat.in_ins == 1)
            {
                h_env->obuf->push_tag("<ins>", HTML_INS);
            }
            break;
        }
        return 1;
    }
    case HTML_N_INS:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->HTMLlineproc0("<U>:INS]</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (h_env->obuf->fontstat.in_ins == 0)
                return 1;
            if (h_env->obuf->fontstat.in_ins == 1 && h_env->obuf->close_effect0(HTML_INS))
                h_env->obuf->fontstat.in_ins = 0;
            if (h_env->obuf->fontstat.in_ins > 0)
            {
                h_env->obuf->fontstat.in_ins--;
                if (h_env->obuf->fontstat.in_ins == 0)
                {
                    h_env->obuf->push_tag("</ins>", HTML_N_INS);
                }
            }
            break;
        }
        return 1;
    }
    case HTML_SUP:
    {
        if (!(h_env->obuf->flag & (RB_DEL | RB_S)))
            this->HTMLlineproc0("^", h_env, true);
        return 1;
    }
    case HTML_N_SUP:
        return 1;
    case HTML_SUB:
    {
        if (!(h_env->obuf->flag & (RB_DEL | RB_S)))
            this->HTMLlineproc0("[", h_env, true);
        return 1;
    }
    case HTML_N_SUB:
    {
        if (!(h_env->obuf->flag & (RB_DEL | RB_S)))
            this->HTMLlineproc0("]", h_env, true);
        return 1;
    }
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
        return 1;
    case HTML_BGSOUND:
    {
        if (w3mApp::Instance().view_unseenobject)
        {
            char *p;
            if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            {
                auto q = html_quote(p);
                Str s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
                this->HTMLlineproc0(s->ptr, h_env, true);
            }
        }
        return 1;
    }
    case HTML_EMBED:
    {
        if (w3mApp::Instance().view_unseenobject)
        {
            char *p;
            if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            {
                auto q = html_quote(p);
                Str s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
                this->HTMLlineproc0(s->ptr, h_env, true);
            }
        }
        return 1;
    }
    case HTML_APPLET:
    {
        if (w3mApp::Instance().view_unseenobject)
        {
            char *p;
            if (tag->TryGetAttributeValue(ATTR_ARCHIVE, &p))
            {
                auto q = html_quote(p);
                auto s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
                this->HTMLlineproc0(s->ptr, h_env, true);
            }
        }
        return 1;
    }
    case HTML_BODY:
    {
        if (w3mApp::Instance().view_unseenobject)
        {
            char *p;
            if (tag->TryGetAttributeValue(ATTR_BACKGROUND, &p))
            {
                auto q = html_quote(p);
                auto s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
                this->HTMLlineproc0(s->ptr, h_env, true);
            }
        }
        return 1;
    }
    case HTML_N_HEAD:
    {
        if (h_env->obuf->flag & RB_TITLE)
            this->HTMLlineproc0("</title>", h_env, true);
        return 1;
    }
    case HTML_HEAD:
    case HTML_N_BODY:
        return 1;
    default:
        /* h_env->obuf->prevchar = '\0'; */
        return 0;
    }
    /* not reached */
    return 0;
}

bool html_feed_environ::need_flushline(Lineprop mode)
{
    if (this->obuf->flag & RB_PRE_INT)
    {
        if (this->obuf->pos > this->limit)
            return 1;
        else
            return 0;
    }

    auto ch = this->obuf->line->Back();
    /* if (ch == ' ' && this->obuf->tag_sp > 0) */
    if (ch == ' ')
        return 0;

    if (this->obuf->pos > this->limit)
        return 1;

    return 0;
}

void HtmlContext::make_caption(struct table *t, struct html_feed_environ *h_env)
{
    if (t->caption->Size() <= 0)
        return;

    int limit;
    if (t->total_width > 0)
        limit = t->total_width;
    else
        limit = h_env->limit;

    struct readbuffer obuf;
    html_feed_environ henv(&obuf, newTextLineList(), limit, h_env->envs.back().indent);
    this->HTMLlineproc0("<center>", &henv, true);
    this->HTMLlineproc0(t->caption->ptr, &henv, false);
    this->HTMLlineproc0("</center>", &henv, true);

    if (t->total_width < henv.maxlimit)
        t->total_width = henv.maxlimit;
    limit = h_env->limit;
    h_env->limit = t->total_width;
    this->HTMLlineproc0("<center>", h_env, true);
    this->HTMLlineproc0(t->caption->ptr, h_env, false);
    this->HTMLlineproc0("</center>", h_env, true);
    h_env->limit = limit;
}

#define TAG_IS(s, tag, len) (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

void HtmlContext::do_refill(struct table *tbl, int row, int col, int maxlimit)
{
    TextList *orgdata;
    TextListItem *l;
    struct readbuffer obuf;
    int colspan, icell;

    if (tbl->tabdata[row] == NULL || tbl->tabdata[row][col] == NULL)
        return;
    orgdata = (TextList *)tbl->tabdata[row][col];
    tbl->tabdata[row][col] = newGeneralList();

    html_feed_environ h_env(&obuf,
                            (TextLineList *)tbl->tabdata[row][col],
                            tbl->get_spec_cell_width(row, col));
    obuf.flag |= RB_INTABLE;
    if (h_env.limit > maxlimit)
        h_env.limit = maxlimit;
    if (tbl->border_mode != BORDER_NONE && tbl->vcellpadding > 0)
        h_env.do_blankline(&obuf, 0, 0, h_env.limit);
    for (l = orgdata->first; l != NULL; l = l->next)
    {
        if (TAG_IS(l->ptr, "<table_alt", 10))
        {
            int id = -1;
            const char *p = l->ptr;
            struct parsed_tag *tag;
            if ((tag = parse_tag(&p, true)) != NULL)
                tag->TryGetAttributeValue(ATTR_TID, &id);
            if (id >= 0 && id < tbl->ntable)
            {
                TextLineListItem *ti;
                struct table *t = tbl->tables[id].ptr;
                int limit = tbl->tables[id].indent + t->total_width;
                tbl->tables[id].ptr = NULL;
                h_env.obuf->save_fonteffect();
                h_env.flushline(0, 2, h_env.limit);
                if (t->vspace > 0 && !(obuf.flag & RB_IGNORE_P))
                    h_env.do_blankline(&obuf, 0, 0, h_env.limit);

                AlignTypes alignment;
                if (h_env.obuf->RB_GET_ALIGN() == RB_CENTER)
                    alignment = ALIGN_CENTER;
                else if (h_env.obuf->RB_GET_ALIGN() == RB_RIGHT)
                    alignment = ALIGN_RIGHT;
                else
                    alignment = ALIGN_LEFT;

                if (alignment != ALIGN_LEFT)
                {
                    for (ti = tbl->tables[id].buf->first;
                         ti != NULL; ti = ti->next)
                        align(ti->ptr, h_env.limit, alignment);
                }
                appendTextLineList(h_env.buf, tbl->tables[id].buf);
                if (h_env.maxlimit < limit)
                    h_env.maxlimit = limit;
                h_env.obuf->restore_fonteffect();
                obuf.flag &= ~RB_IGNORE_P;
                h_env.blank_lines = 0;
                if (t->vspace > 0)
                {
                    h_env.do_blankline(&obuf, 0, 0, h_env.limit);
                    obuf.flag |= RB_IGNORE_P;
                }
            }
        }
        else
            this->HTMLlineproc0(l->ptr, &h_env, true);
    }
    if (obuf.status != R_ST_NORMAL)
    {
        obuf.status = R_ST_EOL;
        this->HTMLlineproc0("\n", &h_env, true);
    }
    this->completeHTMLstream(&h_env, &obuf);
    h_env.flushline(0, 2, h_env.limit);
    if (tbl->border_mode == BORDER_NONE)
    {
        int rowspan = tbl->table_rowspan(row, col);
        if (row + rowspan <= tbl->maxrow)
        {
            if (tbl->vcellpadding > 0 && !(obuf.flag & RB_IGNORE_P))
                h_env.do_blankline(&obuf, 0, 0, h_env.limit);
        }
        else
        {
            if (tbl->vspace > 0)
                h_env.purgeline();
        }
    }
    else
    {
        if (tbl->vcellpadding > 0)
        {
            if (!(obuf.flag & RB_IGNORE_P))
                h_env.do_blankline(&obuf, 0, 0, h_env.limit);
        }
        else
            h_env.purgeline();
    }
    if ((colspan = tbl->table_colspan(row, col)) > 1)
    {
        struct table_cell *cell = &tbl->cell;
        int k;
        k = bsearch_2short(colspan, cell->colspan, col, cell->col, MAXCOL,
                           cell->index, cell->maxcell + 1);
        icell = cell->index[k];
        if (cell->minimum_width[icell] < h_env.maxlimit)
            cell->minimum_width[icell] = h_env.maxlimit;
    }
    else
    {
        if (tbl->minimum_width[col] < h_env.maxlimit)
            tbl->minimum_width[col] = h_env.maxlimit;
    }
}

#define CASE_TABLE_TAG    \
    case HTML_TABLE:      \
    case HTML_N_TABLE:    \
    case HTML_TR:         \
    case HTML_N_TR:       \
    case HTML_TD:         \
    case HTML_N_TD:       \
    case HTML_TH:         \
    case HTML_N_TH:       \
    case HTML_THEAD:      \
    case HTML_N_THEAD:    \
    case HTML_TBODY:      \
    case HTML_N_TBODY:    \
    case HTML_TFOOT:      \
    case HTML_N_TFOOT:    \
    case HTML_COLGROUP:   \
    case HTML_N_COLGROUP: \
    case HTML_COL

#define ATTR_ROWSPAN_MAX 32766

static void
table_close_select(struct table *tbl, struct table_mode *mode, int width, HtmlContext *seq)
{
    Str tmp = seq->process_n_select();
    mode->pre_mode &= ~TBLM_INSELECT;
    mode->end_tag = HTML_UNKNOWN;
    seq->feed_table1(tbl, tmp, mode, width);
}

static void
table_close_textarea(struct table *tbl, struct table_mode *mode, int width, HtmlContext *seq)
{
    Str tmp = seq->process_n_textarea();
    mode->pre_mode &= ~TBLM_INTXTA;
    mode->end_tag = HTML_UNKNOWN;
    seq->feed_table1(tbl, tmp, mode, width);
}

TagActions HtmlContext::feed_table_tag(struct table *tbl, const char *line, struct table_mode *mode, int width, struct parsed_tag *tag)
{
    int cmd;
#ifdef ID_EXT
    char *p;
#endif
    struct table_cell *cell = &tbl->cell;
    int colspan, rowspan;
    int col, prev_col;
    int i, j, k, v, v0, w, id;
    Str tok, tmp, anchor;
    TableAttributes align, valign;

    cmd = tag->tagid;

    if (mode->pre_mode & TBLM_PLAIN)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_PLAIN;
            mode->end_tag = HTML_UNKNOWN;
            tbl->feed_table_block_tag(line, mode, 0, cmd);
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    if (mode->pre_mode & TBLM_INTXTA)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_TEXTAREA:
            table_close_textarea(tbl, mode, width, this);
            if (cmd == HTML_N_TEXTAREA)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }
    if (mode->pre_mode & TBLM_SCRIPT)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_SCRIPT;
            mode->end_tag = HTML_UNKNOWN;
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    if (mode->pre_mode & TBLM_STYLE)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_STYLE;
            mode->end_tag = HTML_UNKNOWN;
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    /* failsafe: a tag other than <option></option>and </select> in *
     * <select> environment is regarded as the end of <select>. */
    if (mode->pre_mode & TBLM_INSELECT)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_FORM:
        case HTML_N_SELECT: /* mode->end_tag */
            table_close_select(tbl, mode, width, this);
            if (cmd == HTML_N_SELECT)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }
    if (mode->caption)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_CAPTION:
            mode->caption = 0;
            if (cmd == HTML_N_CAPTION)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }

    if (mode->pre_mode & TBLM_PRE)
    {
        switch (cmd)
        {
        case HTML_NOBR:
        case HTML_N_NOBR:
        case HTML_PRE_INT:
        case HTML_N_PRE_INT:
            return TAG_ACTION_NONE;
        }
    }

    switch (cmd)
    {
    case HTML_TABLE:
        tbl->check_rowcol(mode);
        return TAG_ACTION_TABLE;
    case HTML_N_TABLE:
        if (tbl->suspended_data)
            tbl->check_rowcol(mode);
        return TAG_ACTION_N_TABLE;
    case HTML_TR:
        if (tbl->col >= 0 && tbl->tabcontentssize > 0)
            tbl->setwidth(mode);
        tbl->col = -1;
        tbl->row++;
        tbl->flag |= TBL_IN_ROW;
        tbl->flag &= ~TBL_IN_COL;
        align = {};
        valign = {};
        if (tag->TryGetAttributeValue(ATTR_ALIGN, &i))
        {
            switch (i)
            {
            case ALIGN_LEFT:
                align = (HTT_LEFT | HTT_TRSET);
                break;
            case ALIGN_RIGHT:
                align = (HTT_RIGHT | HTT_TRSET);
                break;
            case ALIGN_CENTER:
                align = (HTT_CENTER | HTT_TRSET);
                break;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_VALIGN, &i))
        {
            switch (i)
            {
            case VALIGN_TOP:
                valign = (HTT_TOP | HTT_VTRSET);
                break;
            case VALIGN_MIDDLE:
                valign = (HTT_MIDDLE | HTT_VTRSET);
                break;
            case VALIGN_BOTTOM:
                valign = (HTT_BOTTOM | HTT_VTRSET);
                break;
            }
        }
#ifdef ID_EXT
        if (tag->TryGetAttributeValue(ATTR_ID, &p))
            tbl->tridvalue[tbl->row] = Strnew(p);
#endif /* ID_EXT */
        tbl->trattr = align | valign;
        break;
    case HTML_TH:
    case HTML_TD:
        prev_col = tbl->col;
        if (tbl->col >= 0 && tbl->tabcontentssize > 0)
            tbl->setwidth(mode);
        if (tbl->row == -1)
        {
            /* for broken HTML... */
            tbl->row = -1;
            tbl->col = -1;
            tbl->maxrow = tbl->row;
        }
        if (tbl->col == -1)
        {
            if (!(tbl->flag & TBL_IN_ROW))
            {
                tbl->row++;
                tbl->flag |= TBL_IN_ROW;
            }
            if (tbl->row > tbl->maxrow)
                tbl->maxrow = tbl->row;
        }
        tbl->col++;
        tbl->check_row(tbl->row);
        while (tbl->tabattr[tbl->row][tbl->col])
        {
            tbl->col++;
        }
        if (tbl->col > MAXCOL - 1)
        {
            tbl->col = prev_col;
            return TAG_ACTION_NONE;
        }
        if (tbl->col > tbl->maxcol)
        {
            tbl->maxcol = tbl->col;
        }
        colspan = rowspan = 1;
        if (tbl->trattr & HTT_TRSET)
            align = (tbl->trattr & HTT_ALIGN);
        else if (cmd == HTML_TH)
            align = HTT_CENTER;
        else
            align = HTT_LEFT;
        if (tbl->trattr & HTT_VTRSET)
            valign = (tbl->trattr & HTT_VALIGN);
        else
            valign = HTT_MIDDLE;
        if (tag->TryGetAttributeValue(ATTR_ROWSPAN, &rowspan))
        {
            if (rowspan > ATTR_ROWSPAN_MAX)
            {
                rowspan = ATTR_ROWSPAN_MAX;
            }
            if ((tbl->row + rowspan) >= tbl->max_rowsize)
                tbl->check_row(tbl->row + rowspan);
        }
        if (tag->TryGetAttributeValue(ATTR_COLSPAN, &colspan))
        {
            if ((tbl->col + colspan) >= MAXCOL)
            {
                /* Can't expand column */
                colspan = MAXCOL - tbl->col;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_ALIGN, &i))
        {
            switch (i)
            {
            case ALIGN_LEFT:
                align = HTT_LEFT;
                break;
            case ALIGN_RIGHT:
                align = HTT_RIGHT;
                break;
            case ALIGN_CENTER:
                align = HTT_CENTER;
                break;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_VALIGN, &i))
        {
            switch (i)
            {
            case VALIGN_TOP:
                valign = HTT_TOP;
                break;
            case VALIGN_MIDDLE:
                valign = HTT_MIDDLE;
                break;
            case VALIGN_BOTTOM:
                valign = HTT_BOTTOM;
                break;
            }
        }
#ifdef NOWRAP
        if (tag->HasAttribute(ATTR_NOWRAP))
            tbl->tabattr[tbl->row][tbl->col] |= HTT_NOWRAP;
#endif /* NOWRAP */
        v = 0;
        if (tag->TryGetAttributeValue(ATTR_WIDTH, &v))
        {
#ifdef TABLE_EXPAND
            if (v > 0)
            {
                if (tbl->real_width > 0)
                    v = -(v * 100) / (tbl->real_width * ImageManager::Instance().pixel_per_char);
                else
                    v = (int)(v / ImageManager::Instance().pixel_per_char);
            }
#else
            v = RELATIVE_WIDTH(v);
#endif /* not TABLE_EXPAND */
        }
#ifdef ID_EXT
        if (tag->TryGetAttributeValue(ATTR_ID, &p))
            tbl->tabidvalue[tbl->row][tbl->col] = Strnew(p);
#endif /* ID_EXT */
#ifdef NOWRAP
        if (v != 0)
        {
            /* NOWRAP and WIDTH= conflicts each other */
            tbl->tabattr[tbl->row][tbl->col] &= ~HTT_NOWRAP;
        }
#endif /* NOWRAP */
        tbl->tabattr[tbl->row][tbl->col] &= ~(HTT_ALIGN | HTT_VALIGN);
        tbl->tabattr[tbl->row][tbl->col] |= (align | valign);
        if (colspan > 1)
        {
            col = tbl->col;

            cell->icell = cell->maxcell + 1;
            k = bsearch_2short(colspan, cell->colspan, col, cell->col, MAXCOL,
                               cell->index, cell->icell);
            if (k <= cell->maxcell)
            {
                i = cell->index[k];
                if (cell->col[i] == col && cell->colspan[i] == colspan)
                    cell->icell = i;
            }
            if (cell->icell > cell->maxcell && cell->icell < MAXCELL)
            {
                cell->maxcell++;
                cell->col[cell->maxcell] = col;
                cell->colspan[cell->maxcell] = colspan;
                cell->width[cell->maxcell] = 0;
                cell->minimum_width[cell->maxcell] = 0;
                cell->fixed_width[cell->maxcell] = 0;
                if (cell->maxcell > k)
                {
                    int ii;
                    for (ii = cell->maxcell; ii > k; ii--)
                        cell->index[ii] = cell->index[ii - 1];
                }
                cell->index[k] = cell->maxcell;
            }
            if (cell->icell > cell->maxcell)
                cell->icell = -1;
        }
        if (v != 0)
        {
            if (colspan == 1)
            {
                v0 = tbl->fixed_width[tbl->col];
                if (v0 == 0 || (v0 > 0 && v > v0) || (v0 < 0 && v < v0))
                {
#ifdef FEED_TABLE_DEBUG
                    fprintf(stderr, "width(%d) = %d\n", tbl->col, v);
#endif /* TABLE_DEBUG */
                    tbl->fixed_width[tbl->col] = v;
                }
            }
            else if (cell->icell >= 0)
            {
                v0 = cell->fixed_width[cell->icell];
                if (v0 == 0 || (v0 > 0 && v > v0) || (v0 < 0 && v < v0))
                    cell->fixed_width[cell->icell] = v;
            }
        }
        for (i = 0; i < rowspan; i++)
        {
            tbl->check_row(tbl->row + i);
            for (j = 0; j < colspan; j++)
            {
#if 0
		tbl->tabattr[tbl->row + i][tbl->col + j] &= ~(HTT_X | HTT_Y);
#endif
                if (!(tbl->tabattr[tbl->row + i][tbl->col + j] &
                      (HTT_X | HTT_Y)))
                {
                    tbl->tabattr[tbl->row + i][tbl->col + j] |=
                        ((i > 0) ? HTT_Y : HTT_NONE) | ((j > 0) ? HTT_X : HTT_NONE);
                }
                if (tbl->col + j > tbl->maxcol)
                {
                    tbl->maxcol = tbl->col + j;
                }
            }
            if (tbl->row + i > tbl->maxrow)
            {
                tbl->maxrow = tbl->row + i;
            }
        }
        tbl->begin_cell(mode);
        break;
    case HTML_N_TR:
        tbl->setwidth(mode);
        tbl->col = -1;
        tbl->flag &= ~(TBL_IN_ROW | TBL_IN_COL);
        return TAG_ACTION_NONE;
    case HTML_N_TH:
    case HTML_N_TD:
        tbl->setwidth(mode);
        tbl->flag &= ~TBL_IN_COL;
#ifdef FEED_TABLE_DEBUG
        {
            TextListItem *it;
            int i = tbl->col, j = tbl->row;
            fprintf(stderr, "(a) row,col: %d, %d\n", j, i);
            if (tbl->tabdata[j] && tbl->tabdata[j][i])
            {
                for (it = ((TextList *)tbl->tabdata[j][i])->first;
                     it; it = it->next)
                    fprintf(stderr, "  [%s] \n", it->ptr);
            }
        }
#endif
        return TAG_ACTION_NONE;
    case HTML_P:
    case HTML_BR:
    case HTML_CENTER:
    case HTML_N_CENTER:
    case HTML_DIV:
    case HTML_N_DIV:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
    case HTML_DT:
    case HTML_DD:
    case HTML_H:
    case HTML_N_H:
    case HTML_LI:
    case HTML_PRE:
    case HTML_N_PRE:
    case HTML_HR:
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
    case HTML_PRE_PLAIN:
    case HTML_N_PRE_PLAIN:
        tbl->feed_table_block_tag(line, mode, 0, cmd);
        switch (cmd)
        {
        case HTML_PRE:
        case HTML_PRE_PLAIN:
            mode->pre_mode |= TBLM_PRE;
            break;
        case HTML_N_PRE:
        case HTML_N_PRE_PLAIN:
            mode->pre_mode &= ~TBLM_PRE;
            break;
        case HTML_LISTING:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = HTML_N_LISTING;
            break;
        case HTML_XMP:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = HTML_N_XMP;
            break;
        case HTML_PLAINTEXT:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = MAX_HTMLTAG;
            break;
        }
        break;
    case HTML_DL:
    case HTML_BLQ:
    case HTML_OL:
    case HTML_UL:
        tbl->feed_table_block_tag(line, mode, 1, cmd);
        break;
    case HTML_N_DL:
    case HTML_N_BLQ:
    case HTML_N_OL:
    case HTML_N_UL:
        tbl->feed_table_block_tag(line, mode, -1, cmd);
        break;
    case HTML_NOBR:
    case HTML_WBR:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
    case HTML_PRE_INT:
        tbl->feed_table_inline_tag(line, mode, -1);
        switch (cmd)
        {
        case HTML_NOBR:
            mode->nobr_level++;
            if (mode->pre_mode & TBLM_NOBR)
                return TAG_ACTION_NONE;
            mode->pre_mode |= TBLM_NOBR;
            break;
        case HTML_PRE_INT:
            if (mode->pre_mode & TBLM_PRE_INT)
                return TAG_ACTION_NONE;
            mode->pre_mode |= TBLM_PRE_INT;
            tbl->linfo.prev_spaces = 0;
            break;
        }
        mode->nobr_offset = -1;
        if (tbl->linfo.length > 0)
        {
            tbl->check_minimum0(tbl->linfo.length);
            tbl->linfo.length = 0;
        }
        break;
    case HTML_N_NOBR:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
        tbl->feed_table_inline_tag(line, mode, -1);
        if (mode->nobr_level > 0)
            mode->nobr_level--;
        if (mode->nobr_level == 0)
            mode->pre_mode &= ~TBLM_NOBR;
        break;
    case HTML_N_PRE_INT:
        tbl->feed_table_inline_tag(line, mode, -1);
        mode->pre_mode &= ~TBLM_PRE_INT;
        break;
    case HTML_IMG:
        tbl->check_rowcol(mode);
        w = tbl->fixed_width[tbl->col];
        if (w < 0)
        {
            if (tbl->total_width > 0)
                w = -tbl->total_width * w / 100;
            else if (width > 0)
                w = -width * w / 100;
            else
                w = 0;
        }
        else if (w == 0)
        {
            if (tbl->total_width > 0)
                w = tbl->total_width;
            else if (width > 0)
                w = width;
        }
        tok = this->process_img(tag, w);
        this->feed_table1(tbl, tok, mode, width);
        break;
    case HTML_FORM:
        tbl->feed_table_block_tag("", mode, 0, cmd);
        tmp = this->FormOpen(tag);
        if (tmp)
            this->feed_table1(tbl, tmp, mode, width);
        break;
    case HTML_N_FORM:
        tbl->feed_table_block_tag("", mode, 0, cmd);
        this->FormClose();
        break;
    case HTML_INPUT:
        tmp = this->process_input(tag);
        this->feed_table1(tbl, tmp, mode, width);
        break;
    case HTML_SELECT:
        tmp = this->process_select(tag);
        if (tmp)
            this->feed_table1(tbl, tmp, mode, width);
        mode->pre_mode |= TBLM_INSELECT;
        mode->end_tag = HTML_N_SELECT;
        break;
    case HTML_N_SELECT:
    case HTML_OPTION:
        /* nothing */
        break;
    case HTML_TEXTAREA:
        w = 0;
        tbl->check_rowcol(mode);
        if (tbl->col + 1 <= tbl->maxcol &&
            tbl->tabattr[tbl->row][tbl->col + 1] & HTT_X)
        {
            if (cell->icell >= 0 && cell->fixed_width[cell->icell] > 0)
                w = cell->fixed_width[cell->icell];
        }
        else
        {
            if (tbl->fixed_width[tbl->col] > 0)
                w = tbl->fixed_width[tbl->col];
        }
        tmp = this->process_textarea(tag, w);
        if (tmp)
            this->feed_table1(tbl, tmp, mode, width);
        mode->pre_mode |= TBLM_INTXTA;
        mode->end_tag = HTML_N_TEXTAREA;
        break;
    case HTML_A:
        tbl->table_close_anchor0(mode);
        anchor = NULL;
        i = 0;
        tag->TryGetAttributeValue(ATTR_HREF, &anchor);
        tag->TryGetAttributeValue(ATTR_HSEQ, &i);
        if (anchor)
        {
            tbl->check_rowcol(mode);
            if (i == 0)
            {
                Str tmp = this->process_anchor(tag, line);
                if (w3mApp::Instance().displayLinkNumber)
                {
                    Str t = this->GetLinkNumberStr(-1);
                    tbl->feed_table_inline_tag(NULL, mode, t->Size());
                    tmp->Push(t);
                }
                tbl->pushdata(tbl->row, tbl->col, tmp->ptr);
            }
            else
                tbl->pushdata(tbl->row, tbl->col, line);
            if (i >= 0)
            {
                mode->pre_mode |= TBLM_ANCHOR;
                mode->anchor_offset = tbl->tabcontentssize;
            }
        }
        else
            tbl->suspend_or_pushdata(line);
        break;
    case HTML_DEL:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode |= TBLM_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 5); /* [DEL: */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_N_DEL:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode &= ~TBLM_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 5); /* :DEL] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_S:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode |= TBLM_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 3); /* [S: */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_N_S:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode &= ~TBLM_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 3); /* :S] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_INS:
    case HTML_N_INS:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 5); /* [INS:, :INS] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_SUP:
    case HTML_SUB:
    case HTML_N_SUB:
        if (!(mode->pre_mode & (TBLM_DEL | TBLM_S)))
            tbl->feed_table_inline_tag(line, mode, 1); /* ^, [, ] */
        break;
    case HTML_N_SUP:
        break;
    case HTML_TABLE_ALT:
        id = -1;
        w = 0;
        tag->TryGetAttributeValue(ATTR_TID, &id);
        if (id >= 0 && id < tbl->ntable)
        {
            struct table *tbl1 = tbl->tables[id].ptr;
            tbl->feed_table_block_tag(line, mode, 0, cmd);
            tbl->addcontentssize(tbl1->maximum_table_width());
            tbl->check_minimum0(tbl1->sloppy_width);
#ifdef TABLE_EXPAND
            w = tbl1->total_width;
            v = 0;
            colspan = table_colspan(tbl, tbl->row, tbl->col);
            if (colspan > 1)
            {
                if (cell->icell >= 0)
                    v = cell->fixed_width[cell->icell];
            }
            else
                v = tbl->fixed_width[tbl->col];
            if (v < 0 && tbl->real_width > 0 && tbl1->real_width > 0)
                w = -(tbl1->real_width * 100) / tbl->real_width;
            else
                w = tbl1->real_width;
            if (w > 0)
                tbl->check_minimum0(w);
            else if (w < 0 && v < w)
            {
                if (colspan > 1)
                {
                    if (cell->icell >= 0)
                        cell->fixed_width[cell->icell] = w;
                }
                else
                    tbl->fixed_width[tbl->col] = w;
            }
#endif
            tbl->setwidth0(mode);
            tbl->clearcontentssize(mode);
        }
        break;
    case HTML_CAPTION:
        mode->caption = 1;
        break;
    case HTML_N_CAPTION:
    case HTML_THEAD:
    case HTML_N_THEAD:
    case HTML_TBODY:
    case HTML_N_TBODY:
    case HTML_TFOOT:
    case HTML_N_TFOOT:
    case HTML_COLGROUP:
    case HTML_N_COLGROUP:
    case HTML_COL:
        break;
    case HTML_SCRIPT:
        mode->pre_mode |= TBLM_SCRIPT;
        mode->end_tag = HTML_N_SCRIPT;
        break;
    case HTML_STYLE:
        mode->pre_mode |= TBLM_STYLE;
        mode->end_tag = HTML_N_STYLE;
        break;
    case HTML_N_A:
        tbl->table_close_anchor0(mode);
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
        tbl->suspend_or_pushdata(line);
        break;
    case HTML_INTERNAL:
    case HTML_N_INTERNAL:
    case HTML_FORM_INT:
    case HTML_N_FORM_INT:
    case HTML_INPUT_ALT:
    case HTML_N_INPUT_ALT:
    case HTML_SELECT_INT:
    case HTML_N_SELECT_INT:
    case HTML_OPTION_INT:
    case HTML_TEXTAREA_INT:
    case HTML_N_TEXTAREA_INT:
    case HTML_IMG_ALT:
    case HTML_SYMBOL:
    case HTML_N_SYMBOL:
    default:
        /* unknown tag: put into table */
        return TAG_ACTION_FEED;
    }
    return TAG_ACTION_NONE;
}

int HtmlContext::feed_table(struct table *tbl, const char *line, struct table_mode *mode, int width, int internal)
{
    int i;
    Str tmp;
    struct table_linfo *linfo = &tbl->linfo;

    if (*line == '<' && line[1] && REALLY_THE_BEGINNING_OF_A_TAG(line))
    {
        struct parsed_tag *tag;
        const char *p = line;
        tag = parse_tag(&p, internal);
        if (tag)
        {
            switch (this->feed_table_tag(tbl, line, mode, width, tag))
            {
            case TAG_ACTION_NONE:
                return -1;
            case TAG_ACTION_N_TABLE:
                return 0;
            case TAG_ACTION_TABLE:
                return 1;
            case TAG_ACTION_PLAIN:
                break;
            case TAG_ACTION_FEED:
            default:
                if (tag->need_reconstruct)
                    line = tag->ToStr()->ptr;
            }
        }
        else
        {
            if (!(mode->pre_mode & (TBLM_PLAIN | TBLM_INTXTA | TBLM_INSELECT |
                                    TBLM_SCRIPT | TBLM_STYLE)))
                return -1;
        }
    }
    else
    {
        if (mode->pre_mode & (TBLM_DEL | TBLM_S))
            return -1;
    }
    if (mode->caption)
    {
        tbl->caption->Push(line);
        return -1;
    }
    if (mode->pre_mode & TBLM_SCRIPT)
        return -1;
    if (mode->pre_mode & TBLM_STYLE)
        return -1;
    if (mode->pre_mode & TBLM_INTXTA)
    {
        this->feed_textarea(line);
        return -1;
    }
    if (mode->pre_mode & TBLM_INSELECT)
    {
        this->feed_select(line);
        return -1;
    }
    if (!(mode->pre_mode & TBLM_PLAIN) &&
        !(*line == '<' && line[strlen(line) - 1] == '>') &&
        strchr(line, '&') != NULL)
    {
        tmp = Strnew();
        for (auto p = line; *p;)
        {
            const char *q, *r;
            if (*p == '&')
            {
                if (!strncasecmp(p, "&amp;", 5) ||
                    !strncasecmp(p, "&gt;", 4) || !strncasecmp(p, "&lt;", 4))
                {
                    /* do not convert */
                    tmp->Push(*p);
                    p++;
                }
                else
                {
                    q = p;
                    auto [pos, ec] = ucs4_from_entity(p);
                    p = pos.data();
                    switch (ec)
                    {
                    case '<':
                        tmp->Push("&lt;");
                        break;
                    case '>':
                        tmp->Push("&gt;");
                        break;
                    case '&':
                        tmp->Push("&amp;");
                        break;
                    case '\r':
                        tmp->Push('\n');
                        break;
                    default:
                        r = (char *)from_unicode(ec, w3mApp::Instance().InnerCharset);
                        if (r != NULL && strlen(r) == 1 &&
                            ec == (unsigned char)*r)
                        {
                            tmp->Push(*r);
                            break;
                        }
                    case -1:
                        tmp->Push(*q);
                        p = q + 1;
                        break;
                    }
                }
            }
            else
            {
                tmp->Push(*p);
                p++;
            }
        }
        line = tmp->ptr;
    }
    if (!(mode->pre_mode & (TBLM_SPECIAL & ~TBLM_NOBR)))
    {
        if (!(tbl->flag & TBL_IN_COL) || linfo->prev_spaces != 0)
            while (IS_SPACE(*line))
                line++;
        if (*line == '\0')
            return -1;
        tbl->check_rowcol(mode);
        if (mode->pre_mode & TBLM_NOBR && mode->nobr_offset < 0)
            mode->nobr_offset = tbl->tabcontentssize;

        /* count of number of spaces skipped in normal mode */
        i = tbl->skip_space(line, linfo, !(mode->pre_mode & TBLM_NOBR));
        tbl->addcontentssize(visible_length(line) - i);
        tbl->setwidth(mode);
        tbl->pushdata(tbl->row, tbl->col, line);
    }
    else if (mode->pre_mode & TBLM_PRE_INT)
    {
        tbl->check_rowcol(mode);
        if (mode->nobr_offset < 0)
            mode->nobr_offset = tbl->tabcontentssize;
        tbl->addcontentssize(maximum_visible_length(line, tbl->tabcontentssize));
        tbl->setwidth(mode);
        tbl->pushdata(tbl->row, tbl->col, line);
    }
    else
    {
        /* <pre> mode or something like it */
        tbl->check_rowcol(mode);
        while (*line)
        {
            int nl = false;
            const char *p;
            if ((p = strchr(const_cast<char *>(line), '\r')) || (p = strchr(const_cast<char *>(line), '\n')))
            {
                if (*p == '\r' && p[1] == '\n')
                    p++;
                if (p[1])
                {
                    p++;
                    tmp = Strnew_charp_n(line, p - line);
                    line = p;
                    p = tmp->ptr;
                }
                else
                {
                    p = line;
                    line = "";
                }
                nl = true;
            }
            else
            {
                p = line;
                line = "";
            }
            if (mode->pre_mode & TBLM_PLAIN)
                i = maximum_visible_length_plain(p, tbl->tabcontentssize);
            else
                i = maximum_visible_length(p, tbl->tabcontentssize);
            tbl->addcontentssize(i);
            tbl->setwidth(mode);
            if (nl)
                tbl->clearcontentssize(mode);
            tbl->pushdata(tbl->row, tbl->col, p);
        }
    }
    return -1;
}

void HtmlContext::feed_table1(struct table *tbl, Str tok, struct table_mode *mode, int width)
{
    if (!tok)
        return;

    auto tokbuf = Strnew();
    auto status = R_ST_NORMAL;
    auto line = tok->ptr;
    while (read_token(tokbuf, &line, &status, mode->pre_mode & TBLM_PREMODE, 0))
        this->feed_table(tbl, tokbuf->ptr, mode, width, true);
}

static int
floor_at_intervals(int x, int step)
{
    int mo = x % step;
    if (mo > 0)
        x -= mo;
    else if (mo < 0)
        x += step - mo;
    return x;
}

#define RULE(mode, n) (((mode) == BORDER_THICK) ? ((n) + 16) : (n))
#define TK_VERTICALBAR(mode) RULE(mode, 5)

void HtmlContext::renderTable(struct table *t, int max_width, struct html_feed_environ *h_env)
{
    t->total_height = 0;
    if (t->maxcol < 0)
    {
        this->make_caption(t, h_env);
        return;
    }

    if (t->sloppy_width > max_width)
        max_width = t->sloppy_width;

    int rulewidth = t->table_rule_width(Terminal::SymbolWidth());

    max_width -= t->table_border_width(Terminal::SymbolWidth());

    if (rulewidth > 1)
        max_width = floor_at_intervals(max_width, rulewidth);

    if (max_width < rulewidth)
        max_width = rulewidth;

    t->check_maximum_width();

#ifdef MATRIX
    if (t->maxcol == 0)
    {
        if (t->tabwidth[0] > max_width)
            t->tabwidth[0] = max_width;
        if (t->total_width > 0)
            t->tabwidth[0] = max_width;
        else if (t->fixed_width[0] > 0)
            t->tabwidth[0] = t->fixed_width[0];
        if (t->tabwidth[0] < t->minimum_width[0])
            t->tabwidth[0] = t->minimum_width[0];
    }
    else
    {
        t->set_table_matrix(max_width);

        int itr = 0;
        auto mat = m_get(t->maxcol + 1, t->maxcol + 1);
        auto pivot = px_get(t->maxcol + 1);
        auto newwidth = v_get(t->maxcol + 1);
        auto minv = m_get(t->maxcol + 1, t->maxcol + 1);
        do
        {
            m_copy(t->matrix, mat);
            LUfactor(mat, pivot);
            LUsolve(mat, pivot, t->vector, newwidth);
            LUinverse(mat, pivot, minv);
#ifdef TABLE_DEBUG
            set_integered_width(t, newwidth->ve, new_tabwidth);
            fprintf(stderr, "itr=%d\n", itr);
            fprintf(stderr, "max_width=%d\n", max_width);
            fprintf(stderr, "minimum : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", t->minimum_width[i]);
            fprintf(stderr, "\nfixed : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", t->fixed_width[i]);
            fprintf(stderr, "\ndecided : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", new_tabwidth[i]);
            fprintf(stderr, "\n");
#endif /* TABLE_DEBUG */
            itr++;

        } while (t->check_table_width(newwidth->ve, minv, itr));
        short new_tabwidth[MAXCOL];
        t->set_integered_width(newwidth->ve, new_tabwidth, Terminal::SymbolWidth());
        t->check_minimum_width(new_tabwidth);
        v_free(newwidth);
        px_free(pivot);
        m_free(mat);
        m_free(minv);
        m_free(t->matrix);
        v_free(t->vector);
        for (int i = 0; i <= t->maxcol; i++)
        {
            t->tabwidth[i] = new_tabwidth[i];
        }
    }
#else  /* not MATRIX */
    set_table_width(t, new_tabwidth, max_width);
    for (i = 0; i <= t->maxcol; i++)
    {
        t->tabwidth[i] = new_tabwidth[i];
    }
#endif /* not MATRIX */

    t->check_minimum_width(t->tabwidth);
    for (int i = 0; i <= t->maxcol; i++)
        t->tabwidth[i] = ceil_at_intervals(t->tabwidth[i], rulewidth);

    this->renderCoTable(t, h_env->limit);

    for (int i = 0; i <= t->maxcol; i++)
    {
        for (int j = 0; j <= t->maxrow; j++)
        {
            t->check_row(j);
            if (t->tabattr[j][i] & HTT_Y)
                continue;
            this->do_refill(t, j, i, h_env->limit);
        }
    }

    t->check_minimum_width(t->tabwidth);
    t->total_width = 0;
    for (int i = 0; i <= t->maxcol; i++)
    {
        t->tabwidth[i] = ceil_at_intervals(t->tabwidth[i], rulewidth);
        t->total_width += t->tabwidth[i];
    }

    t->total_width += t->table_border_width(Terminal::SymbolWidth());

    t->check_table_height();

    for (int i = 0; i <= t->maxcol; i++)
    {
        for (int j = 0; j <= t->maxrow; j++)
        {
            if ((t->tabattr[j][i] & HTT_Y) ||
                (t->tabattr[j][i] & HTT_TOP) || (t->tabdata[j][i] == NULL))
                continue;
            auto h = t->tabheight[j];
            for (auto k = j + 1; k <= t->maxrow; k++)
            {
                if (!(t->tabattr[k][i] & HTT_Y))
                    break;
                h += t->tabheight[k];
                switch (t->border_mode)
                {
                case BORDER_THIN:
                case BORDER_THICK:
                case BORDER_NOWIN:
                    h += 1;
                    break;
                }
            }
            h -= t->tabdata[j][i]->nitem;
            if (t->tabattr[j][i] & HTT_MIDDLE)
                h /= 2;
            if (h <= 0)
                continue;
            auto l = newTextLineList();
            for (auto k = 0; k < h; k++)
                pushTextLine(l, newTextLine(NULL, 0));
            t->tabdata[j][i] = appendGeneralList((GeneralList *)l,
                                                 t->tabdata[j][i]);
        }
    }

    /* table output */
    auto width = t->total_width;

    this->make_caption(t, h_env);

    this->HTMLlineproc0("<pre for_table>", h_env, true);

    if (t->id != NULL)
    {
        auto idtag = Sprintf("<_id id=\"%s\">", html_quote((t->id)->ptr));
        this->HTMLlineproc0(idtag->ptr, h_env, true);
    }

    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    {
        auto renderbuf = Strnew();
        t->print_sep(-1, VALIGN_TOP, t->maxcol, renderbuf, Terminal::SymbolWidth());
        h_env->push_render_image(renderbuf, width, t->total_width);
        t->total_height += 1;
        break;
    }
    }

    Str vrulea = nullptr;
    Str vruleb = Strnew();
    Str vrulec = nullptr;
    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
        vrulea = Strnew();
        vrulec = Strnew();
        push_symbol(vrulea, TK_VERTICALBAR(t->border_mode), Terminal::SymbolWidth(), 1);
        for (int i = 0; i < t->cellpadding; i++)
        {
            vrulea->Push(' ');
            vruleb->Push(' ');
            vrulec->Push(' ');
        }
        push_symbol(vrulec, TK_VERTICALBAR(t->border_mode), Terminal::SymbolWidth(), 1);
    case BORDER_NOWIN:
        push_symbol(vruleb, TK_VERTICALBAR(BORDER_THIN), Terminal::SymbolWidth(), 1);
        for (int i = 0; i < t->cellpadding; i++)
            vruleb->Push(' ');
        break;
    case BORDER_NONE:
        for (int i = 0; i < t->cellspacing; i++)
            vruleb->Push(' ');
    }

    for (int r = 0; r <= t->maxrow; r++)
    {
        for (int h = 0; h < t->tabheight[r]; h++)
        {
            auto renderbuf = Strnew();
            if (t->border_mode == BORDER_THIN || t->border_mode == BORDER_THICK)
                renderbuf->Push(vrulea);

            if (t->tridvalue[r] != NULL && h == 0)
            {
                auto idtag = Sprintf("<_id id=\"%s\">",
                                     html_quote((t->tridvalue[r])->ptr));
                renderbuf->Push(idtag);
            }

            for (int i = 0; i <= t->maxcol; i++)
            {
                t->check_row(r);

                if (t->tabidvalue[r][i] != NULL && h == 0)
                {
                    auto idtag = Sprintf("<_id id=\"%s\">",
                                         html_quote((t->tabidvalue[r][i])->ptr));
                    renderbuf->Push(idtag);
                }

                if (!(t->tabattr[r][i] & HTT_X))
                {
                    int w = t->tabwidth[i];
                    for (int j = i + 1;
                         j <= t->maxcol && (t->tabattr[r][j] & HTT_X); j++)
                        w += t->tabwidth[j] + t->cellspacing;
                    if (t->tabattr[r][i] & HTT_Y)
                    {
                        int j = r - 1;
                        for (; j >= 0 && t->tabattr[j] && (t->tabattr[j][i] & HTT_Y); j--)
                            ;
                        t->print_item(j, i, w, renderbuf);
                    }
                    else
                        t->print_item(r, i, w, renderbuf);
                }
                if (i < t->maxcol && !(t->tabattr[r][i + 1] & HTT_X))
                    renderbuf->Push(vruleb);
            }
            switch (t->border_mode)
            {
            case BORDER_THIN:
            case BORDER_THICK:
                renderbuf->Push(vrulec);
                t->total_height += 1;
                break;
            }
            h_env->push_render_image(renderbuf, width, t->total_width);
        }
        if (r < t->maxrow && t->border_mode != BORDER_NONE)
        {
            auto renderbuf = Strnew();
            t->print_sep(r, VALIGN_MIDDLE, t->maxcol, renderbuf, Terminal::SymbolWidth());
            h_env->push_render_image(renderbuf, width, t->total_width);
        }
        t->total_height += t->tabheight[r];
    }
    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    {
        auto renderbuf = Strnew();
        t->print_sep(t->maxrow, VALIGN_BOTTOM, t->maxcol, renderbuf, Terminal::SymbolWidth());
        h_env->push_render_image(renderbuf, width, t->total_width);
        t->total_height += 1;
        break;
    }
    }
    if (t->total_height == 0)
    {
        auto renderbuf = Strnew(" ");
        t->total_height++;
        t->total_width = 1;
        h_env->push_render_image(renderbuf, 1, t->total_width);
    }
    this->HTMLlineproc0("</pre>", h_env, true);
}

void HtmlContext::renderCoTable(struct table *tbl, int maxlimit)
{
    struct readbuffer obuf;
    struct table *t;
    int i, col, row;
    int indent, maxwidth;

    for (i = 0; i < tbl->ntable; i++)
    {
        t = tbl->tables[i].ptr;
        col = tbl->tables[i].col;
        row = tbl->tables[i].row;
        indent = tbl->tables[i].indent;

        html_feed_environ h_env(&obuf, tbl->tables[i].buf,
                                tbl->get_spec_cell_width(row, col), indent);
        tbl->check_row(row);
        if (h_env.limit > maxlimit)
            h_env.limit = maxlimit;
        if (t->total_width == 0)
            maxwidth = h_env.limit - indent;
        else if (t->total_width > 0)
            maxwidth = t->total_width;
        else
            maxwidth = t->total_width = -t->total_width * h_env.limit / 100;
        this->renderTable(t, maxwidth, &h_env);
    }
}

BufferPtr loadHTMLStream(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset, bool internal)
{
    auto newBuf = Buffer::Create(url);
    newBuf->type = "text/html";
    struct readbuffer obuf;
    html_feed_environ htmlenv1(&obuf, newTextLineList(), Terminal::columns());
    clen_t linelen = 0;
    clen_t trbyte = 0;
    HtmlContext context;

    context.Initialize(newBuf, content_charset);
    Str lineBuf2 = nullptr;
    while ((lineBuf2 = stream->mygets())->Size())
    {
        linelen += lineBuf2->Size();
        showProgress(&linelen, &trbyte, 0);
        CharacterEncodingScheme detected = {};
        auto converted = wc_Str_conv_with_detect(lineBuf2, &detected, context.DocCharset(), w3mApp::Instance().InnerCharset);
        context.SetCES(detected);
        context.HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
    }
    if (obuf.status != R_ST_NORMAL)
    {
        obuf.status = R_ST_EOL;
        context.HTMLlineproc0("\n", &htmlenv1, internal);
    }
    obuf.status = R_ST_NORMAL;
    context.completeHTMLstream(&htmlenv1, &obuf);
    htmlenv1.flushline(0, 2, htmlenv1.limit);
    if (htmlenv1.title)
        newBuf->buffername = htmlenv1.title;

    // if (!success)
    // {
    //     context.HTMLlineproc0("<br>Transfer Interrupted!<br>", &htmlenv1, true);
    // }

    // if (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)
    // {
    //     context.print_internal_information(&htmlenv1);
    //     return;
    // }
    // if (w3mApp::Instance().w3m_backend)
    // {
    //     context.print_internal_information(&htmlenv1);
    //     backend_halfdump_buf = htmlenv1.buf;
    //     return;
    // }

    newBuf->trbyte = trbyte + linelen;

    newBuf->document_charset = context.DocCharset();

    ImageFlags image_flag;
    if (newBuf->image_flag)
        image_flag = newBuf->image_flag;
    else if (ImageManager::Instance().activeImage && ImageManager::Instance().displayImage && ImageManager::Instance().autoImage)
        image_flag = IMG_FLAG_AUTO;
    else
        image_flag = IMG_FLAG_SKIP;
    newBuf->image_flag = image_flag;

    {
        auto feed = [feeder = TextFeeder{htmlenv1.buf->first}]() -> Str {
            return feeder();
        };

        context.BufferFromLines(newBuf, feed);
    }

    // newBuf->document_charset = w3mApp::Instance().InnerCharset;
    // newBuf->document_charset = WC_CES_US_ASCII;
    // newBuf->CurrentAsLast();
    // newBuf->type = "text/html";
    // newBuf->real_type = newBuf->type;
    // formResetBuffer(newBuf, newBuf->formitem);

    return newBuf;
}
