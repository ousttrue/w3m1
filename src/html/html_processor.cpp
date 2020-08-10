#include "html_processor.h"
#include "html/html_context.h"
#include "textarea.h"
#include "html/html_form.h"
#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "html/form.h"
#include "transport/loader.h"
#include "file.h"
#include "myctype.h"
#include "entity.h"
#include "http/compression.h"
#include "html/image.h"
#include "symbol.h"
#include "tagstack.h"
#include "frame.h"
#include "public.h"
#include "commands.h"
#include "html/maparea.h"
#include "html/tokenizer.h"
#include "w3m.h"
#include "frontend/terms.h"
#include "frontend/line.h"
#include "transport/istream.h"
#include <signal.h>
#include <setjmp.h>
#include <vector>
static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

#define FORMSTACK_SIZE 10
#define INITIAL_FORM_SIZE 10
static FormList **forms;
static int *form_stack;
static int form_max = -1;
static int form_sp = 0;
static int forms_size = 0;
int cur_form_id()
{
    return form_sp >= 0 ? form_stack[form_sp] : -1;
}
Str process_n_form(void)
{
    if (form_sp >= 0)
        form_sp--;
    return nullptr;
}


static int cur_iseq;

static void
print_internal_information(struct html_feed_environ *henv)
{
    TextLineList *tl = newTextLineList();

    {
        auto s = Strnew("<internal>");
        pushTextLine(tl, newTextLine(s, 0));
        if (henv->title)
        {
            s = Strnew_m_charp("<title_alt title=\"",
                               html_quote(henv->title), "\">");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }

    get_formselect()->print_internal(tl);
    get_textarea()->print_internal(tl);

    {
        auto s = Strnew("</internal>");
        pushTextLine(tl, newTextLine(s, 0));
    }

    if (henv->buf)
    {
        appendTextLineList(henv->buf, tl);
    }
    else if (henv->f)
    {
        TextLineListItem *p;
        for (p = tl->first; p; p = p->next)
            fprintf(henv->f, "%s\n", Str_conv_to_halfdump(p->ptr->line)->ptr);
    }
}

///
/// feed
///

#define w3m_halfdump (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)

///
/// process
///
Str process_form_int(struct parsed_tag *tag, int fid)
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

    if (w3m_halfdump)
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

Str process_input(struct parsed_tag *tag, HtmlContext *seq)
{
    int i, w, v, x, y, z, iw, ih;
    Str tmp = nullptr;
    const char *qq = "";
    int qlen = 0;

    if (cur_form_id() < 0)
    {
        const char *s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }
    if (tmp == nullptr)
        tmp = Strnew();

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
            tmp->Push(seq->GetLinkNumberStr(0));
        tmp->Push('[');
        break;
    case FORM_INPUT_RADIO:
        if (displayLinkNumber)
            tmp->Push(seq->GetLinkNumberStr(0));
        tmp->Push('(');
    }

    tmp->Push(Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=%s "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      seq->Increment(), cur_form_id(), p, html_quote(r), w, i, qq));

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
                tmp->Push(seq->GetLinkNumberStr(-1));
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
Str process_img(struct parsed_tag *tag, int width, HtmlContext *seq)
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
        Str tmp2;
        r2 = strchr(r, '#');
        auto s = "<form_int method=internal action=map>";
        tmp2 = process_form(parse_tag(&s, TRUE));
        if (tmp2)
            tmp->Push(tmp2);
        tmp->Push(Sprintf("<input_alt fid=\"%d\" "
                          "type=hidden name=link value=\"",
                          cur_form_id));
        tmp->Push(html_quote((r2) ? r2 + 1 : r));
        tmp->Push(Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                          "type=submit no_effect=true>",
                          seq->Increment(), cur_form_id));
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

            u.Parse2(wc_conv(p, w3mApp::Instance().InnerCharset, seq->CES())->ptr, GetCurBaseUrl());
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
                    push_symbol(tmp, IMG_SYMBOL, seq->SymbolWidth(), 1);
                    n = seq->SymbolWidth();
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
            w = w / w3mApp::Instance().pixel_per_char / seq->SymbolWidth();
            if (w <= 0)
                w = 1;
            push_symbol(tmp, HR_SYMBOL, seq->SymbolWidth(), w);
            n = w * seq->SymbolWidth();
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
#ifdef USE_IMAGE
    if (use_image)
    {
        for (; n < nw; n++)
            tmp->Push(' ');
    }
#endif
    tmp->Push("</img_alt>");
    if (pre_int && !ext_pre_int)
        tmp->Push("</pre_int>");
    if (r)
    {
        tmp->Push("</input_alt>");
        process_n_form();
    }
#ifdef USE_IMAGE
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
#endif
    return tmp;
}

