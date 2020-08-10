#include "tagstack.h"
#include "indep.h"
#include "gc_helper.h"
#include "html.h"
#include "fm.h"
#include "file.h"
#include "html/html.h"
#include "table.h"
#include "myctype.h"
#include "file.h"
#include "entity.h"
#include "symbol.h"
#include "ctrlcode.h"
#include "html/html_processor.h"
#include "html/textarea.h"
#include "html/html_form.h"
#include "html/html_sequence.h"
#include "html/tokenizer.h"
#include "frontend/buffer.h"
#include "frontend/line.h"
#include "charset.h"

#define in_bold fontstat[0]
#define in_under fontstat[1]
#define in_italic fontstat[2]
#define in_strike fontstat[3]
#define in_ins fontstat[4]
#define in_stand fontstat[5]

void Breakpoint::set(const struct readbuffer *obuf, int tag_length)
{
    _len = obuf->line->Size();
    _tlen = tag_length;

    _pos = obuf->pos;
    flag = obuf->flag;
#ifdef FORMAT_NICE
    flag &= ~RB_FILL;
#endif /* FORMAT_NICE */
    top_margin = obuf->top_margin;
    bottom_margin = obuf->bottom_margin;

    if (init_flag)
    {
        init_flag = 0;

        anchor = obuf->anchor;
        img_alt = obuf->img_alt;
        in_bold = obuf->in_bold;
        in_italic = obuf->in_italic;
        in_under = obuf->in_under;
        in_strike = obuf->in_strike;
        in_ins = obuf->in_ins;
        nobr_level = obuf->nobr_level;
        prev_ctype = obuf->prev_ctype;
    }
}

void Breakpoint::back_to(struct readbuffer *obuf)
{
    obuf->pos = _pos;
    obuf->flag = flag;
    obuf->top_margin = top_margin;
    obuf->bottom_margin = bottom_margin;

    obuf->anchor = anchor;
    obuf->img_alt = img_alt;
    obuf->in_bold = in_bold;
    obuf->in_italic = in_italic;
    obuf->in_under = in_under;
    obuf->in_strike = in_strike;
    obuf->in_ins = in_ins;
    obuf->prev_ctype = prev_ctype;
    if (obuf->flag & RB_NOBR)
        obuf->nobr_level = nobr_level;
}

// #define set_prevchar(x,y,n) Strcopy_charp_n((x),(y),(n))
static inline void set_space_to_prevchar(Str x)
{
    x->CopyFrom(" ", 1);
}

static struct table *tables[MAX_TABLE];
static struct table_mode table_mode[MAX_TABLE];

static Str cur_title;
void SetCurTitle(Str title)
{
    cur_title = title;
}

static Str
process_title(struct parsed_tag *tag)
{
    cur_title = Strnew();
    return NULL;
}

static Str
process_n_title(struct parsed_tag *tag)
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

static void
feed_title(const char *str)
{
    if (!cur_title)
        return;
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
            cur_title->Push(*(str++));
    }
}

struct link_stack
{
    HtmlTags cmd;
    short offset;
    short pos;
    struct link_stack *next;
};

static struct link_stack *link_stack = NULL;

static char *
has_hidden_link(struct readbuffer *obuf, int cmd)
{
    Str line = obuf->line;
    struct link_stack *p;

    if (line->Back() != '>')
        return NULL;

    for (p = link_stack; p; p = p->next)
        if (p->cmd == cmd)
            break;
    if (!p)
        return NULL;

    if (obuf->pos == p->pos)
        return line->ptr + p->offset;

    return NULL;
}

static void
push_link(HtmlTags cmd, int offset, int pos)
{
    struct link_stack *p;
    p = New(struct link_stack);
    p->cmd = cmd;
    p->offset = offset;
    p->pos = pos;
    p->next = link_stack;
    link_stack = p;
}

static void
append_tags(struct readbuffer *obuf)
{
    int i;
    int len = obuf->line->Size();
    int set_bp = 0;

    for (i = 0; i < obuf->tag_sp; i++)
    {
        switch (obuf->tag_stack[i]->cmd)
        {
        case HTML_A:
        case HTML_IMG_ALT:
        case HTML_B:
        case HTML_U:
        case HTML_I:
        case HTML_S:
            push_link(obuf->tag_stack[i]->cmd, obuf->line->Size(), obuf->pos);
            break;
        }
        obuf->line->Push(obuf->tag_stack[i]->cmdname);
        switch (obuf->tag_stack[i]->cmd)
        {
        case HTML_NOBR:
            if (obuf->nobr_level > 1)
                break;
        case HTML_WBR:
            set_bp = 1;
            break;
        }
    }
    obuf->tag_sp = 0;
    if (set_bp)
        obuf->bp.set(obuf, obuf->line->Size() - len);
}

static void
push_tag(struct readbuffer *obuf, char *cmdname, HtmlTags cmd)
{
    obuf->tag_stack[obuf->tag_sp] = New(struct cmdtable);
    obuf->tag_stack[obuf->tag_sp]->cmdname = allocStr(cmdname, -1);
    obuf->tag_stack[obuf->tag_sp]->cmd = cmd;
    obuf->tag_sp++;
    if (obuf->tag_sp >= TAG_STACK_SIZE || obuf->flag & (RB_SPECIAL & ~RB_NOBR))
        append_tags(obuf);
}

static void
push_nchars(struct readbuffer *obuf, int width,
            char *str, int len, Lineprop mode)
{
    append_tags(obuf);
    obuf->line->Push(str, len);
    obuf->pos += width;
    if (width > 0)
    {
        obuf->prevchar->CopyFrom(str, len);
        obuf->prev_ctype = mode;
    }
    obuf->flag |= RB_NFLUSHED;
}

#define push_charp(obuf, width, str, mode) \
    push_nchars(obuf, width, str, strlen(str), mode)

#define push_str(obuf, width, str, mode) \
    push_nchars(obuf, width, str->ptr, str->Size(), mode)

static void
check_breakpoint(struct readbuffer *obuf, int pre_mode, char *ch)
{
    int tlen, len = obuf->line->Size();

    append_tags(obuf);
    if (pre_mode)
        return;
    tlen = obuf->line->Size() - len;
    if (tlen > 0 || is_boundary((unsigned char *)obuf->prevchar->ptr,
                                (unsigned char *)ch))
        obuf->bp.set(obuf, tlen);
}

static void
push_char(struct readbuffer *obuf, int pre_mode, char ch)
{
    check_breakpoint(obuf, pre_mode, &ch);
    obuf->line->Push(ch);
    obuf->pos++;
    obuf->prevchar->CopyFrom(&ch, 1);
    if (ch != ' ')
        obuf->prev_ctype = PC_ASCII;
    obuf->flag |= RB_NFLUSHED;
}

#define PUSH(c) push_char(obuf, obuf->flag &RB_SPECIAL, c)

static void
push_spaces(struct readbuffer *obuf, int pre_mode, int width)
{
    int i;

    if (width <= 0)
        return;
    check_breakpoint(obuf, pre_mode, " ");
    for (i = 0; i < width; i++)
        obuf->line->Push(' ');
    obuf->pos += width;
    set_space_to_prevchar(obuf->prevchar);
    obuf->flag |= RB_NFLUSHED;
}

static void
proc_mchar(struct readbuffer *obuf, int pre_mode,
           int width, char **str, Lineprop mode)
{
    check_breakpoint(obuf, pre_mode, *str);
    obuf->pos += width;
    obuf->line->Push(*str, get_mclen(*str));
    if (width > 0)
    {
        obuf->prevchar->CopyFrom(*str, 1);
        if (**str != ' ')
            obuf->prev_ctype = mode;
    }
    (*str) += get_mclen(*str);
    obuf->flag |= RB_NFLUSHED;
}

static int
sloppy_parse_line(char **str)
{
    if (**str == '<')
    {
        while (**str && **str != '>')
            (*str)++;
        if (**str == '>')
            (*str)++;
        return 1;
    }
    else
    {
        while (**str && **str != '<')
            (*str)++;
        return 0;
    }
}

static void fillline(struct readbuffer *obuf, int indent)
{
    push_spaces(obuf, 1, indent - obuf->pos);
    obuf->flag &= ~RB_NFLUSHED;
}

#define MAX_CMD_LEN 128

