#include "html_processor.h"
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
#include "w3m.h"
#include "frontend/terms.h"
#include "transport/istream.h"
#include <signal.h>
#include <setjmp.h>
static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

static int cur_hseq;
int GetCurHSeq()
{
    return cur_hseq;
}
void SetCurHSeq(int seq)
{
    cur_hseq = seq;
}
Str getLinkNumberStr(int correction)
{
    return Sprintf("[%d]", cur_hseq + correction);
}

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10

#define INITIAL_FORM_SIZE 10
static FormList **forms;
static int *form_stack;
static int form_max = -1;
static int form_sp = 0;
static int forms_size = 0;
static int cur_form_id()
{
    return form_sp >= 0 ? form_stack[form_sp] : -1;
}
Str process_n_form(void)
{
    if (form_sp >= 0)
        form_sp--;
    return NULL;
}

static Str cur_select;
static Str select_str;
static int select_is_multiple;
static int n_selectitem;
static Str cur_option;
static Str cur_option_value;
static Str cur_option_label;
static int cur_option_selected;
static int cur_status;
#ifdef MENU_SELECT
/* menu based <select>  */
FormSelectOption *select_option;
static int max_select = MAX_SELECT;
static int n_select;
static int cur_option_maxwidth;
#endif /* MENU_SELECT */

static Str cur_textarea;
Str *textarea_str;
static int cur_textarea_size;
static int cur_textarea_rows;
static int cur_textarea_readonly;
static int n_textarea;
static int ignore_nl_textarea;
static int max_textarea = MAX_TEXTAREA;

static CharacterEncodingScheme cur_document_charset;
static int cur_iseq;