Str process_anchor(struct parsed_tag *tag, char *tagbuf, HtmlContext *seq)
{
    if (tag->need_reconstruct)
    {
        tag->SetAttributeValue(ATTR_HSEQ, Sprintf("%d", seq->Increment())->ptr);
        return tag->ToStr();
    }
    else
    {
        Str tmp = Sprintf("<a hseq=\"%d\"", seq->Increment());
        tmp->Push(tagbuf + 2);
        return tmp;
    }
}

static Lineprop ex_efct(Lineprop ex)
{
    Lineprop effect = P_UNKNOWN;

    if (!ex)
        return P_UNKNOWN;

    if (ex & PE_EX_ITALIC)
        effect |= PE_EX_ITALIC_E;

    if (ex & PE_EX_INSERT)
        effect |= PE_EX_INSERT_E;

    if (ex & PE_EX_STRIKE)
        effect |= PE_EX_STRIKE_E;

    return effect;
}

static int currentLn(BufferPtr buf)
{
    if (buf->CurrentLine())
        /*     return buf->currentLine->real_linenumber + 1;      */
        return buf->CurrentLine()->linenumber + 1;
    else
        return 1;
}

///
/// HTML parse state
///
class HtmlProcessor
{
public:
    Lineprop effect = P_UNKNOWN;
    Lineprop ex_effect = P_UNKNOWN;
    char symbol = '\0';

private:
    Anchor *a_href = nullptr;
    Anchor *a_img = nullptr;
    Anchor *a_form = nullptr;

    HtmlTextArea *g_ta;
    std::vector<Anchor *> a_textarea;

    FormSelect *g_fs;
    std::vector<Anchor *> a_select;

    HtmlTags internal = HTML_UNKNOWN;

    frameset_element *idFrame = nullptr;
    std::vector<struct frameset *> frameset_s;

public:
    HtmlProcessor(HtmlTextArea *ta, FormSelect *fs)
        : g_ta(ta), g_fs(fs)
    {
        g_ta->clear(-1);
        g_fs->clear(-1);
    }

    /* end of processing for one line */
    bool EndLineAddBuffer()
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

    void Process(parsed_tag *tag, BufferPtr buf, int pos, const char *str)
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
                this->effect |= PE_ANCHOR;
                this->a_href = buf->href.Put(Anchor::CreateHref(p,
                                                                q ? q : "",
                                                                r ? r : "",
                                                                s ? s : "",
                                                                *t, currentLn(buf), pos));
                this->a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
                this->a_href->slave = (hseq > 0) ? FALSE : TRUE;
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
                    URL u;
                    Image *image;