static HtmlTags gethtmlcmd(char **s)
{
    char cmdstr[MAX_CMD_LEN];
    char *p = cmdstr;
    char *save = *s;

    (*s)++;
    /* first character */
    if (IS_ALNUM(**s) || **s == '_' || **s == '/')
    {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    else
        return HTML_UNKNOWN;
    if (p[-1] == '/')
        SKIP_BLANKS(s);
    while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN)
    {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    if (p - cmdstr == MAX_CMD_LEN)
    {
        /* buffer overflow: perhaps caused by bad HTML source */
        *s = save + 1;
        return HTML_UNKNOWN;
    }
    *p = '\0';

    /* hash search */
    //     extern Hash_si tagtable;
    //     int cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
    auto cmd = GetTag(cmdstr, HTML_UNKNOWN);
    while (**s && **s != '>')
        (*s)++;
    if (**s == '>')
        (*s)++;
    return cmd;
}

static void
passthrough(struct readbuffer *obuf, char *str, int back)
{
    Str tok = Strnew();
    char *str_bak;

    if (back)
    {
        Str str_save = Strnew(str);
        obuf->line->Pop(obuf->line->ptr + obuf->line->Size() - str);
        str = str_save->ptr;
    }
    while (*str)
    {
        str_bak = str;
        if (sloppy_parse_line(&str))
        {
            char *q = str_bak;
            auto cmd = gethtmlcmd(&q);
            if (back)
            {
                struct link_stack *p;
                for (p = link_stack; p; p = p->next)
                {
                    if (p->cmd == cmd)
                    {
                        link_stack = p->next;
                        break;
                    }
                }
                back = 0;
            }
            else
            {
                tok->Push(str_bak, str - str_bak);
                push_tag(obuf, tok->ptr, cmd);
                tok->Clear();
            }
        }
        else
        {
            push_nchars(obuf, 0, str_bak, str - str_bak, obuf->prev_ctype);
        }
    }
}

static int
close_effect0(struct readbuffer *obuf, int cmd)
{
    int i;
    char *p;

    for (i = obuf->tag_sp - 1; i >= 0; i--)
    {
        if (obuf->tag_stack[i]->cmd == cmd)
            break;
    }
    if (i >= 0)
    {
        obuf->tag_sp--;
        bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
              (obuf->tag_sp - i) * sizeof(struct cmdtable *));
        return 1;
    }
    else if ((p = has_hidden_link(obuf, cmd)) != NULL)
    {
        passthrough(obuf, p, 1);
        return 1;
    }
    return 0;
}

static void
clear_ignore_p_flag(int cmd, struct readbuffer *obuf)
{
    static int clear_flag_cmd[] = {
        HTML_HR, HTML_UNKNOWN};
    int i;

    for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++)
    {
        if (cmd == clear_flag_cmd[i])
        {
            obuf->flag &= ~RB_IGNORE_P;
            return;
        }
    }
}

static void
set_alignment(struct readbuffer *obuf, struct parsed_tag *tag)
{
    ReadBufferFlags flag = (ReadBufferFlags)-1;
    int align;

    if (tag->TryGetAttributeValue(ATTR_ALIGN, &align))
    {
        switch (align)
        {
        case ALIGN_CENTER:
            flag = RB_CENTER;
            break;
        case ALIGN_RIGHT:
            flag = RB_RIGHT;
            break;
        case ALIGN_LEFT:
            flag = RB_LEFT;
        }
    }
    RB_SAVE_FLAG(obuf);
    if (flag != -1)
    {
        RB_SET_ALIGN(obuf, flag);
    }
}

#ifdef ID_EXT
static void
process_idattr(struct readbuffer *obuf, int cmd, struct parsed_tag *tag)
{
    char *id = NULL, *framename = NULL;
    Str idtag = NULL;

    /* 
     * HTML_TABLE is handled by the other process.
     */
    if (cmd == HTML_TABLE)
        return;

    tag->TryGetAttributeValue(ATTR_ID, &id);
    tag->TryGetAttributeValue(ATTR_FRAMENAME, &framename);
    if (id == NULL)
        return;
    if (framename)
        idtag = Sprintf("<_id id=\"%s\" framename=\"%s\">",
                        html_quote(id), html_quote(framename));
    else
        idtag = Sprintf("<_id id=\"%s\">", html_quote(id));
    push_tag(obuf, idtag->ptr, HTML_NOP);
}
#endif /* ID_EXT */

#define CLOSE_P                                                              \
    if (obuf->flag & RB_P)                                                   \
    {                                                                        \
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit); \
        RB_RESTORE_FLAG(obuf);                                               \
        obuf->flag &= ~RB_P;                                                 \
    }

#define CLOSE_A \
    CLOSE_P;    \
    close_anchor(h_env, obuf, seq);

#define CLOSE_DT                           \
    if (obuf->flag & RB_IN_DT)             \
    {                                      \
        obuf->flag &= ~RB_IN_DT;           \
        HTMLlineproc1("</b>", h_env, seq); \
    }

void html_feed_environ::Initialize(TextLineList *buf, readbuffer *obuf, int limit, environment *envs, int nenv)
{
    this->buf = buf;
    this->f = NULL;
    this->obuf = obuf;
    this->tagbuf = Strnew();
    this->limit = limit;
    this->maxlimit = 0;
    this->envs = envs;
    this->nenv = nenv;
    this->envc = 0;
    this->envc_real = 0;
    this->title = NULL;
    this->blank_lines = 0;
}

void html_feed_environ::PUSH_ENV(unsigned char cmd)
{
    if (++envc_real < nenv)
    {
        ++envc;
        envs[envc].env = cmd;
        envs[envc].count = 0;
        if (envc <= MAX_INDENT_LEVEL)
            envs[envc].indent = envs[envc - 1].indent + INDENT_INCR;
        else
            envs[envc].indent = envs[envc - 1].indent;
    }
}

void html_feed_environ::POP_ENV()
{
    if (envc_real-- < nenv)
    {
        envc--;
    }
}

static void
proc_escape(struct readbuffer *obuf, char **str_return)
{
    char *str = *str_return, *estr;
    int ech = ucs4_from_entity(str_return);
    int width, n_add = *str_return - str;
    Lineprop mode = PC_ASCII;

    if (ech < 0)
    {
        *str_return = str;
        proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
        return;
    }
    mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

    estr = (char *)from_unicode(ech, w3mApp::Instance().InnerCharset);
    check_breakpoint(obuf, obuf->flag & RB_SPECIAL, estr);
    width = get_strwidth(estr);
    if (width == 1 && ech == (unsigned char)*estr &&
        ech != '&' && ech != '<' && ech != '>')
    {
        if (IS_CNTRL(ech))
            mode = PC_CTRL;
        push_charp(obuf, width, estr, mode);
    }
    else
        push_nchars(obuf, width, str, n_add, mode);
    obuf->prevchar->CopyFrom(estr, strlen(estr));
    obuf->prev_ctype = mode;
}

void push_render_image(Str str, int width, int limit,
                       struct html_feed_environ *h_env)
{
    struct readbuffer *obuf = h_env->obuf;
    int indent = h_env->currentEnv().indent;

    push_spaces(obuf, 1, (limit - width) / 2);
    push_str(obuf, width, str, PC_ASCII);
    push_spaces(obuf, 1, (limit - width + 1) / 2);
    if (width > 0)
        flushline(h_env, obuf, indent, 0, h_env->limit);
}