static void
print_internal_information(struct html_feed_environ *henv)
{
    int i;
    Str s;
    TextLineList *tl = newTextLineList();

    s = Strnew("<internal>");
    pushTextLine(tl, newTextLine(s, 0));
    if (henv->title)
    {
        s = Strnew_m_charp("<title_alt title=\"",
                           html_quote(henv->title), "\">", NULL);
        pushTextLine(tl, newTextLine(s, 0));
    }
#if 0
    if (form_max >= 0) {
        FormList *fp;
        for (i = 0; i <= form_max; i++) {
            fp = forms[i];
            s = Sprintf("<form_int fid=\"%d\" action=\"%s\" method=\"%s\"",
                        i, html_quote(fp->action->ptr),
                        (fp->method == FORM_METHOD_POST) ? "post"
                        : ((fp->method ==
                            FORM_METHOD_INTERNAL) ? "internal" : "get"));
            if (fp->target)
                s->Push( Sprintf(" target=\"%s\"", html_quote(fp->target)));
            if (fp->enctype == FORM_ENCTYPE_MULTIPART)
                s->Push( " enctype=\"multipart/form-data\"");
#ifdef USE_M17N
            if (fp->charset)
                s->Push( Sprintf(" accept-charset=\"%s\"",
                                  html_quote(fp->charset)));
#endif
            s->Push( ">");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
#endif
#ifdef MENU_SELECT
    if (n_select > 0)
    {
        FormSelectOptionItem *ip;
        for (i = 0; i < n_select; i++)
        {
            s = Sprintf("<select_int selectnumber=%d>", i);
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
#endif /* MENU_SELECT */
    if (n_textarea > 0)
    {
        for (i = 0; i < n_textarea; i++)
        {
            s = Sprintf("<textarea_int textareanumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            s = Strnew(html_quote(textarea_str[i]->ptr));
            s->Push("</textarea_int>");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
    s = Strnew("</internal>");
    pushTextLine(tl, newTextLine(s, 0));

    if (henv->buf)
        appendTextLineList(henv->buf, tl);
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
void feed_select(char *str)
{
    Str tmp = Strnew();
    int prev_status = cur_status;
    static int prev_spaces = -1;
    char *p;

    if (cur_select == NULL)
        return;
    while (read_token(tmp, &str, &cur_status, 0, 0))
    {
        if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
            continue;
        p = tmp->ptr;
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
                    cur_option_value = NULL;
                if (tag->TryGetAttributeValue(ATTR_LABEL, &q))
                    cur_option_label = Strnew(q);
                else
                    cur_option_label = NULL;
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

void feed_textarea(char *str)
{
    if (cur_textarea == NULL)
        return;
    if (ignore_nl_textarea)
    {
        if (*str == '\r')
            str++;
        if (*str == '\n')
            str++;
    }
    ignore_nl_textarea = FALSE;
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

#define w3m_halfdump (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)

///
/// process
///
Str process_form_int(struct parsed_tag *tag, int fid)
{
    char *p, *q, *r, *s, *tg, *n;

    p = "get";
    tag->TryGetAttributeValue(ATTR_METHOD, &p);
    q = "!CURRENT_URL!";
    tag->TryGetAttributeValue(ATTR_ACTION, &q);
    r = NULL;
#ifdef USE_M17N
    if (tag->TryGetAttributeValue(ATTR_ACCEPT_CHARSET, &r))
        r = check_accept_charset(r);
    if (!r && tag->TryGetAttributeValue(ATTR_CHARSET, &r))
        r = check_charset(r);
#endif
    s = NULL;
    tag->TryGetAttributeValue(ATTR_ENCTYPE, &s);
    tg = NULL;
    tag->TryGetAttributeValue(ATTR_TARGET, &tg);
    n = NULL;
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

    forms[fid] = newFormList(q, p, r, s, tg, n, NULL);
    return NULL;
}

Str process_input(struct parsed_tag *tag)
{
    int i, w, v, x, y, z, iw, ih;
    char *q, *p, *r, *p2, *s;
    Str tmp = NULL;
    char *qq = "";
    int qlen = 0;

    if (cur_form_id() < 0)
    {
        char *s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }
    if (tmp == NULL)
        tmp = Strnew();

    p = "text";
    tag->TryGetAttributeValue(ATTR_TYPE, &p);
    q = NULL;
    tag->TryGetAttributeValue(ATTR_VALUE, &q);
    r = "";
    tag->TryGetAttributeValue(ATTR_NAME, &r);
    w = 20;
    tag->TryGetAttributeValue(ATTR_SIZE, &w);
    i = 20;
    tag->TryGetAttributeValue(ATTR_MAXLENGTH, &i);
    p2 = NULL;
    tag->TryGetAttributeValue(ATTR_ALT, &p2);
    x = tag->HasAttribute(ATTR_CHECKED);
    y = tag->HasAttribute(ATTR_ACCEPT);
    z = tag->HasAttribute(ATTR_READONLY);

    v = formtype(p);
    if (v == FORM_UNKNOWN)
        return NULL;

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
        q = NULL;
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
            tmp->Push(getLinkNumberStr(0));
        tmp->Push('[');
        break;
    case FORM_INPUT_RADIO:
        if (displayLinkNumber)
            tmp->Push(getLinkNumberStr(0));
        tmp->Push('(');
    }
    tmp->Push(Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=%s "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      cur_hseq++, cur_form_id(), p, html_quote(r), w, i, qq));
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
            s = NULL;
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
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            if (displayLinkNumber)
                tmp->Push(getLinkNumberStr(-1));
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

Str process_select(struct parsed_tag *tag)
{
    Str tmp = NULL;
    char *p;

    if (cur_form_id() < 0)
    {
        char *s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }

    p = "";
    tag->TryGetAttributeValue(ATTR_NAME, &p);
    cur_select = Strnew(p);
    select_is_multiple = tag->HasAttribute(ATTR_MULTIPLE);

#ifdef MENU_SELECT
    if (!select_is_multiple)
    {
        select_str = Strnew("<pre_int>");
        if (displayLinkNumber)
            select_str->Push(getLinkNumberStr(0));
        select_str->Push(Sprintf("[<input_alt hseq=\"%d\" "
                                 "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
                                 cur_hseq++, cur_form_id(), html_quote(p), n_select));
        select_str->Push(">");
        if (n_select == max_select)
        {
            max_select *= 2;
            select_option =
                New_Reuse(FormSelectOption, select_option, max_select);
        }
        select_option[n_select].first = NULL;
        select_option[n_select].last = NULL;
        cur_option_maxwidth = 0;
    }
    else
#endif /* MENU_SELECT */
        select_str = Strnew();
    cur_option = NULL;
    cur_status = R_ST_NORMAL;
    n_selectitem = 0;
    return tmp;
}

Str process_n_select(void)
{
    if (cur_select == NULL)
        return NULL;
    process_option();
#ifdef MENU_SELECT
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
#endif /* MENU_SELECT */
        select_str->Push("<br>");
    cur_select = NULL;
    n_selectitem = 0;
    return select_str;
}

void process_option(void)
{
    char begin_char = '[', end_char = ']';
    int len;

    if (cur_select == NULL || cur_option == NULL)
        return;
    while (cur_option->Size() > 0 && IS_SPACE(cur_option->Back()))
        cur_option->Pop(1);
    if (cur_option_value == NULL)
        cur_option_value = cur_option;
    if (cur_option_label == NULL)
        cur_option_label = cur_option;
#ifdef MENU_SELECT
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
#endif /* MENU_SELECT */
    if (!select_is_multiple)
    {
        begin_char = '(';
        end_char = ')';
    }
    select_str->Push(Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                             "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                             begin_char, cur_hseq++, cur_form_id(),
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

Str process_textarea(struct parsed_tag *tag, int width)
{
    Str tmp = NULL;
    char *p;
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

    if (cur_form_id() < 0)
    {
        char *s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }

    p = "";
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

Str process_n_textarea(void)
{
    Str tmp;
    int i;

    if (cur_textarea == NULL)
        return NULL;

    tmp = Strnew();
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
    for (i = 0; i < cur_textarea_size; i++)
        tmp->Push(' ');
    tmp->Push("</u></input_alt>]</pre_int>\n");
    cur_hseq++;
    n_textarea++;
    cur_textarea = NULL;

    return tmp;
}

#define IMG_SYMBOL UL_SYMBOL(12)
Str process_img(struct parsed_tag *tag, int width)
{
    char *p, *q, *r, *r2 = NULL, *s, *t;
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
    q = NULL;
    tag->TryGetAttributeValue(ATTR_ALT, &q);
    if (!pseudoInlines && (q == NULL || (*q == '\0' && ignore_null_img_alt)))
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
    r = NULL;
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
        s = "<form_int method=internal action=map>";
        tmp2 = process_form(parse_tag(&s, TRUE));
        if (tmp2)
            tmp->Push(tmp2);
        tmp->Push(Sprintf("<input_alt fid=\"%d\" "
                          "type=hidden name=link value=\"",
                          cur_form_id));
        tmp->Push(html_quote((r2) ? r2 + 1 : r));
        tmp->Push(Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                          "type=submit no_effect=true>",
                          cur_hseq++, cur_form_id));
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

            u.Parse2(wc_conv(p, w3mApp::Instance().InnerCharset, cur_document_charset)->ptr, GetCurBaseUrl());
            image.url = u.ToStr()->ptr;
            if (!uncompressed_file_type(u.file.c_str(), &image.ext))
                image.ext = filename_extension(u.file.c_str(), TRUE);
            image.cache = NULL;
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
    if (q != NULL && *q == '\0' && ignore_null_img_alt)
        q = NULL;
    if (q != NULL)
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
                    push_symbol(tmp, IMG_SYMBOL, symbol_width, 1);
                    n = symbol_width;
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
            w = w / w3mApp::Instance().pixel_per_char / symbol_width;
            if (w <= 0)
                w = 1;
            push_symbol(tmp, HR_SYMBOL, symbol_width, w);
            n = w * symbol_width;
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

Str process_anchor(struct parsed_tag *tag, char *tagbuf)
{
    if (tag->need_reconstruct)
    {
        parsedtag_set_value(tag, ATTR_HSEQ, Sprintf("%d", cur_hseq++)->ptr);
        return parsedtag2str(tag);
    }
    else
    {
        Str tmp = Sprintf("<a hseq=\"%d\"", cur_hseq++);
        tmp->Push(tagbuf + 2);
        return tmp;
    }
}

static int
ex_efct(int ex)
{
    int effect = 0;

    if (!ex)
        return 0;

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
    if (buf->currentLine)
        /*     return buf->currentLine->real_linenumber + 1;      */
        return buf->currentLine->linenumber + 1;
    else
        return 1;
}

static char *outc = NULL;
static Lineprop *outp = NULL;
static int out_size = 0;

#define PPUSH(p, c)      \
    {                    \
        outp[pos] = (p); \
        outc[pos] = (c); \
        pos++;           \
    }
    
#define PSIZE                                       \
    if (out_size <= pos + 1)                        \
    {                                               \
        out_size = pos * 3 / 2;                     \
        outc = New_Reuse(char, outc, out_size);     \
        outp = New_Reuse(Lineprop, outp, out_size); \
    }

using FeedFunc = Str(*)();
static void HTMLlineproc2body(BufferPtr buf, FeedFunc feed, int llimit)
{
    Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
    char *p, *q, *r, *s, *t;
    Lineprop mode;
#ifdef DEBUG
    FILE *debug = NULL;
#endif

    struct frameset *frameset_s[FRAMESTACK_SIZE];
    int frameset_sp = -1;
    union frameset_element *idFrame = NULL;
    char *id = NULL;
    int hseq, form_id;
    char symbol = '\0';
    int internal = 0;
    Anchor **a_textarea = NULL;
    Anchor **a_select = NULL;

    if (out_size == 0)
    {
        out_size = LINELEN;
        outc = NewAtom_N(char, out_size);
        outp = NewAtom_N(Lineprop, out_size);
    }

    n_textarea = -1;
    if (!max_textarea)
    { /* halfload */
        max_textarea = MAX_TEXTAREA;
        textarea_str = New_N(Str, max_textarea);
        a_textarea = New_N(Anchor *, max_textarea);
    }

    n_select = -1;
    if (!max_select)
    { /* halfload */
        max_select = MAX_SELECT;
        select_option = New_N(FormSelectOption, max_select);
        a_select = New_N(Anchor *, max_select);
    }

#ifdef DEBUG
    if (w3m_debug)
        debug = fopen("zzzerr", "a");
#endif

    //
    // each line
    //
    Lineprop effect = 0;
    Lineprop ex_effect = 0;
    Str line;
    for(int nlines = 0; line = feed();)
    {
#ifdef DEBUG
        if (w3m_debug)
        {
            line->Puts(debug);
            fputc('\n', debug);
        }
#endif

        if (n_textarea >= 0 && *(line->ptr) != '<')
        { /* halfload */
            textarea_str[n_textarea]->Push(line);
            continue;
        }

    proc_again:
        if (++nlines == llimit)
            break;

#ifdef ENABLE_REMOVE_TRAILINGSPACES
        StripRight(line);
#endif

        //
        // each char
        //
        auto str = line->ptr;
        auto endp = str + line->Size();
        int pos = 0;
        while (str < endp)
        {
            PSIZE;
            mode = get_mctype(*str);
            if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<')
            {

                char **buf = set_symbol(symbol_width0);
                int len;

                p = buf[(int)symbol];
                len = get_mclen(p);
                mode = get_mctype(*p);
                PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--)
                    {
                        PSIZE;
                        PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                    }
                }
                str += symbol_width;
            }
            else if (mode == PC_CTRL || mode == PC_UNDEF)
            {
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str++;
            }
            else if (mode & PC_UNKNOWN)
            {
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str += get_mclen(str);
            }
            else if (*str != '<' && *str != '&')
            {
                int len = get_mclen(str);
                PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--)
                    {
                        PSIZE;
                        PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
                    }
                }
            }
            else if (*str == '&')
            {
                /* 
                 * & escape processing
                 */
                {
                    auto [pos, view] = getescapecmd(str, w3mApp::Instance().InnerCharset);
                    str = const_cast<char *>(pos);
                    p = const_cast<char *>(view.data());
                }

                while (*p)
                {
                    PSIZE;
                    mode = get_mctype(*p);
                    if (mode == PC_CTRL || mode == PC_UNDEF)
                    {
                        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p++;
                    }
                    else if (mode & PC_UNKNOWN)
                    {
                        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p += get_mclen(p);
                    }
                    else
                    {
                        int len = get_mclen(p);
                        PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                        if (--len)
                        {
                            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                            while (len--)
                            {
                                PSIZE;
                                PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                            }
                        }
                    }
                }
            }
            else
            {
                /* tag processing */
                struct parsed_tag *tag;
                if (!(tag = parse_tag(&str, TRUE)))
                    continue;
                switch (tag->tagid)
                {
                case HTML_B:
                    effect |= PE_BOLD;
                    break;
                case HTML_N_B:
                    effect &= ~PE_BOLD;
                    break;
                case HTML_I:
                    ex_effect |= PE_EX_ITALIC;
                    break;
                case HTML_N_I:
                    ex_effect &= ~PE_EX_ITALIC;
                    break;
                case HTML_INS:
                    ex_effect |= PE_EX_INSERT;
                    break;
                case HTML_N_INS:
                    ex_effect &= ~PE_EX_INSERT;
                    break;
                case HTML_U:
                    effect |= PE_UNDER;
                    break;
                case HTML_N_U:
                    effect &= ~PE_UNDER;
                    break;
                case HTML_S:
                    ex_effect |= PE_EX_STRIKE;
                    break;
                case HTML_N_S:
                    ex_effect &= ~PE_EX_STRIKE;
                    break;
                case HTML_A:
                    if (renderFrameSet &&
                        tag->TryGetAttributeValue(ATTR_FRAMENAME, &p))
                    {
                        p = wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                        if (!idFrame || strcmp(idFrame->body->name, p))
                        {
                            idFrame = search_frame(renderFrameSet, p);
                            if (idFrame && idFrame->body->attr != F_BODY)
                                idFrame = NULL;
                        }
                    }
                    p = r = s = NULL;
                    q = Strnew(buf->baseTarget)->ptr;
                    t = "";
                    hseq = 0;
                    id = NULL;
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
                    if (tag->TryGetAttributeValue(ATTR_REFERER, &r))
                        r = wc_conv_strict(r, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                    tag->TryGetAttributeValue(ATTR_TITLE, &s);
                    tag->TryGetAttributeValue(ATTR_ACCESSKEY, &t);
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
                    if (id && idFrame)
                    {
                        auto a = Anchor::CreateName(id, currentLn(buf), pos);
                        idFrame->body->nameList.Put(a);
                    }
                    if (p)
                    {
                        effect |= PE_ANCHOR;
                        a_href = buf->href.Put(Anchor::CreateHref(p,
                                                                  q ? q : "",
                                                                  r ? r : "",
                                                                  s ? s : "",
                                                                  *t, currentLn(buf), pos));
                        a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
                        a_href->slave = (hseq > 0) ? FALSE : TRUE;
                    }
                    break;
                case HTML_N_A:
                    effect &= ~PE_ANCHOR;
                    if (a_href)
                    {
                        a_href->end.line = currentLn(buf);
                        a_href->end.pos = pos;
                        if (a_href->start == a_href->end)
                        {
                            if (buf->hmarklist.size() &&
                                a_href->hseq < buf->hmarklist.size())
                                buf->hmarklist[a_href->hseq].invalid = 1;
                            a_href->hseq = -1;
                        }
                        a_href = NULL;
                    }
                    break;

                case HTML_LINK:
                    buf->linklist.push_back(Link::create(*tag, buf->document_charset));
                    break;

                case HTML_IMG_ALT:
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
                        q = NULL;
                        tag->TryGetAttributeValue(ATTR_USEMAP, &q);
                        if (iseq > 0)
                        {
                            buf->putHmarker(currentLn(buf), pos, iseq - 1);
                        }

                        s = NULL;
                        tag->TryGetAttributeValue(ATTR_TITLE, &s);
                        p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                                           buf->document_charset)
                                ->ptr;
                        a_img = buf->img.Put(Anchor::CreateImage(
                            p,
                            s ? s : "",
                            currentLn(buf), pos));

                        a_img->hseq = iseq;
                        a_img->image = NULL;
                        if (iseq > 0)
                        {
                            URL u;
                            Image *image;

                            u.Parse2(a_img->url, GetCurBaseUrl());
                            a_img->image = image = New(Image);
                            image->url = u.ToStr()->ptr;
                            if (!uncompressed_file_type(u.file.c_str(), &image->ext))
                                image->ext = filename_extension(u.file.c_str(), TRUE);
                            image->cache = NULL;
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
                                a_img->url = a->url;
                                a_img->image = a->image;
                            }
                        }
                    }
                    effect |= PE_IMAGE;
                    break;
                case HTML_N_IMG_ALT:
                    effect &= ~PE_IMAGE;
                    if (a_img)
                    {
                        a_img->end.line = currentLn(buf);
                        a_img->end.pos = pos;
                    }
                    a_img = NULL;
                    break;
                case HTML_INPUT_ALT:
                {
                    FormList *form;
                    int top = 0, bottom = 0;
                    int textareanumber = -1;
                    int selectnumber = -1;

                    hseq = 0;
                    form_id = -1;

                    tag->TryGetAttributeValue(ATTR_HSEQ, &hseq);
                    tag->TryGetAttributeValue(ATTR_FID, &form_id);
                    tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &top);
                    tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &bottom);
                    if (form_id < 0 || form_id > form_max || forms == NULL)
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
                    if (a_textarea &&
                        tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER,
                                            &textareanumber))
                    {
                        if (textareanumber >= max_textarea)
                        {
                            max_textarea = 2 * textareanumber;
                            textarea_str = New_Reuse(Str, textarea_str,
                                                     max_textarea);
                            a_textarea = New_Reuse(Anchor *, a_textarea,
                                                   max_textarea);
                        }
                    }

                    if (a_select &&
                        tag->TryGetAttributeValue(ATTR_SELECTNUMBER,
                                            &selectnumber))
                    {
                        if (selectnumber >= max_select)
                        {
                            max_select = 2 * selectnumber;
                            select_option = New_Reuse(FormSelectOption,
                                                      select_option,
                                                      max_select);
                            a_select = New_Reuse(Anchor *, a_select,
                                                 max_select);
                        }
                    }

                    auto fi = formList_addInput(form, tag);
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
                        a_form = buf->formitem.Put(a);
                    }
                    else
                    {
                        a_form = nullptr;
                    }

                    if (a_textarea && textareanumber >= 0)
                        a_textarea[textareanumber] = a_form;

                    if (a_select && selectnumber >= 0)
                        a_select[selectnumber] = a_form;

                    if (a_form)
                    {
                        a_form->hseq = hseq - 1;
                        a_form->y = currentLn(buf) - top;
                        a_form->rows = 1 + top + bottom;
                        if (!tag->HasAttribute(ATTR_NO_EFFECT))
                            effect |= PE_FORM;
                        break;
                    }
                }
                case HTML_N_INPUT_ALT:
                    effect &= ~PE_FORM;
                    if (a_form)
                    {
                        a_form->end.line = currentLn(buf);
                        a_form->end.pos = pos;
                        if (a_form->start.line == a_form->end.line &&
                            a_form->start.pos == a_form->end.pos)
                            a_form->hseq = -1;
                    }
                    a_form = NULL;
                    break;
                case HTML_MAP:
                    if (tag->TryGetAttributeValue(ATTR_NAME, &p))
                    {
                        MapList *m = New(MapList);
                        m->name = Strnew(p);
                        m->area = newGeneralList();
                        m->next = buf->maplist;
                        buf->maplist = m;
                    }
                    break;
                case HTML_N_MAP:
                    /* nothing to do */
                    break;
                case HTML_AREA:
                    if (buf->maplist == NULL) /* outside of <map>..</map> */
                        break;
                    if (tag->TryGetAttributeValue(ATTR_HREF, &p))
                    {
                        MapArea *a;
                        p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                                           buf->document_charset)
                                ->ptr;
                        t = NULL;
                        tag->TryGetAttributeValue(ATTR_TARGET, &t);
                        q = "";
                        tag->TryGetAttributeValue(ATTR_ALT, &q);
                        r = NULL;
                        s = NULL;

                        tag->TryGetAttributeValue(ATTR_SHAPE, &r);
                        tag->TryGetAttributeValue(ATTR_COORDS, &s);

                        a = newMapArea(p, t, q, r, s);
                        pushValue(buf->maplist->area, (void *)a);
                    }
                    break;
                case HTML_FRAMESET:
                    frameset_sp++;
                    if (frameset_sp >= FRAMESTACK_SIZE)
                        break;
                    frameset_s[frameset_sp] = newFrameSet(tag);
                    if (frameset_s[frameset_sp] == NULL)
                        break;
                    if (frameset_sp == 0)
                    {
                        if (buf->frameset == NULL)
                        {
                            buf->frameset = frameset_s[frameset_sp];
                        }
                        else
                            pushFrameTree(&(buf->frameQ),
                                          frameset_s[frameset_sp], NULL);
                    }
                    else
                        addFrameSetElement(frameset_s[frameset_sp - 1],
                                           *(union frameset_element *)&frameset_s[frameset_sp]);
                    break;
                case HTML_N_FRAMESET:
                    if (frameset_sp >= 0)
                        frameset_sp--;
                    break;
                case HTML_FRAME:
                    if (frameset_sp >= 0 && frameset_sp < FRAMESTACK_SIZE)
                    {
                        union frameset_element element;

                        element.body = newFrame(tag, buf);
                        addFrameSetElement(frameset_s[frameset_sp], element);
                    }
                    break;
                case HTML_BASE:
                    if (tag->TryGetAttributeValue(ATTR_HREF, &p))
                    {
                        p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                                           buf->document_charset)
                                ->ptr;
                        buf->baseURL.Parse(p, NULL);
                    }
                    if (tag->TryGetAttributeValue(ATTR_TARGET, &p))
                        buf->baseTarget =
                            wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                    break;
                case HTML_META:
                    p = q = NULL;
                    tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &p);
                    tag->TryGetAttributeValue(ATTR_CONTENT, &q);
                    if (p && q && !strcasecmp(p, "refresh") && MetaRefresh)
                    {
                        Str tmp = NULL;
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
                                                       &reload, NULL);
                    }
                    break;
                case HTML_INTERNAL:
                    internal = HTML_INTERNAL;
                    break;
                case HTML_N_INTERNAL:
                    internal = HTML_N_INTERNAL;
                    break;
                case HTML_FORM_INT:
                    if (tag->TryGetAttributeValue(ATTR_FID, &form_id))
                        process_form_int(tag, form_id);
                    break;
                case HTML_TEXTAREA_INT:
                    if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER,
                                            &n_textarea) &&
                        n_textarea < max_textarea)
                    {
                        textarea_str[n_textarea] = Strnew();
                    }
                    else
                        n_textarea = -1;
                    break;
                case HTML_N_TEXTAREA_INT:
                    if (n_textarea >= 0)
                    {
                        FormItemList *item = a_textarea[n_textarea]->item;
                        item->init_value = item->value =
                            textarea_str[n_textarea];
                    }
                    break;

                case HTML_SELECT_INT:
                    if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &n_select) && n_select < max_select)
                    {
                        select_option[n_select].first = NULL;
                        select_option[n_select].last = NULL;
                    }
                    else
                        n_select = -1;
                    break;
                case HTML_N_SELECT_INT:
                    if (n_select >= 0)
                    {
                        FormItemList *item = a_select[n_select]->item;
                        item->select_option = select_option[n_select].first;
                        chooseSelectOption(item, item->select_option);
                        item->init_selected = item->selected;
                        item->init_value = item->value;
                        item->init_label = item->label;
                    }
                    break;
                case HTML_OPTION_INT:
                    if (n_select >= 0)
                    {
                        int selected;
                        q = "";
                        tag->TryGetAttributeValue(ATTR_LABEL, &q);
                        p = q;
                        tag->TryGetAttributeValue(ATTR_VALUE, &p);
                        selected = tag->HasAttribute(ATTR_SELECTED);
                        addSelectOption(&select_option[n_select],
                                        Strnew(p), Strnew(q),
                                        selected);
                    }
                    break;

                case HTML_TITLE_ALT:
                    if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
                        buf->buffername = html_unquote(p, w3mApp::Instance().InnerCharset);
                    break;
                case HTML_SYMBOL:
                    effect |= PC_SYMBOL;
                    if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
                        symbol = (char)atoi(p);
                    break;
                case HTML_N_SYMBOL:
                    effect &= ~PC_SYMBOL;
                    break;
                }

                id = NULL;
                if (tag->TryGetAttributeValue(ATTR_ID, &id))
                {
                    id = wc_conv_strict(id, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                    buf->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
                }
                if (renderFrameSet &&
                    tag->TryGetAttributeValue(ATTR_FRAMENAME, &p))
                {
                    p = wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                    if (!idFrame || strcmp(idFrame->body->name, p))
                    {
                        idFrame = search_frame(renderFrameSet, p);
                        if (idFrame && idFrame->body->attr != F_BODY)
                            idFrame = NULL;
                    }
                }
                if (id && idFrame)
                {
                    auto a = Anchor::CreateName(id, currentLn(buf), pos);
                    idFrame->body->nameList.Put(a);
                }
            }
        }
        /* end of processing for one line */
        if (!internal)
            addnewline(buf, outc, outp, NULL, pos, -1, nlines);
        if (internal == HTML_N_INTERNAL)
            internal = 0;
        if (str != endp)
        {
            line = line->Substr(str - line->ptr, endp - str);
            goto proc_again;
        }
    }