                    u.Parse2(this->a_img->url, GetCurBaseUrl());
                    this->a_img->image = image = New(Image);
                    image->url = u.ToStr()->ptr;
                    if (!uncompressed_file_type(u.file.c_str(), &image->ext))
                        image->ext = filename_extension(u.file.c_str(), TRUE);
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
                    image->cache = getImage(image, GetCurBaseUrl(),
                                            IMG_FLAG_SKIP);
                }
                else if (iseq < 0)
                {
                    BufferPoint *po = &buf->imarklist[-iseq - 1];
                    auto a = buf->img.RetrieveAnchor(po->line, po->pos);
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
            FormList *form;
            int top = 0, bottom = 0;

            auto hseq = 0;
            tag->TryGetAttributeValue(ATTR_HSEQ, &hseq);
            auto form_id = -1;
            tag->TryGetAttributeValue(ATTR_FID, &form_id);
            tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &top);
            tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &bottom);
            if (form_id < 0 || form_id > form_max || forms == nullptr)
                break; /* outside of <form>..</form> */
            form = forms[form_id];
            if (hseq > 0)
            {
                int hpos = pos;
                if (*str == '[')
                    hpos++;
                buf->putHmarker(currentLn(buf), hpos, hseq - 1);
            }
            if (!form->target)
                form->target = Strnew(buf->baseTarget)->ptr;

            int textareanumber = -1;
            if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &textareanumber))
            {
                g_ta->grow(textareanumber);
            }

            int selectnumber = -1;
            if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &selectnumber))
            {
                g_fs->grow(selectnumber);
            }

            auto fi = formList_addInput(form, tag, g_ta);
            if (fi)
            {
                Anchor a;
                a.target = form->target ? form->target : "";
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
                buf->baseURL.Parse(p, nullptr);
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
            if (p && q && !strcasecmp(p, "refresh") && MetaRefresh)
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
                process_form_int(tag, form_id);
            break;
        }
        case HTML_TEXTAREA_INT:
        {
            int n_textarea = -1;
            if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &n_textarea))
            {
                g_ta->set(n_textarea, Strnew());
            }
            break;
        }
        case HTML_N_TEXTAREA_INT:
        {
            auto [n, t] = g_ta->getCurrent();
            auto anchor = a_textarea[n];
            if (anchor)
            {
                FormItemList *item = anchor->item;
                item->init_value = item->value = t;
            }
            break;
        }
        case HTML_SELECT_INT:
        {
            int n_select = -1;
            if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &n_select))
            {
                g_fs->set(n_select);
            }
            break;
        }
        case HTML_N_SELECT_INT:
        {
            auto [n_select, select] = g_fs->getCurrent();
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
            auto [n_select, select] = g_fs->getCurrent();
            if (select)
            {
                auto q = "";
                tag->TryGetAttributeValue(ATTR_LABEL, &q);
                auto p = q;
                tag->TryGetAttributeValue(ATTR_VALUE, &p);
                auto selected = tag->HasAttribute(ATTR_SELECTED);
                addSelectOption(select,
                                Strnew(p), Strnew(q),
                                selected);
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
};

///
/// 1行ごとに Line の構築と html タグを解釈する
///
using FeedFunc = Str (*)();
static void HTMLlineproc2body(BufferPtr buf, FeedFunc feed, int llimit, HtmlContext *seq)
{
    HtmlProcessor state(get_textarea(), get_formselect());

    //
    // each line
    //
    Str line = nullptr;
    for (int nlines = 1; nlines != llimit; ++nlines)
    {
        if (!line)
        {
            // new line
            line = feed();
            if (!line)
            {
                break;
            }

            auto [n, t] = get_textarea()->getCurrent();
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
            if ((state.effect | ex_efct(state.ex_effect)) & PC_SYMBOL && *str != '<')
            {
                // symbol
                auto p = get_width_symbol(seq->SymbolWidth0(), state.symbol);
                assert(p.size() > 0);
                int len = get_mclen(p.data());
                mode = get_mctype(p[0]);

                out.push(mode | state.effect | ex_efct(state.ex_effect), p[0]);
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    for (int i = 1; len--; ++i)
                    {
                        out.push(mode | state.effect | ex_efct(state.ex_effect), p[i]);
                    }
                }
                str += seq->SymbolWidth();
            }
            else if (mode == PC_CTRL || mode == PC_UNDEF)
            {
                // control
                out.push(PC_ASCII | state.effect | ex_efct(state.ex_effect), ' ');
                str++;
            }
            else if (mode & PC_UNKNOWN)
            {
                // unknown
                out.push(PC_ASCII | state.effect | ex_efct(state.ex_effect), ' ');
                str += get_mclen(str);
            }
            else if (*str != '<' && *str != '&')
            {
                // multibyte char ?
                int len = get_mclen(str);
                out.push(mode | state.effect | ex_efct(state.ex_effect), *(str++));
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--)
                    {
                        out.push(mode | state.effect | ex_efct(state.ex_effect), *(str++));
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
                        out.push(PC_ASCII | state.effect | ex_efct(state.ex_effect), ' ');
                        p++;
                    }
                    else if (mode & PC_UNKNOWN)
                    {
                        out.push(PC_ASCII | state.effect | ex_efct(state.ex_effect), ' ');
                        p += get_mclen(p);
                    }
                    else
                    {
                        int len = get_mclen(p);
                        out.push(mode | state.effect | ex_efct(state.ex_effect), *(p++));
                        if (--len)
                        {
                            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                            while (len--)
                            {
                                out.push(mode | state.effect | ex_efct(state.ex_effect), *(p++));
                            }
                        }
                    }
                }
            }
            else
            {
                /* tag processing */
                auto tag = parse_tag(&str, TRUE);
                if (!tag)
                    continue;

                state.Process(tag, buf, out.len(), str);
            }
        }

        if (state.EndLineAddBuffer())
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

    for (int form_id = 1; form_id <= form_max; form_id++)
        forms[form_id]->next = forms[form_id - 1];
    buf->formlist = (form_max >= 0) ? forms[form_max] : nullptr;
    addMultirowsForm(buf, buf->formitem);
    addMultirowsImg(buf, buf->img);
}