void flushline(struct html_feed_environ *h_env, struct readbuffer *obuf, int indent,
               int force, int width)
{
    TextLineList *buf = h_env->buf;
    FILE *f = h_env->f;
    Str line = obuf->line, pass = NULL;
    char *hidden_anchor = NULL, *hidden_img = NULL, *hidden_bold = NULL,
         *hidden_under = NULL, *hidden_italic = NULL, *hidden_strike = NULL,
         *hidden_ins = NULL, *hidden = NULL;

#ifdef DEBUG
    if (w3m_debug)
    {
        FILE *df = fopen("zzzproc1", "a");
        fprintf(df, "flushline(%s,%d,%d,%d)\n", obuf->line->ptr, indent, force,
                width);
        if (buf)
        {
            TextLineListItem *p;
            for (p = buf->first; p; p = p->next)
            {
                fprintf(df, "buf=\"%s\"\n", p->ptr->line->ptr);
            }
        }
        fclose(df);
    }
#endif

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && line->Back() == ' ')
    {
        line->Pop(1);
        obuf->pos--;
    }

    append_tags(obuf);

    if (obuf->anchor.url.size())
        hidden = hidden_anchor = has_hidden_link(obuf, HTML_A);
    if (obuf->img_alt)
    {
        if ((hidden_img = has_hidden_link(obuf, HTML_IMG_ALT)) != NULL)
        {
            if (!hidden || hidden_img < hidden)
                hidden = hidden_img;
        }
    }
    if (obuf->in_bold)
    {
        if ((hidden_bold = has_hidden_link(obuf, HTML_B)) != NULL)
        {
            if (!hidden || hidden_bold < hidden)
                hidden = hidden_bold;
        }
    }
    if (obuf->in_italic)
    {
        if ((hidden_italic = has_hidden_link(obuf, HTML_I)) != NULL)
        {
            if (!hidden || hidden_italic < hidden)
                hidden = hidden_italic;
        }
    }
    if (obuf->in_under)
    {
        if ((hidden_under = has_hidden_link(obuf, HTML_U)) != NULL)
        {
            if (!hidden || hidden_under < hidden)
                hidden = hidden_under;
        }
    }
    if (obuf->in_strike)
    {
        if ((hidden_strike = has_hidden_link(obuf, HTML_S)) != NULL)
        {
            if (!hidden || hidden_strike < hidden)
                hidden = hidden_strike;
        }
    }
    if (obuf->in_ins)
    {
        if ((hidden_ins = has_hidden_link(obuf, HTML_INS)) != NULL)
        {
            if (!hidden || hidden_ins < hidden)
                hidden = hidden_ins;
        }
    }
    if (hidden)
    {
        pass = Strnew(hidden);
        line->Pop(line->ptr + line->Size() - hidden);
    }

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && obuf->pos > width)
    {
        char *tp = &line->ptr[obuf->bp.len() - obuf->bp.tlen()];
        char *ep = &line->ptr[line->Size()];

        if (obuf->bp.pos() == obuf->pos && tp <= ep &&
            tp > line->ptr && tp[-1] == ' ')
        {
            bcopy(tp, tp - 1, ep - tp + 1);
            line->Pop(1);
            obuf->pos--;
        }
    }

    if (obuf->anchor.url.size() && !hidden_anchor)
        line->Push("</a>");
    if (obuf->img_alt && !hidden_img)
        line->Push("</img_alt>");
    if (obuf->in_bold && !hidden_bold)
        line->Push("</b>");
    if (obuf->in_italic && !hidden_italic)
        line->Push("</i>");
    if (obuf->in_under && !hidden_under)
        line->Push("</u>");
    if (obuf->in_strike && !hidden_strike)
        line->Push("</s>");
    if (obuf->in_ins && !hidden_ins)
        line->Push("</ins>");

    if (obuf->top_margin > 0)
    {
        int i;
        struct html_feed_environ h;
        struct readbuffer o;
        struct environment e[1];

        init_henv(&h, &o, e, 1, NULL, width, indent);
        o.line = Strnew_size(width + 20);
        o.pos = obuf->pos;
        o.flag = obuf->flag;
        o.top_margin = -1;
        o.bottom_margin = -1;
        o.line->Push("<pre_int>");
        for (i = 0; i < o.pos; i++)
            o.line->Push(' ');
        o.line->Push("</pre_int>");
        for (i = 0; i < obuf->top_margin; i++)
            flushline(h_env, &o, indent, force, width);
    }

    if (force == 1 || obuf->flag & RB_NFLUSHED)
    {
        TextLine *lbuf = newTextLine(line, obuf->pos);
        if (RB_GET_ALIGN(obuf) == RB_CENTER)
        {
            align(lbuf, width, ALIGN_CENTER);
        }
        else if (RB_GET_ALIGN(obuf) == RB_RIGHT)
        {
            align(lbuf, width, ALIGN_RIGHT);
        }
        else if (RB_GET_ALIGN(obuf) == RB_LEFT && obuf->flag & RB_INTABLE)
        {
            align(lbuf, width, ALIGN_LEFT);
        }
#ifdef FORMAT_NICE
        else if (obuf->flag & RB_FILL)
        {
            char *p;
            int rest, rrest;
            int nspace, d, i;

            rest = width - get_Str_strwidth(line);
            if (rest > 1)
            {
                nspace = 0;
                for (p = line->ptr + indent; *p; p++)
                {
                    if (*p == ' ')
                        nspace++;
                }
                if (nspace > 0)
                {
                    int indent_here = 0;
                    d = rest / nspace;
                    p = line->ptr;
                    while (IS_SPACE(*p))
                    {
                        p++;
                        indent_here++;
                    }
                    rrest = rest - d * nspace;
                    line = Strnew_size(width + 1);
                    for (i = 0; i < indent_here; i++)
                        line->Push(' ');
                    for (; *p; p++)
                    {
                        line->Push(*p);
                        if (*p == ' ')
                        {
                            for (i = 0; i < d; i++)
                                line->Push(' ');
                            if (rrest > 0)
                            {
                                line->Push(' ');
                                rrest--;
                            }
                        }
                    }
                    lbuf = newTextLine(line, width);
                }
            }
        }
#endif /* FORMAT_NICE */
#ifdef TABLE_DEBUG
        if (w3m_debug)
        {
            FILE *f = fopen("zzzproc1", "a");
            fprintf(f, "pos=%d,%d, maxlimit=%d\n",
                    visible_length(lbuf->line->ptr), lbuf->pos,
                    h_env->maxlimit);
            fclose(f);
        }
#endif
        if (lbuf->pos > h_env->maxlimit)
            h_env->maxlimit = lbuf->pos;
        if (buf)
            pushTextLine(buf, lbuf);
        else if (f)
        {
            Str_conv_to_halfdump(lbuf->line)->Puts(f);
            fputc('\n', f);
        }
        if (obuf->flag & RB_SPECIAL || obuf->flag & RB_NFLUSHED)
            h_env->blank_lines = 0;
        else
            h_env->blank_lines++;
    }
    else
    {
        char *p = line->ptr, *q;
        Str tmp = Strnew(), tmp2 = Strnew();

#define APPEND(str)                    \
    if (buf)                           \
        appendTextLine(buf, (str), 0); \
    else if (f)                        \
    (str)->Puts(f)

        while (*p)
        {
            q = p;
            if (sloppy_parse_line(&p))
            {
                tmp->Push(q, p - q);
                if (force == 2)
                {
                    APPEND(tmp);
                }
                else
                    tmp2->Push(tmp);
                tmp->Clear();
            }
        }
        if (force == 2)
        {
            if (pass)
            {
                APPEND(pass);
            }
            pass = NULL;
        }
        else
        {
            if (pass)
                tmp2->Push(pass);
            pass = tmp2;
        }
    }

    if (obuf->bottom_margin > 0)
    {
        int i;
        struct html_feed_environ h;
        struct readbuffer o;
        struct environment e[1];

        init_henv(&h, &o, e, 1, NULL, width, indent);
        o.line = Strnew_size(width + 20);
        o.pos = obuf->pos;
        o.flag = obuf->flag;
        o.top_margin = -1;
        o.bottom_margin = -1;
        o.line->Push("<pre_int>");
        for (i = 0; i < o.pos; i++)
            o.line->Push(' ');
        o.line->Push("</pre_int>");
        for (i = 0; i < obuf->bottom_margin; i++)
            flushline(h_env, &o, indent, force, width);
    }
    if (obuf->top_margin < 0 || obuf->bottom_margin < 0)
        return;

    obuf->line = Strnew_size(256);
    obuf->pos = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    set_space_to_prevchar(obuf->prevchar);
    obuf->bp.initialize();
    obuf->flag &= ~RB_NFLUSHED;
    obuf->bp.set(obuf, 0);
    obuf->prev_ctype = PC_ASCII;
    link_stack = NULL;
    fillline(obuf, indent);
    if (pass)
        passthrough(obuf, pass->ptr, 0);
    if (!hidden_anchor && obuf->anchor.url.size())
    {
        Str tmp;
        if (obuf->anchor.hseq > 0)
            obuf->anchor.hseq = -obuf->anchor.hseq;
        tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", obuf->anchor.hseq);
        tmp->Push(html_quote(obuf->anchor.url));
        if (obuf->anchor.target.size())
        {
            tmp->Push("\" TARGET=\"");
            tmp->Push(html_quote(obuf->anchor.target));
        }
        if (obuf->anchor.referer.size())
        {
            tmp->Push("\" REFERER=\"");
            tmp->Push(html_quote(obuf->anchor.referer));
        }
        if (obuf->anchor.title.size())
        {
            tmp->Push("\" TITLE=\"");
            tmp->Push(html_quote(obuf->anchor.title));
        }
        if (obuf->anchor.accesskey)
        {
            char *c = html_quote_char(obuf->anchor.accesskey);
            tmp->Push("\" ACCESSKEY=\"");
            if (c)
                tmp->Push(c);
            else
                tmp->Push(obuf->anchor.accesskey);
        }
        tmp->Push("\">");
        push_tag(obuf, tmp->ptr, HTML_A);
    }
    if (!hidden_img && obuf->img_alt)
    {
        Str tmp = Strnew("<IMG_ALT SRC=\"");
        tmp->Push(html_quote(obuf->img_alt->ptr));
        tmp->Push("\">");
        push_tag(obuf, tmp->ptr, HTML_IMG_ALT);
    }
    if (!hidden_bold && obuf->in_bold)
        push_tag(obuf, "<B>", HTML_B);
    if (!hidden_italic && obuf->in_italic)
        push_tag(obuf, "<I>", HTML_I);
    if (!hidden_under && obuf->in_under)
        push_tag(obuf, "<U>", HTML_U);
    if (!hidden_strike && obuf->in_strike)
        push_tag(obuf, "<S>", HTML_S);
    if (!hidden_ins && obuf->in_ins)
        push_tag(obuf, "<INS>", HTML_INS);
}