#ifdef DEBUG
    if (w3m_debug)
        fclose(debug);
#endif
    for (form_id = 1; form_id <= form_max; form_id++)
        forms[form_id]->next = forms[form_id - 1];
    buf->formlist = (form_max >= 0) ? forms[form_max] : NULL;
    if (n_textarea)
        addMultirowsForm(buf, buf->formitem);
#ifdef USE_IMAGE
    addMultirowsImg(buf, buf->img);
#endif
}

static TextLineListItem *_tl_lp2;
static Str
textlist_feed()
{
    TextLine *p;
    if (_tl_lp2 != NULL)
    {
        p = _tl_lp2->ptr;
        _tl_lp2 = _tl_lp2->next;
        return p->line;
    }
    return NULL;
}
void HTMLlineproc2(BufferPtr buf, TextLineList *tl)
{
    _tl_lp2 = tl->first;
    HTMLlineproc2body(buf, textlist_feed, -1);
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
        return NULL;
    }
    return s;
}
void HTMLlineproc3(BufferPtr buf, InputStream *stream)
{
    _file_lp2 = stream;
    HTMLlineproc2body(buf, file_feed, -1);
}

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
    MySignalHandler prevtrap = NULL;

#ifdef USE_M17N
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
#else
    symbol_width = symbol_width0 = 1;