static TextLineListItem *_tl_lp2;
static Str
textlist_feed()
{
    TextLine *p;
    if (_tl_lp2 != nullptr)
    {
        p = _tl_lp2->ptr;
        _tl_lp2 = _tl_lp2->next;
        return p->line;
    }
    return nullptr;
}
static void HTMLlineproc2(BufferPtr buf, TextLineList *tl, HtmlContext *seq)
{
    _tl_lp2 = tl->first;
    HTMLlineproc2body(buf, textlist_feed, -1, seq);
}

static InputStream *_file_lp2;
static Str
file_feed()
{
    Str s;
    s = StrISgets(_file_lp2);
    if (s->Size() == 0)
    {
        ISclose(_file_lp2);
        return nullptr;
    }
    return s;
}

static void HTMLlineproc3(BufferPtr buf, InputStream *stream, HtmlContext *seq)
{
    _file_lp2 = stream;
    HTMLlineproc2body(buf, file_feed, -1, seq);
}

///
/// entry
///
void loadHTMLstream(URLFile *f, BufferPtr newBuf, FILE *src, int internal)
{
    struct environment envs[MAX_ENV_LEVEL];
    clen_t linelen = 0;
    clen_t trbyte = 0;
    Str lineBuf2 = Strnew();
#ifdef USE_M17N
    CharacterEncodingScheme charset = WC_CES_US_ASCII;
    CharacterEncodingScheme doc_charset = w3mApp::Instance().DocumentCharset;
#endif
    struct html_feed_environ htmlenv1;
    struct readbuffer obuf;
#ifdef USE_IMAGE
    int image_flag;
#endif
    MySignalHandler prevtrap = nullptr;

    get_textarea()->clear(0);
    get_formselect()->clear(0);

    form_sp = -1;
    form_max = -1;
    forms_size = 0;
    forms = nullptr;

    HtmlContext seq;

    cur_iseq = 1;
    if (newBuf->image_flag)
        image_flag = newBuf->image_flag;
    else if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && autoImage)
        image_flag = IMG_FLAG_AUTO;
    else
        image_flag = IMG_FLAG_SKIP;
    if (newBuf->currentURL.file.size())
    {
        *GetCurBaseUrl() = *newBuf->BaseURL();
    }

    if (w3mApp::Instance().w3m_halfload)
    {
        newBuf->buffername = "---";
        newBuf->document_charset = w3mApp::Instance().InnerCharset;

        get_textarea()->clear(0);
        get_formselect()->clear(0);

        HTMLlineproc3(newBuf, f->stream, &seq);
        w3mApp::Instance().w3m_halfload = FALSE;
        return;
    }

    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, nullptr, newBuf->width, 0);

    if (w3m_halfdump)
        htmlenv1.f = stdout;
    else
        htmlenv1.buf = newTextLineList();

    auto success = TrapJmp([&]() {

#ifdef USE_M17N
        if (newBuf != nullptr)
        {
            if (newBuf->bufferprop & BP_FRAME)
                charset = w3mApp::Instance().InnerCharset;
            else if (newBuf->document_charset)
                charset = doc_charset = newBuf->document_charset;
        }
        if (content_charset && w3mApp::Instance().UseContentCharset)
            doc_charset = content_charset;
        else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
            doc_charset = WC_CES_UTF_8;
        meta_charset = WC_CES_NONE;
#endif
#if 0
    do_blankline(&htmlenv1, &obuf, 0, 0, htmlenv1.limit);
    obuf.flag = RB_IGNORE_P;
#endif
        if (IStype(f->stream) != IST_ENCODED)
            f->stream = newEncodedStream(f->stream, f->encoding);
        while ((lineBuf2 = f->StrmyISgets())->Size())
        {
#ifdef USE_NNTP
            if (f->scheme == SCM_NEWS && lineBuf2->ptr[0] == '.')
            {
                lineBuf2->Delete(0, 1);
                if (lineBuf2->ptr[0] == '\n' || lineBuf2->ptr[0] == '\r' ||
                    lineBuf2->ptr[0] == '\0')
                {
                    /*
                 * iseos(f->stream) = TRUE;
                 */
                    break;
                }
            }
#endif /* USE_NNTP */
            if (src)
                lineBuf2->Puts(src);
            linelen += lineBuf2->Size();
            if (w3mApp::Instance().w3m_dump & DUMP_EXTRA)
                printf("W3m-in-progress: %s\n", convert_size2(linelen, GetCurrentContentLength(), TRUE));
            if (w3mApp::Instance().w3m_dump & DUMP_SOURCE)
                continue;
            showProgress(&linelen, &trbyte);
            /*
         * if (frame_source)
         * continue;
         */
            if (meta_charset)
            { /* <META> */
                if (content_charset == 0 && w3mApp::Instance().UseContentCharset)
                {
                    doc_charset = meta_charset;
                    charset = WC_CES_US_ASCII;
                }
                meta_charset = WC_CES_NONE;
            }

            lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);

            seq.SetCES(charset);

            HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal, &seq);
        }
        if (obuf.status != R_ST_NORMAL)
        {
            obuf.status = R_ST_EOL;
            HTMLlineproc0("\n", &htmlenv1, internal, &seq);
        }
        obuf.status = R_ST_NORMAL;
        completeHTMLstream(&htmlenv1, &obuf, &seq);
        flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
        if (htmlenv1.title)
            newBuf->buffername = htmlenv1.title;

        return true;
    });

    if (!success)
    {
        HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1, &seq);
    }
    else
    {

        if (w3m_halfdump)
        {
            print_internal_information(&htmlenv1);
            return;
        }
        if (w3mApp::Instance().w3m_backend)
        {
            print_internal_information(&htmlenv1);
            backend_halfdump_buf = htmlenv1.buf;
            return;
        }
    }

    newBuf->trbyte = trbyte + linelen;

    if (!(newBuf->bufferprop & BP_FRAME))
        newBuf->document_charset = charset;

    newBuf->image_flag = image_flag;

    HTMLlineproc2(newBuf, htmlenv1.buf, &seq);
}