void do_blankline(struct html_feed_environ *h_env, struct readbuffer *obuf,
                  int indent, int indent_incr, int width)
{
    if (h_env->blank_lines == 0)
        flushline(h_env, obuf, indent, 1, width);
}

void purgeline(struct html_feed_environ *h_env)
{
    char *p, *q;
    Str tmp;

    if (h_env->buf == NULL || h_env->blank_lines == 0)
        return;

    p = rpopTextLine(h_env->buf)->line->ptr;
    tmp = Strnew();
    while (*p)
    {
        q = p;
        if (sloppy_parse_line(&p))
        {
            tmp->Push(q, p - q);
        }
    }
    appendTextLine(h_env->buf, tmp, 0);
    h_env->blank_lines--;
}

static void
close_anchor(struct html_feed_environ *h_env, struct readbuffer *obuf, HSequence *seq)
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

        if (i >= 0 || (p = has_hidden_link(obuf, HTML_A)))
        {
            if (obuf->anchor.hseq > 0)
            {
                HTMLlineproc1(ANSP, h_env, seq);
                set_space_to_prevchar(obuf->prevchar);
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
                    passthrough(obuf, p, 1);
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

        push_tag(obuf, "</a>", HTML_N_A);
    }
    obuf->anchor = {};
}

void save_fonteffect(struct html_feed_environ *h_env, struct readbuffer *obuf)
{
    if (obuf->fontstat_sp < FONT_STACK_SIZE)
        bcopy(obuf->fontstat, obuf->fontstat_stack[obuf->fontstat_sp],
              FONTSTAT_SIZE);
    obuf->fontstat_sp++;
    if (obuf->in_bold)
        push_tag(obuf, "</b>", HTML_N_B);
    if (obuf->in_italic)
        push_tag(obuf, "</i>", HTML_N_I);
    if (obuf->in_under)
        push_tag(obuf, "</u>", HTML_N_U);
    if (obuf->in_strike)
        push_tag(obuf, "</s>", HTML_N_S);
    if (obuf->in_ins)
        push_tag(obuf, "</ins>", HTML_N_INS);
    bzero(obuf->fontstat, FONTSTAT_SIZE);
}

void restore_fonteffect(struct html_feed_environ *h_env, struct readbuffer *obuf)
{
    if (obuf->fontstat_sp > 0)
        obuf->fontstat_sp--;
    if (obuf->fontstat_sp < FONT_STACK_SIZE)
        bcopy(obuf->fontstat_stack[obuf->fontstat_sp], obuf->fontstat,
              FONTSTAT_SIZE);
    if (obuf->in_bold)
        push_tag(obuf, "<b>", HTML_B);
    if (obuf->in_italic)
        push_tag(obuf, "<i>", HTML_I);
    if (obuf->in_under)
        push_tag(obuf, "<u>", HTML_U);
    if (obuf->in_strike)
        push_tag(obuf, "<s>", HTML_S);
    if (obuf->in_ins)
        push_tag(obuf, "<ins>", HTML_INS);
}

static int
ul_type(struct parsed_tag *tag, int default_type)
{
    char *p;
    if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
    {
        if (!strcasecmp(p, "disc"))
            return (int)'d';
        else if (!strcasecmp(p, "circle"))
            return (int)'c';
        else if (!strcasecmp(p, "square"))
            return (int)'s';
    }
    return default_type;
}

#define REAL_WIDTH(w, limit) (((w) >= 0) ? (int)((w) / w3mApp::Instance().pixel_per_char) : -(w) * (limit) / 100)

static Str process_hr(struct parsed_tag *tag, int width, int indent_width, HSequence *seq)
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
    w /= seq->SymbolWidth();
    if (w <= 0)
        w = 1;
    push_symbol(tmp, HR_SYMBOL, seq->SymbolWidth(), w);
    tmp->Push("</div_int></nobr>");
    return tmp;
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

static Str
romanNum2(int l, int n)
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

Str romanNumeral(int n)
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