#endif

    SetCurTitle(nullptr);
    n_textarea = 0;
    cur_textarea = NULL;
    max_textarea = MAX_TEXTAREA;
    textarea_str = New_N(Str, max_textarea);
#ifdef MENU_SELECT
    n_select = 0;
    max_select = MAX_SELECT;
    select_option = New_N(FormSelectOption, max_select);
#endif /* MENU_SELECT */
    cur_select = NULL;
    form_sp = -1;
    form_max = -1;
    forms_size = 0;
    forms = NULL;
    cur_hseq = 1;
#ifdef USE_IMAGE
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
#endif

    if (w3mApp::Instance().w3m_halfload)
    {
        newBuf->buffername = "---";
#ifdef USE_M17N
        newBuf->document_charset = w3mApp::Instance().InnerCharset;
#endif
        max_textarea = 0;
#ifdef MENU_SELECT
        max_select = 0;
#endif
        HTMLlineproc3(newBuf, f->stream);
        w3mApp::Instance().w3m_halfload = FALSE;
        return;
    }

    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);

    if (w3m_halfdump)
        htmlenv1.f = stdout;
    else
        htmlenv1.buf = newTextLineList();

    auto success = TrapJmp([&]() {

#ifdef USE_M17N
        if (newBuf != NULL)
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
#if defined(USE_M17N) && defined(USE_IMAGE)
            cur_document_charset = charset;
#endif
            HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
        }
        if (obuf.status != R_ST_NORMAL)
        {
            obuf.status = R_ST_EOL;
            HTMLlineproc0("\n", &htmlenv1, internal);
        }
        obuf.status = R_ST_NORMAL;
        completeHTMLstream(&htmlenv1, &obuf);
        flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
        if (htmlenv1.title)
            newBuf->buffername = htmlenv1.title;

        return true;
    });

    if (!success)
    {
        HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1);
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
#ifdef USE_M17N
    if (!(newBuf->bufferprop & BP_FRAME))
        newBuf->document_charset = charset;