/* 
 * loadHTMLBuffer: read file and make new buffer
 */
BufferPtr
loadHTMLBuffer(URLFile *f, BufferPtr newBuf)
{
    FILE *src = nullptr;
    Str tmp;

    if (newBuf == nullptr)
        newBuf = newBuffer(INIT_BUFFER_WIDTH());
    if (newBuf->sourcefile.empty() &&
        (f->scheme != SCM_LOCAL || newBuf->mailcap))
    {
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
            newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(f, newBuf, src, newBuf->bufferprop & BP_FRAME);

    newBuf->CurrentAsLast();

    formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

/* 
 * loadHTMLString: read string and make new buffer
 */
BufferPtr loadHTMLString(Str page)
{
    auto newBuf = newBuffer(INIT_BUFFER_WIDTH());

    auto success = TrapJmp([&]() {
        URLFile f(SCM_LOCAL, newStrStream(page));

        newBuf->document_charset = w3mApp::Instance().InnerCharset;
        loadHTMLstream(&f, newBuf, nullptr, TRUE);
        newBuf->document_charset = WC_CES_US_ASCII;

        return true;
    });

    if (!success)
    {
        return nullptr;
    }

    newBuf->CurrentAsLast();
    newBuf->type = "text/html";
    newBuf->real_type = newBuf->type;

    formResetBuffer(newBuf, newBuf->formitem);
    return newBuf;
}