int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env, HSequence *seq)
{
    char *p, *q, *r;
    int i, w, x, y, z, count, width;
    struct readbuffer *obuf = h_env->obuf;
    // struct environment *envs = h_env->envs;
    Str tmp;
    int hseq;

#ifdef ID_EXT
    char *id = NULL;
#endif /* ID_EXT */

    int cmd = tag->tagid;

    if (obuf->flag & RB_PRE)
    {
        switch (cmd)
        {
        case HTML_NOBR:
        case HTML_N_NOBR:
        case HTML_PRE_INT:
        case HTML_N_PRE_INT:
            return 1;
        }
    }

    switch (cmd)
    {
    case HTML_B:
        obuf->in_bold++;
        if (obuf->in_bold > 1)
            return 1;
        return 0;
    case HTML_N_B:
        if (obuf->in_bold == 1 && close_effect0(obuf, HTML_B))
            obuf->in_bold = 0;
        if (obuf->in_bold > 0)
        {
            obuf->in_bold--;
            if (obuf->in_bold == 0)
                return 0;
        }
        return 1;
    case HTML_I:
        obuf->in_italic++;
        if (obuf->in_italic > 1)
            return 1;
        return 0;
    case HTML_N_I:
        if (obuf->in_italic == 1 && close_effect0(obuf, HTML_I))
            obuf->in_italic = 0;
        if (obuf->in_italic > 0)
        {
            obuf->in_italic--;
            if (obuf->in_italic == 0)
                return 0;
        }
        return 1;
    case HTML_U:
        obuf->in_under++;
        if (obuf->in_under > 1)
            return 1;
        return 0;
    case HTML_N_U:
        if (obuf->in_under == 1 && close_effect0(obuf, HTML_U))
            obuf->in_under = 0;
        if (obuf->in_under > 0)
        {
            obuf->in_under--;
            if (obuf->in_under == 0)
                return 0;
        }
        return 1;
    case HTML_EM:
        HTMLlineproc1("<i>", h_env, seq);
        return 1;
    case HTML_N_EM:
        HTMLlineproc1("</i>", h_env, seq);
        return 1;
    case HTML_STRONG:
        HTMLlineproc1("<b>", h_env, seq);
        return 1;
    case HTML_N_STRONG:
        HTMLlineproc1("</b>", h_env, seq);
        return 1;
    case HTML_Q:
        HTMLlineproc1("`", h_env, seq);
        return 1;
    case HTML_N_Q:
        HTMLlineproc1("'", h_env, seq);
        return 1;
    case HTML_P:
    case HTML_N_P:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 1, h_env->limit);
            do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                         h_env->limit);
        }
        obuf->flag |= RB_IGNORE_P;
        if (cmd == HTML_P)
        {
            set_alignment(obuf, tag);
            obuf->flag |= RB_P;
        }
        return 1;
    case HTML_BR:
        flushline(h_env, obuf, h_env->currentEnv().indent, 1, h_env->limit);
        h_env->blank_lines = 0;
        return 1;
    case HTML_H:
        if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
            do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                         h_env->limit);
        }
        HTMLlineproc1("<b>", h_env, seq);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_H:
        HTMLlineproc1("</b>", h_env, seq);
        if (!(obuf->flag & RB_PREMODE))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        }
        do_blankline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        close_anchor(h_env, obuf, seq);
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_UL:
    case HTML_OL:
    case HTML_BLQ:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
            if (!(obuf->flag & RB_PREMODE) &&
                (h_env->currentIndex() == 0 || cmd == HTML_BLQ))
                do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                             h_env->limit);
        }
        h_env->PUSH_ENV(cmd);
        if (cmd == HTML_UL || cmd == HTML_OL)
        {
            if (tag->TryGetAttributeValue(ATTR_START, &count))
            {
                h_env->currentEnv().count = count - 1;
            }
        }
        if (cmd == HTML_OL)
        {
            h_env->currentEnv().type = '1';
            if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
            {
                h_env->currentEnv().type = (int)*p;
            }
        }
        if (cmd == HTML_UL)
            h_env->currentEnv().type = ul_type(tag, 0);
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        return 1;
    case HTML_N_UL:
    case HTML_N_OL:
    case HTML_N_DL:
    case HTML_N_BLQ:
        CLOSE_DT;
        CLOSE_A;
        if (h_env->currentIndex() > 0)
        {
            flushline(h_env, obuf, h_env->prevEnv().indent, 0,
                      h_env->limit);
            h_env->POP_ENV();
            if (!(obuf->flag & RB_PREMODE) &&
                (h_env->currentIndex() == 0 || cmd == HTML_N_DL || cmd == HTML_N_BLQ))
            {
                do_blankline(h_env, obuf,
                             h_env->currentEnv().indent,
                             INDENT_INCR, h_env->limit);
                obuf->flag |= RB_IGNORE_P;
            }
        }
        close_anchor(h_env, obuf, seq);
        return 1;
    case HTML_DL:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
            if (!(obuf->flag & RB_PREMODE))
                do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                             h_env->limit);
        }
        h_env->PUSH_ENV(cmd);
        if (tag->HasAttribute(ATTR_COMPACT))
            h_env->currentEnv().env = HTML_DL_COMPACT;
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_LI:
        CLOSE_A;
        CLOSE_DT;
        if (h_env->currentIndex() > 0)
        {
            Str num;
            flushline(h_env, obuf,
                      h_env->prevEnv().indent, 0, h_env->limit);
            h_env->currentEnv().count++;
            if (tag->TryGetAttributeValue(ATTR_VALUE, &p))
            {
                count = atoi(p);
                if (count > 0)
                    h_env->currentEnv().count = count;
                else
                    h_env->currentEnv().count = 0;
            }
            switch (h_env->currentEnv().env)
            {
            case HTML_UL:
                h_env->currentEnv().type = ul_type(tag, h_env->currentEnv().type);
                for (i = 0; i < INDENT_INCR - 3; i++)
                    push_charp(obuf, 1, NBSP, PC_ASCII);
                tmp = Strnew();
                switch (h_env->currentEnv().type)
                {
                case 'd':
                    push_symbol(tmp, UL_SYMBOL_DISC, seq->SymbolWidth(), 1);
                    break;
                case 'c':
                    push_symbol(tmp, UL_SYMBOL_CIRCLE, seq->SymbolWidth(), 1);
                    break;
                case 's':
                    push_symbol(tmp, UL_SYMBOL_SQUARE, seq->SymbolWidth(), 1);
                    break;
                default:
                    push_symbol(tmp,
                                UL_SYMBOL((h_env->realIndex() - 1) % MAX_UL_LEVEL),
                                seq->SymbolWidth(),
                                1);
                    break;
                }
                if (seq->SymbolWidth() == 1)
                    push_charp(obuf, 1, NBSP, PC_ASCII);
                push_str(obuf, seq->SymbolWidth(), tmp, PC_ASCII);
                push_charp(obuf, 1, NBSP, PC_ASCII);
                set_space_to_prevchar(obuf->prevchar);
                break;
            case HTML_OL:
                if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
                    h_env->currentEnv().type = (int)*p;
                switch ((h_env->currentEnv().count > 0) ? h_env->currentEnv().type : '1')
                {
                case 'i':
                    num = romanNumeral(h_env->currentEnv().count);
                    break;
                case 'I':
                    num = romanNumeral(h_env->currentEnv().count);
                    ToUpper(num);
                    break;
                case 'a':
                    num = romanAlphabet(h_env->currentEnv().count);
                    break;
                case 'A':
                    num = romanAlphabet(h_env->currentEnv().count);
                    ToUpper(num);
                    break;
                default:
                    num = Sprintf("%d", h_env->currentEnv().count);
                    break;
                }
                if (INDENT_INCR >= 4)
                    num->Push(". ");
                else
                    num->Push('.');
                push_spaces(obuf, 1, INDENT_INCR - num->Size());
                push_str(obuf, num->Size(), num, PC_ASCII);
                if (INDENT_INCR >= 4)
                    set_space_to_prevchar(obuf->prevchar);
                break;
            default:
                push_spaces(obuf, 1, INDENT_INCR);
                break;
            }
        }
        else
        {
            flushline(h_env, obuf, 0, 0, h_env->limit);
        }
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_DT:
        CLOSE_A;
        if (h_env->currentIndex() == 0 ||
            (h_env->realIndex() < h_env->capacity() &&
             h_env->currentEnv().env != HTML_DL &&
             h_env->currentEnv().env != HTML_DL_COMPACT))
        {
            h_env->PUSH_ENV(HTML_DL);
        }
        if (h_env->currentIndex() > 0)
        {
            flushline(h_env, obuf,
                      h_env->prevEnv().indent, 0, h_env->limit);
        }
        if (!(obuf->flag & RB_IN_DT))
        {
            HTMLlineproc1("<b>", h_env, seq);
            obuf->flag |= RB_IN_DT;
        }
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_DD:
        CLOSE_A;
        CLOSE_DT;
        if (h_env->currentEnv().env == HTML_DL_COMPACT)
        {
            if (obuf->pos > h_env->currentEnv().indent)
                flushline(h_env, obuf, h_env->currentEnv().indent, 0,
                          h_env->limit);
            else
                push_spaces(obuf, 1, h_env->currentEnv().indent - obuf->pos);
        }
        else
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        /* obuf->flag |= RB_IGNORE_P; */
        return 1;
    case HTML_TITLE:
        close_anchor(h_env, obuf, seq);
        process_title(tag);
        obuf->flag |= RB_TITLE;
        obuf->end_tag = HTML_N_TITLE;
        return 1;
    case HTML_N_TITLE:
        if (!(obuf->flag & RB_TITLE))
            return 1;
        obuf->flag &= ~RB_TITLE;
        obuf->end_tag = 0;
        tmp = process_n_title(tag);
        if (tmp)
            HTMLlineproc1(tmp->ptr, h_env, seq);
        return 1;
    case HTML_TITLE_ALT:
        if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
            h_env->title = html_unquote(p, w3mApp::Instance().InnerCharset);
        return 0;
    case HTML_FRAMESET:
        h_env->PUSH_ENV(cmd);
        push_charp(obuf, 9, "--FRAME--", PC_ASCII);
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        return 0;
    case HTML_N_FRAMESET:
        if (h_env->currentIndex() > 0)
        {
            h_env->POP_ENV();
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        }
        return 0;
    case HTML_NOFRAMES:
        CLOSE_A;
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        obuf->flag |= (RB_NOFRAMES | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    case HTML_N_NOFRAMES:
        CLOSE_A;
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        obuf->flag &= ~RB_NOFRAMES;
        return 1;
    case HTML_FRAME:
        q = r = NULL;
        tag->TryGetAttributeValue(ATTR_SRC, &q);
        tag->TryGetAttributeValue(ATTR_NAME, &r);
        if (q)
        {
            q = html_quote(q);
            push_tag(obuf, Sprintf("<a hseq=\"%d\" href=\"%s\">", seq->Increment(), q)->ptr, HTML_A);
            if (r)
                q = html_quote(r);
            push_charp(obuf, get_strwidth(q), q, PC_ASCII);
            push_tag(obuf, "</a>", HTML_N_A);
        }
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        return 0;
    case HTML_HR:
        close_anchor(h_env, obuf, seq);
        tmp = process_hr(tag, h_env->limit, h_env->currentEnv().indent, seq);
        HTMLlineproc1(tmp->ptr, h_env, seq);
        set_space_to_prevchar(obuf->prevchar);
        return 1;
    case HTML_PRE:
        x = tag->HasAttribute(ATTR_FOR_TABLE);
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
            if (!x)
                do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                             h_env->limit);
        }
        else
            fillline(obuf, h_env->currentEnv().indent);
        obuf->flag |= (RB_PRE | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    case HTML_N_PRE:
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        if (!(obuf->flag & RB_IGNORE_P))
        {
            do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                         h_env->limit);
            obuf->flag |= RB_IGNORE_P;
            h_env->blank_lines++;
        }
        obuf->flag &= ~RB_PRE;
        close_anchor(h_env, obuf, seq);
        return 1;
    case HTML_PRE_INT:
        i = obuf->line->Size();
        append_tags(obuf);
        if (!(obuf->flag & RB_SPECIAL))
        {
            obuf->bp.set(obuf, obuf->line->Size() - i);
        }
        obuf->flag |= RB_PRE_INT;
        return 0;
    case HTML_N_PRE_INT:
        push_tag(obuf, "</pre_int>", HTML_N_PRE_INT);
        obuf->flag &= ~RB_PRE_INT;
        if (!(obuf->flag & RB_SPECIAL) && obuf->pos > obuf->bp.pos())
        {
            obuf->prevchar->CopyFrom("", 0);
            obuf->prev_ctype = PC_CTRL;
        }
        return 1;
    case HTML_NOBR:
        obuf->flag |= RB_NOBR;
        obuf->nobr_level++;
        return 0;
    case HTML_N_NOBR:
        if (obuf->nobr_level > 0)
            obuf->nobr_level--;
        if (obuf->nobr_level == 0)
            obuf->flag &= ~RB_NOBR;
        return 0;
    case HTML_PRE_PLAIN:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
            do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                         h_env->limit);
        }
        obuf->flag |= (RB_PRE | RB_IGNORE_P);
        return 1;
    case HTML_N_PRE_PLAIN:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
            do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                         h_env->limit);
            obuf->flag |= RB_IGNORE_P;
        }
        obuf->flag &= ~RB_PRE;
        return 1;
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
            do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                         h_env->limit);
        }
        obuf->flag |= (RB_PLAIN | RB_IGNORE_P);
        switch (cmd)
        {
        case HTML_LISTING:
            obuf->end_tag = HTML_N_LISTING;
            break;
        case HTML_XMP:
            obuf->end_tag = HTML_N_XMP;
            break;
        case HTML_PLAINTEXT:
            obuf->end_tag = MAX_HTMLTAG;
            break;
        }
        return 1;
    case HTML_N_LISTING:
    case HTML_N_XMP:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
        {
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
            do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                         h_env->limit);
            obuf->flag |= RB_IGNORE_P;
        }
        obuf->flag &= ~RB_PLAIN;
        obuf->end_tag = 0;
        return 1;
    case HTML_SCRIPT:
        obuf->flag |= RB_SCRIPT;
        obuf->end_tag = HTML_N_SCRIPT;
        return 1;
    case HTML_STYLE:
        obuf->flag |= RB_STYLE;
        obuf->end_tag = HTML_N_STYLE;
        return 1;
    case HTML_N_SCRIPT:
        obuf->flag &= ~RB_SCRIPT;
        obuf->end_tag = 0;
        return 1;
    case HTML_N_STYLE:
        obuf->flag &= ~RB_STYLE;
        obuf->end_tag = 0;
        return 1;
    case HTML_A:
        if (obuf->anchor.url.size())
            close_anchor(h_env, obuf, seq);

        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
            obuf->anchor.url = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_TARGET, &p))
            obuf->anchor.target = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_REFERER, &p))
            obuf->anchor.referer = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
            obuf->anchor.title = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_ACCESSKEY, &p))
            obuf->anchor.accesskey = (unsigned char)*p;

        hseq = 0;
        if (tag->TryGetAttributeValue(ATTR_HSEQ, &hseq))
            obuf->anchor.hseq = hseq;

        if (hseq == 0 && obuf->anchor.url.size())
        {
            obuf->anchor.hseq = seq->Get();
            tmp = process_anchor(tag, h_env->tagbuf->ptr, seq);
            push_tag(obuf, tmp->ptr, HTML_A);
            if (displayLinkNumber)
                HTMLlineproc1(seq->GetLinkNumberStr(-1)->ptr, h_env, seq);
            return 1;
        }
        return 0;
    case HTML_N_A:
        close_anchor(h_env, obuf, seq);
        return 1;
    case HTML_IMG:
        tmp = process_img(tag, h_env->limit, seq);
        HTMLlineproc1(tmp->ptr, h_env, seq);
        return 1;
    case HTML_IMG_ALT:
        if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            obuf->img_alt = Strnew(p);