#endif
#ifdef USE_IMAGE
    newBuf->image_flag = image_flag;
#endif
    HTMLlineproc2(newBuf, htmlenv1.buf);
}

/* 
 * loadHTMLBuffer: read file and make new buffer
 */
BufferPtr
loadHTMLBuffer(URLFile *f, BufferPtr newBuf)
{
    FILE *src = NULL;
    Str tmp;

    if (newBuf == NULL)
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

    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

/* 
 * loadHTMLString: read string and make new buffer
 */
BufferPtr
loadHTMLString(Str page)
{
    MySignalHandler prevtrap = NULL;
    BufferPtr newBuf;

    newBuf = newBuffer(INIT_BUFFER_WIDTH());

    auto success = TrapJmp([&]() {
        URLFile f(SCM_LOCAL, newStrStream(page));

        newBuf->document_charset = w3mApp::Instance().InnerCharset;
        loadHTMLstream(&f, newBuf, NULL, TRUE);
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
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    return newBuf;
}

int next_status(char c, int *status)
{
    switch (*status)
    {
    case R_ST_NORMAL:
        if (c == '<')
        {
            *status = R_ST_TAG0;
            return 0;
        }
        else if (c == '&')
        {
            *status = R_ST_AMP;
            return 1;
        }
        else
            return 1;
        break;
    case R_ST_TAG0:
        if (c == '!')
        {
            *status = R_ST_CMNT1;
            return 0;
        }
        *status = R_ST_TAG;
        /* continues to next case */
    case R_ST_TAG:
        if (c == '>')
            *status = R_ST_NORMAL;
        else if (c == '=')
            *status = R_ST_EQL;
        return 0;
    case R_ST_EQL:
        if (c == '"')
            *status = R_ST_DQUOTE;
        else if (c == '\'')
            *status = R_ST_QUOTE;
        else if (IS_SPACE(c))
            *status = R_ST_EQL;
        else if (c == '>')
            *status = R_ST_NORMAL;
        else
            *status = R_ST_VALUE;
        return 0;
    case R_ST_QUOTE:
        if (c == '\'')
            *status = R_ST_TAG;
        return 0;
    case R_ST_DQUOTE:
        if (c == '"')
            *status = R_ST_TAG;
        return 0;
    case R_ST_VALUE:
        if (c == '>')
            *status = R_ST_NORMAL;
        else if (IS_SPACE(c))
            *status = R_ST_TAG;
        return 0;
    case R_ST_AMP:
        if (c == ';')
        {
            *status = R_ST_NORMAL;
            return 0;
        }
        else if (c != '#' && !IS_ALNUM(c) && c != '_')
        {
            /* something's wrong! */
            *status = R_ST_NORMAL;
            return 0;
        }
        else
            return 0;
    case R_ST_CMNT1:
        switch (c)
        {
        case '-':
            *status = R_ST_CMNT2;
            break;
        case '>':
            *status = R_ST_NORMAL;
            break;
        default:
            *status = R_ST_IRRTAG;
        }
        return 0;
    case R_ST_CMNT2:
        switch (c)
        {
        case '-':
            *status = R_ST_CMNT;
            break;
        case '>':
            *status = R_ST_NORMAL;
            break;
        default:
            *status = R_ST_IRRTAG;
        }
        return 0;
    case R_ST_CMNT:
        if (c == '-')
            *status = R_ST_NCMNT1;
        return 0;
    case R_ST_NCMNT1:
        if (c == '-')
            *status = R_ST_NCMNT2;
        else
            *status = R_ST_CMNT;
        return 0;
    case R_ST_NCMNT2:
        switch (c)
        {
        case '>':
            *status = R_ST_NORMAL;
            break;
        case '-':
            *status = R_ST_NCMNT2;
            break;
        default:
            if (IS_SPACE(c))
                *status = R_ST_NCMNT3;
            else
                *status = R_ST_CMNT;
            break;
        }
        break;
    case R_ST_NCMNT3:
        switch (c)
        {
        case '>':
            *status = R_ST_NORMAL;
            break;
        case '-':
            *status = R_ST_NCMNT1;
            break;
        default:
            if (IS_SPACE(c))
                *status = R_ST_NCMNT3;
            else
                *status = R_ST_CMNT;
            break;
        }
        return 0;
    case R_ST_IRRTAG:
        if (c == '>')
            *status = R_ST_NORMAL;
        return 0;
    }
    /* notreached */
    return 0;
}

int read_token(Str buf, char **instr, int *status, int pre, int append)
{
    char *p;
    int prev_status;

    if (!append)
        buf->Clear();
    if (**instr == '\0')
        return 0;
    for (p = *instr; *p; p++)
    {
        prev_status = *status;
        next_status(*p, status);
        switch (*status)
        {
        case R_ST_NORMAL:
            if (prev_status == R_ST_AMP && *p != ';')
            {
                p--;
                break;
            }
            if (prev_status == R_ST_NCMNT2 || prev_status == R_ST_NCMNT3 ||
                prev_status == R_ST_IRRTAG || prev_status == R_ST_CMNT1)
            {
                if (prev_status == R_ST_CMNT1 && !append && !pre)
                    buf->Clear();
                if (pre)
                    buf->Push(*p);
                p++;
                goto proc_end;
            }
            buf->Push((!pre && IS_SPACE(*p)) ? ' ' : *p);
            if (ST_IS_REAL_TAG(prev_status))
            {
                *instr = p + 1;
                if (buf->Size() < 2 ||
                    buf->ptr[buf->Size() - 2] != '<' ||
                    buf->ptr[buf->Size() - 1] != '>')
                    return 1;
                buf->Pop(2);
            }
            break;
        case R_ST_TAG0:
        case R_ST_TAG:
            if (prev_status == R_ST_NORMAL && p != *instr)
            {
                *instr = p;
                *status = prev_status;
                return 1;
            }
            if (*status == R_ST_TAG0 && !REALLY_THE_BEGINNING_OF_A_TAG(p))
            {
                /* it seems that this '<' is not a beginning of a tag */
                /*
                 * buf->Push( "&lt;");
                 */
                buf->Push('<');
                *status = R_ST_NORMAL;
            }
            else
                buf->Push(*p);
            break;
        case R_ST_EQL:
        case R_ST_QUOTE:
        case R_ST_DQUOTE:
        case R_ST_VALUE:
        case R_ST_AMP:
            buf->Push(*p);
            break;
        case R_ST_CMNT:
        case R_ST_IRRTAG:
            if (pre)
                buf->Push(*p);
            else if (!append)
                buf->Clear();
            break;
        case R_ST_CMNT1:
        case R_ST_CMNT2:
        case R_ST_NCMNT1:
        case R_ST_NCMNT2:
        case R_ST_NCMNT3:
            /* do nothing */
            if (pre)
                buf->Push(*p);
            break;
        }
    }
proc_end:
    *instr = p;
    return 1;
}

Str correct_irrtag(int status)
{
    char c;
    Str tmp = Strnew();

    while (status != R_ST_NORMAL)
    {
        switch (status)
        {
        case R_ST_CMNT:   /* required "-->" */
        case R_ST_NCMNT1: /* required "->" */
            c = '-';
            break;
        case R_ST_NCMNT2:
        case R_ST_NCMNT3:
        case R_ST_IRRTAG:
        case R_ST_CMNT1:
        case R_ST_CMNT2:
        case R_ST_TAG:
        case R_ST_TAG0:
        case R_ST_EQL: /* required ">" */
        case R_ST_VALUE:
            c = '>';
            break;
        case R_ST_QUOTE:
            c = '\'';
            break;
        case R_ST_DQUOTE:
            c = '"';
            break;
        case R_ST_AMP:
            c = ';';
            break;
        default:
            return tmp;
        }
        next_status(c, &status);
        tmp->Push(c);
    }
    return tmp;
}