#ifdef USE_IMAGE
        i = 0;
        if (tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &i))
        {
            if (i > obuf->top_margin)
                obuf->top_margin = i;
        }
        i = 0;
        if (tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &i))
        {
            if (i > obuf->bottom_margin)
                obuf->bottom_margin = i;
        }
#endif
        return 0;
    case HTML_N_IMG_ALT:
        if (obuf->img_alt)
        {
            if (!close_effect0(obuf, HTML_IMG_ALT))
                push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
            obuf->img_alt = NULL;
        }
        return 1;
    case HTML_INPUT_ALT:
        i = 0;
        if (tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &i))
        {
            if (i > obuf->top_margin)
                obuf->top_margin = i;
        }
        i = 0;
        if (tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &i))
        {
            if (i > obuf->bottom_margin)
                obuf->bottom_margin = i;
        }
        return 0;
    case HTML_TABLE:
        close_anchor(h_env, obuf, seq);
        obuf->table_level++;
        if (obuf->table_level >= MAX_TABLE)
            break;
        w = BORDER_NONE;
        /* x: cellspacing, y: cellpadding */
        x = 2;
        y = 1;
        z = 0;
        width = 0;
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
        if (tag->TryGetAttributeValue(ATTR_WIDTH, &i))
        {
            if (obuf->table_level == 0)
                width = REAL_WIDTH(i, h_env->limit - h_env->currentEnv().indent);
            else
                width = RELATIVE_WIDTH(i);
        }
        if (tag->HasAttribute(ATTR_HBORDER))
            w = BORDER_NOWIN;
        tag->TryGetAttributeValue(ATTR_CELLSPACING, &x);
        tag->TryGetAttributeValue(ATTR_CELLPADDING, &y);
        tag->TryGetAttributeValue(ATTR_VSPACE, &z);
#ifdef ID_EXT
        tag->TryGetAttributeValue(ATTR_ID, &id);
#endif /* ID_EXT */
        tables[obuf->table_level] = begin_table(w, x, y, z, seq);
#ifdef ID_EXT
        if (id != NULL)
            tables[obuf->table_level]->id = Strnew(id);
#endif /* ID_EXT */
        table_mode[obuf->table_level].pre_mode = 0;
        table_mode[obuf->table_level].indent_level = 0;
        table_mode[obuf->table_level].nobr_level = 0;
        table_mode[obuf->table_level].caption = 0;
        table_mode[obuf->table_level].end_tag = 0; /* HTML_UNKNOWN */
#ifndef TABLE_EXPAND
        tables[obuf->table_level]->total_width = width;
#else
        tables[obuf->table_level]->real_width = width;
        tables[obuf->table_level]->total_width = 0;
#endif
        return 1;
    case HTML_N_TABLE:
        /* should be processed in HTMLlineproc() */
        return 1;
    case HTML_CENTER:
        CLOSE_A;
        if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        RB_SAVE_FLAG(obuf);
        RB_SET_ALIGN(obuf, RB_CENTER);
        return 1;
    case HTML_N_CENTER:
        CLOSE_A;
        if (!(obuf->flag & RB_PREMODE))
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_DIV:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_DIV:
        CLOSE_A;
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_DIV_INT:
        CLOSE_P;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_DIV_INT:
        CLOSE_P;
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_FORM:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        tmp = process_form(tag);
        if (tmp)
            HTMLlineproc1(tmp->ptr, h_env, seq);
        return 1;
    case HTML_N_FORM:
        CLOSE_A;
        flushline(h_env, obuf, h_env->currentEnv().indent, 0, h_env->limit);
        obuf->flag |= RB_IGNORE_P;
        process_n_form();
        return 1;
    case HTML_INPUT:
        close_anchor(h_env, obuf, seq);
        tmp = process_input(tag, seq);
        if (tmp)
            HTMLlineproc1(tmp->ptr, h_env, seq);
        return 1;
    case HTML_SELECT:
        close_anchor(h_env, obuf, seq);
        tmp = process_select(tag, seq);
        if (tmp)
            HTMLlineproc1(tmp->ptr, h_env, seq);
        obuf->flag |= RB_INSELECT;
        obuf->end_tag = HTML_N_SELECT;
        return 1;
    case HTML_N_SELECT:
        obuf->flag &= ~RB_INSELECT;
        obuf->end_tag = 0;
        tmp = process_n_select(seq);
        if (tmp)
            HTMLlineproc1(tmp->ptr, h_env, seq);
        return 1;
    case HTML_OPTION:
        /* nothing */
        return 1;
    case HTML_TEXTAREA:
        close_anchor(h_env, obuf, seq);
        tmp = process_textarea(tag, h_env->limit);
        if (tmp)
            HTMLlineproc1(tmp->ptr, h_env, seq);
        obuf->flag |= RB_INTXTA;
        obuf->end_tag = HTML_N_TEXTAREA;
        return 1;
    case HTML_N_TEXTAREA:
        obuf->flag &= ~RB_INTXTA;
        obuf->end_tag = 0;
        tmp = process_n_textarea(seq);
        if (tmp)
            HTMLlineproc1(tmp->ptr, h_env, seq);
        return 1;
    case HTML_ISINDEX:
        p = "";
        q = "!CURRENT_URL!";
        tag->TryGetAttributeValue(ATTR_PROMPT, &p);
        tag->TryGetAttributeValue(ATTR_ACTION, &q);
        tmp = Strnew_m_charp("<form method=get action=\"",
                             html_quote(q),
                             "\">",
                             html_quote(p),
                             "<input type=text name=\"\" accept></form>",
                             NULL);
        HTMLlineproc1(tmp->ptr, h_env, seq);
        return 1;
    case HTML_META:
        p = q = NULL;
        tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &p);
        tag->TryGetAttributeValue(ATTR_CONTENT, &q);
#ifdef USE_M17N
        if (p && q && !strcasecmp(p, "Content-Type") &&
            (q = strcasestr(q, "charset")) != NULL)
        {
            q += 7;
            SKIP_BLANKS(&q);
            if (*q == '=')
            {
                q++;
                SKIP_BLANKS(&q);
                SetMetaCharset(wc_guess_charset(q, WC_CES_NONE));
            }
        }
        else
#endif
            if (p && q && !strcasecmp(p, "refresh"))
        {
            int refresh_interval;
            tmp = NULL;
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
                HTMLlineproc1(tmp->ptr, h_env, seq);
                do_blankline(h_env, obuf, h_env->currentEnv().indent, 0,
                             h_env->limit);
                if (!is_redisplay &&
                    !((obuf->flag & RB_NOFRAMES) && w3mApp::Instance().RenderFrame))
                {
                    tag->need_reconstruct = TRUE;
                    return 0;
                }
            }
        }
        return 1;
    case HTML_BASE:
        p = NULL;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
        {
            GetCurBaseUrl()->Parse(p, NULL);
        }
    case HTML_MAP:
    case HTML_N_MAP:
    case HTML_AREA:
        return 0;
    case HTML_DEL:
        switch (displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag |= RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc1("<U>[DEL:</U>", h_env, seq);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            obuf->in_strike++;
            if (obuf->in_strike == 1)
            {
                push_tag(obuf, "<s>", HTML_S);
            }
            break;
        }
        return 1;
    case HTML_N_DEL:
        switch (displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag &= ~RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc1("<U>:DEL]</U>", h_env, seq);
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike == 0)
                return 1;
            if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
                obuf->in_strike = 0;
            if (obuf->in_strike > 0)
            {
                obuf->in_strike--;
                if (obuf->in_strike == 0)
                {
                    push_tag(obuf, "</s>", HTML_N_S);
                }
            }
            break;
        }
        return 1;
    case HTML_S:
        switch (displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag |= RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc1("<U>[S:</U>", h_env, seq);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            obuf->in_strike++;
            if (obuf->in_strike == 1)
            {
                push_tag(obuf, "<s>", HTML_S);
            }
            break;
        }
        return 1;
    case HTML_N_S:
        switch (displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag &= ~RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc1("<U>:S]</U>", h_env, seq);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike == 0)
                return 1;
            if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
                obuf->in_strike = 0;
            if (obuf->in_strike > 0)
            {
                obuf->in_strike--;
                if (obuf->in_strike == 0)
                {
                    push_tag(obuf, "</s>", HTML_N_S);
                }
            }
        }
        return 1;
    case HTML_INS:
        switch (displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc1("<U>[INS:</U>", h_env, seq);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            obuf->in_ins++;
            if (obuf->in_ins == 1)
            {
                push_tag(obuf, "<ins>", HTML_INS);
            }
            break;
        }
        return 1;
    case HTML_N_INS:
        switch (displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc1("<U>:INS]</U>", h_env, seq);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_ins == 0)
                return 1;
            if (obuf->in_ins == 1 && close_effect0(obuf, HTML_INS))
                obuf->in_ins = 0;
            if (obuf->in_ins > 0)
            {
                obuf->in_ins--;
                if (obuf->in_ins == 0)
                {
                    push_tag(obuf, "</ins>", HTML_N_INS);
                }
            }
            break;
        }
        return 1;
    case HTML_SUP:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc1("^", h_env, seq);
        return 1;
    case HTML_N_SUP:
        return 1;
    case HTML_SUB:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc1("[", h_env, seq);
        return 1;
    case HTML_N_SUB:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc1("]", h_env, seq);
        return 1;
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
        return 1;
    case HTML_BGSOUND:
        if (view_unseenobject)
        {
            if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
                HTMLlineproc1(s->ptr, h_env, seq);
            }
        }
        return 1;
    case HTML_EMBED:
        if (view_unseenobject)
        {
            if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
                HTMLlineproc1(s->ptr, h_env, seq);
            }
        }
        return 1;
    case HTML_APPLET:
        if (view_unseenobject)
        {
            if (tag->TryGetAttributeValue(ATTR_ARCHIVE, &p))
            {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
                HTMLlineproc1(s->ptr, h_env, seq);
            }
        }
        return 1;
    case HTML_BODY:
        if (view_unseenobject)
        {
            if (tag->TryGetAttributeValue(ATTR_BACKGROUND, &p))
            {
                Str s;
                q = html_quote(p);
                s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
                HTMLlineproc1(s->ptr, h_env, seq);
            }
        }
    case HTML_N_HEAD:
        if (obuf->flag & RB_TITLE)
            HTMLlineproc1("</title>", h_env, seq);
    case HTML_HEAD:
    case HTML_N_BODY:
        return 1;
    default:
        /* obuf->prevchar = '\0'; */
        return 0;
    }
    /* not reached */
    return 0;
}

static int
need_flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
               Lineprop mode)
{
    if (obuf->flag & RB_PRE_INT)
    {
        if (obuf->pos > h_env->limit)
            return 1;
        else
            return 0;
    }

    auto ch = obuf->line->Back();
    /* if (ch == ' ' && obuf->tag_sp > 0) */
    if (ch == ' ')
        return 0;

    if (obuf->pos > h_env->limit)
        return 1;

    return 0;
}

static int
table_width(struct html_feed_environ *h_env, int table_level)
{
    int width;
    if (table_level < 0)
        return 0;
    width = tables[table_level]->total_width;
    if (table_level > 0 || width > 0)
        return width;
    return h_env->limit - h_env->currentEnv().indent;
}

// HTML processing first pass
//
// * from loadHtmlStream
//
void HTMLlineproc0(const char *line, struct html_feed_environ *h_env, bool internal, HSequence *seq)
{
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
        fprintf(f, "HTMLlineproc1(\"%s\",%d,%lx)\n", line, h_env->limit,
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
        char *str;
        int is_tag = FALSE;
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
                    is_tag = TRUE;
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
                feed_title(str);
                continue;
            }
            /* select */
            if (pre_mode & RB_INSELECT)
            {
                if (obuf->table_level >= 0)
                    goto proc_normal;
                feed_select(str, seq);
                continue;
            }
            if (is_tag)
            {
                char *p;
                if (strncmp(str, "<!--", 4) && (p = strchr(str + 1, '<')))
                {
                    str = Strnew_charp_n(str, p - str)->ptr;
                    line = Strnew_m_charp(p, line, NULL)->ptr;
                }
                is_tag = FALSE;
            }
            if (obuf->table_level >= 0)
                goto proc_normal;
            /* textarea */
            if (pre_mode & RB_INTXTA)
            {
                feed_textarea(str);
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
            switch (feed_table(tbl, str, tbl_mode, tbl_width, internal, seq))
            {
            case 0:
                /* </table> tag */
                obuf->table_level--;
                if (obuf->table_level >= MAX_TABLE - 1)
                    continue;
                end_table(tbl, seq);
                if (obuf->table_level >= 0)
                {
                    struct table *tbl0 = tables[obuf->table_level];
                    str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
                    pushTable(tbl0, tbl);
                    tbl = tbl0;
                    tbl_mode = &table_mode[obuf->table_level];
                    tbl_width = table_width(h_env, obuf->table_level);
                    feed_table(tbl, str, tbl_mode, tbl_width, TRUE, seq);
                    continue;
                    /* continue to the next */
                }
                if (obuf->flag & RB_DEL)
                    continue;
                /* all tables have been read */
                if (tbl->vspace > 0 && !(obuf->flag & RB_IGNORE_P))
                {
                    int indent = h_env->currentEnv().indent;
                    flushline(h_env, obuf, indent, 0, h_env->limit);
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                }
                save_fonteffect(h_env, obuf);
                renderTable(tbl, tbl_width, h_env, seq);
                restore_fonteffect(h_env, obuf);
                obuf->flag &= ~RB_IGNORE_P;
                if (tbl->vspace > 0)
                {
                    int indent = h_env->currentEnv().indent;
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                    obuf->flag |= RB_IGNORE_P;
                }
                set_space_to_prevchar(obuf->prevchar);
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
            if (HTMLtagproc1(tag, h_env, seq) == 0)
            {
                /* preserve the tag for second-stage processing */
                if (tag->need_reconstruct)
                    h_env->tagbuf = tag->ToStr();
                push_tag(obuf, h_env->tagbuf->ptr, cmd);
            }
#ifdef ID_EXT
            else
            {
                process_idattr(obuf, cmd, tag);
            }
#endif /* ID_EXT */
            obuf->bp.initialize();
            clear_ignore_p_flag(cmd, obuf);
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
                    char *p = str;
                    int ech = ucs4_from_entity(&p);
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
                        PUSH(' ');
                    else
                        flushline(h_env, obuf, h_env->currentEnv().indent,
                                  1, h_env->limit);
                }
                else if (ch == '\t')
                {
                    do
                    {
                        PUSH(' ');
                    } while ((h_env->currentEnv().indent + obuf->pos) % w3mApp::Instance().Tabstop != 0);
                    str++;
                }
                else if (obuf->flag & RB_PLAIN)
                {
                    char *p = html_quote_char(*str);
                    if (p)
                    {
                        push_charp(obuf, 1, p, PC_ASCII);
                        str++;
                    }
                    else
                    {
                        proc_mchar(obuf, 1, delta, &str, mode);
                    }
                }
                else
                {
                    if (*str == '&')
                        proc_escape(obuf, &str);
                    else
                        proc_mchar(obuf, 1, delta, &str, mode);
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
                        PUSH(' ');
                    }
                    str++;
                }
                else
                {
#ifdef USE_M17N
                    if (mode == PC_KANJI1)
                        is_hangul = wtf_is_hangul((uint8_t *)str);
                    else
                        is_hangul = 0;
                    if (!SimplePreserveSpace && mode == PC_KANJI1 &&
                        !is_hangul && !prev_is_hangul &&
                        obuf->pos > h_env->currentEnv().indent &&
                        obuf->line->Back() == ' ')
                    {
                        while (obuf->line->Size() >= 2 &&
                               !strncmp(obuf->line->ptr + obuf->line->Size() -
                                            2,
                                        "  ", 2) &&
                               obuf->pos >= h_env->currentEnv().indent)
                        {
                            obuf->line->Pop(1);
                            obuf->pos--;
                        }
                        if (obuf->line->Size() >= 3 &&
                            obuf->prev_ctype == PC_KANJI1 &&
                            obuf->line->Back() == ' ' &&
                            obuf->pos >= h_env->currentEnv().indent)
                        {
                            obuf->line->Pop(1);
                            obuf->pos--;
                        }
                    }
                    prev_is_hangul = is_hangul;
#endif
                    if (*str == '&')
                        proc_escape(obuf, &str);
                    else
                        proc_mchar(obuf, obuf->flag & RB_SPECIAL, delta, &str,
                                   mode);
                }
            }
            if (need_flushline(h_env, obuf, mode))
            {
                char *bp = obuf->line->ptr + obuf->bp.len();
                char *tp = bp - obuf->bp.tlen();
                int i = 0;

                if (tp > obuf->line->ptr && tp[-1] == ' ')
                    i = 1;

                indent = h_env->currentEnv().indent;
                if (obuf->bp.pos() - i > indent)
                {
                    Str line;
                    append_tags(obuf);
                    line = Strnew(bp);
                    obuf->line->Pop(obuf->line->Size() - obuf->bp.len());
#ifdef FORMAT_NICE
                    if (obuf->pos - i > h_env->limit)
                        obuf->flag |= RB_FILL;
#endif /* FORMAT_NICE */
                    obuf->bp.back_to(obuf);
                    flushline(h_env, obuf, indent, 0, h_env->limit);
#ifdef FORMAT_NICE
                    obuf->flag &= ~RB_FILL;
#endif /* FORMAT_NICE */
                    HTMLlineproc1(line->ptr, h_env, seq);
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
        indent = h_env->currentEnv().indent;
        if (obuf->pos - i > h_env->limit)
        {
#ifdef FORMAT_NICE
            obuf->flag |= RB_FILL;
#endif /* FORMAT_NICE */
            flushline(h_env, obuf, indent, 0, h_env->limit);
#ifdef FORMAT_NICE
            obuf->flag &= ~RB_FILL;
#endif /* FORMAT_NICE */
        }
    }
}

void init_henv(struct html_feed_environ *h_env, struct readbuffer *obuf,
               struct environment *envs, int nenv, TextLineList *buf,
               int limit, int indent)
{
    envs[0].indent = indent;

    obuf->line = Strnew();
    obuf->cprop = P_UNKNOWN;
    obuf->pos = 0;
    obuf->prevchar = Strnew_size(8);
    set_space_to_prevchar(obuf->prevchar);
    obuf->flag = RB_IGNORE_P;
    obuf->flag_sp = 0;
    obuf->status = R_ST_NORMAL;
    obuf->table_level = -1;
    obuf->nobr_level = 0;
    obuf->anchor = {};
    obuf->img_alt = 0;
    obuf->in_bold = 0;
    obuf->in_italic = 0;
    obuf->in_under = 0;
    obuf->in_strike = 0;
    obuf->in_ins = 0;
    obuf->prev_ctype = PC_ASCII;
    obuf->tag_sp = 0;
    obuf->fontstat_sp = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    obuf->bp.initialize();
    obuf->bp.set(obuf, 0);

    h_env->Initialize(buf, obuf, limit, envs, nenv);
}

void completeHTMLstream(struct html_feed_environ *h_env, struct readbuffer *obuf, HSequence *seq)
{
    close_anchor(h_env, obuf, seq);
    if (obuf->img_alt)
    {
        push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
        obuf->img_alt = NULL;
    }
    if (obuf->in_bold)
    {
        push_tag(obuf, "</b>", HTML_N_B);
        obuf->in_bold = 0;
    }
    if (obuf->in_italic)
    {
        push_tag(obuf, "</i>", HTML_N_I);
        obuf->in_italic = 0;
    }
    if (obuf->in_under)
    {
        push_tag(obuf, "</u>", HTML_N_U);
        obuf->in_under = 0;
    }
    if (obuf->in_strike)
    {
        push_tag(obuf, "</s>", HTML_N_S);
        obuf->in_strike = 0;
    }
    if (obuf->in_ins)
    {
        push_tag(obuf, "</ins>", HTML_N_INS);
        obuf->in_ins = 0;
    }
    if (obuf->flag & RB_INTXTA)
        HTMLlineproc1("</textarea>", h_env, seq);
    /* for unbalanced select tag */
    if (obuf->flag & RB_INSELECT)
        HTMLlineproc1("</select>", h_env, seq);
    if (obuf->flag & RB_TITLE)
        HTMLlineproc1("</title>", h_env, seq);

    /* for unbalanced table tag */
    if (obuf->table_level >= MAX_TABLE)
        obuf->table_level = MAX_TABLE - 1;

    while (obuf->table_level >= 0)
    {
        table_mode[obuf->table_level].pre_mode &= ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
        HTMLlineproc1("</table>", h_env, seq);
    }
}
