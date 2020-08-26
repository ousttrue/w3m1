#include "readbuffer.h"
#include "gc_helper.h"
#include "indep.h"
#include "file.h"
// #include "tagstack.h"
#include "myctype.h"
#include "entity.h"
#include <list>

int sloppy_parse_line(char **str)
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
        fontstat = obuf->fontstat;
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
    obuf->fontstat = fontstat;
    obuf->prev_ctype = prev_ctype;
    if (obuf->flag & RB_NOBR)
        obuf->nobr_level = nobr_level;
}

struct link_stack
{
    HtmlTags cmd;
    short offset;
    short pos;
};

static struct std::list<link_stack> link_stack;

static void push_link(HtmlTags cmd, int offset, int pos)
{
    link_stack.push_front({});
    auto p = &link_stack.front();
    p->cmd = cmd;
    p->offset = offset;
    p->pos = pos;
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

///
///
///
readbuffer::readbuffer()
{
    this->line = Strnew();
    this->cprop = P_UNKNOWN;
    this->pos = 0;
    this->prevchar = Strnew_size(8);
    this->set_space_to_prevchar();
    this->flag = RB_IGNORE_P;
    this->flag_sp = 0;
    this->status = R_ST_NORMAL;
    this->table_level = -1;
    this->nobr_level = 0;
    this->anchor = {};
    this->img_alt = 0;
    this->fontstat = {};
    this->prev_ctype = PC_ASCII;
    this->tag_sp = 0;
    this->fontstat_sp = 0;
    this->top_margin = 0;
    this->bottom_margin = 0;
    this->bp.initialize();
    this->bp.set(this, 0);
}

void readbuffer::reset()
{
    this->line = Strnew_size(256);
    this->pos = 0;
    this->top_margin = 0;
    this->bottom_margin = 0;
    set_space_to_prevchar();
    this->bp.initialize();
    this->flag &= ~RB_NFLUSHED;
    this->bp.set(this, 0);
    this->prev_ctype = PC_ASCII;
    link_stack.clear();
}

void readbuffer::passthrough(char *str, int back)
{
    Str tok = Strnew();
    char *str_bak;

    if (back)
    {
        Str str_save = Strnew(str);
        this->line->Pop(this->line->ptr + this->line->Size() - str);
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
                for (auto p = link_stack.begin(); p != link_stack.end(); ++p)
                {
                    if (p->cmd == cmd)
                    {
                        link_stack.erase(link_stack.begin(), p);
                        break;
                    }
                }
                back = 0;
            }
            else
            {
                tok->Push(str_bak, str - str_bak);
                push_tag(tok->ptr, cmd);
                tok->Clear();
            }
        }
        else
        {
            push_nchars(0, str_bak, str - str_bak, this->prev_ctype);
        }
    }
}

int readbuffer::close_effect0(int cmd)
{
    int i;
    char *p;

    for (i = this->tag_sp - 1; i >= 0; i--)
    {
        if (this->tag_stack[i]->cmd == cmd)
            break;
    }
    if (i >= 0)
    {
        this->tag_sp--;
        bcopy(&this->tag_stack[i + 1], &this->tag_stack[i],
              (this->tag_sp - i) * sizeof(struct cmdtable *));
        return 1;
    }
    else if ((p = has_hidden_link(cmd)) != NULL)
    {
        passthrough(p, 1);
        return 1;
    }
    return 0;
}

char *readbuffer::has_hidden_link(int cmd)
{
    Str line = this->line;
    if (line->Back() != '>')
        return NULL;

    auto p = std::find_if(link_stack.begin(), link_stack.end(), [cmd](auto &p) -> bool {
        return p.cmd == cmd;
    });
    if (p == link_stack.end())
        return NULL;

    if (this->pos == p->pos)
        return line->ptr + p->offset;

    return NULL;
}

void readbuffer::process_idattr(int cmd, struct parsed_tag *tag)
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
    push_tag(idtag->ptr, HTML_NOP);
}

void readbuffer::proc_escape(const char **str_return)
{
    const char *str = *str_return, *estr;
    auto [pos, ech] = ucs4_from_entity(*str_return);
    *str_return = pos.data();
    int width, n_add = *str_return - str;
    Lineprop mode = PC_ASCII;

    if (ech < 0)
    {
        *str_return = str;
        proc_mchar(this->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
        return;
    }
    mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

    estr = (char *)from_unicode(ech, w3mApp::Instance().InnerCharset);
    check_breakpoint(this->flag & RB_SPECIAL, estr);
    width = get_strwidth(estr);
    if (width == 1 && ech == (unsigned char)*estr &&
        ech != '&' && ech != '<' && ech != '>')
    {
        if (IS_CNTRL(ech))
            mode = PC_CTRL;
        push_charp(width, estr, mode);
    }
    else
        push_nchars(width, str, n_add, mode);
    this->prevchar->CopyFrom(estr, strlen(estr));
    this->prev_ctype = mode;
}

void readbuffer::push_nchars(int width, const char *str, int len, Lineprop mode)
{
    append_tags();
    this->line->Push(str, len);
    this->pos += width;
    if (width > 0)
    {
        this->prevchar->CopyFrom(str, len);
        this->prev_ctype = mode;
    }
    this->flag |= RB_NFLUSHED;
}

void readbuffer::push_charp(int width, const char *str, Lineprop mode)
{
    push_nchars(width, str, strlen(str), mode);
}

void readbuffer::push_str(int width, Str str, Lineprop mode)
{
    push_nchars(width, str->ptr, str->Size(), mode);
}

void readbuffer::check_breakpoint(int pre_mode, const char *ch)
{
    int tlen, len = this->line->Size();

    append_tags();
    if (pre_mode)
        return;
    tlen = this->line->Size() - len;
    if (tlen > 0 || is_boundary((unsigned char *)this->prevchar->ptr,
                                (unsigned char *)ch))
        this->bp.set(this, tlen);
}

void readbuffer::push_char(int pre_mode, char ch)
{
    check_breakpoint(pre_mode, &ch);
    this->line->Push(ch);
    this->pos++;
    this->prevchar->CopyFrom(&ch, 1);
    if (ch != ' ')
        this->prev_ctype = PC_ASCII;
    this->flag |= RB_NFLUSHED;
}

void readbuffer::PUSH(int c)
{
    push_char(this->flag & RB_SPECIAL, c);
}

void readbuffer::set_space_to_prevchar()
{
    prevchar->CopyFrom(" ", 1);
}

void readbuffer::push_spaces(int pre_mode, int width)
{
    if (width <= 0)
        return;
    check_breakpoint(pre_mode, " ");
    for (int i = 0; i < width; i++)
        this->line->Push(' ');
    this->pos += width;
    set_space_to_prevchar();
    this->flag |= RB_NFLUSHED;
}

void readbuffer::fillline(int indent)
{
    push_spaces(1, indent - this->pos);
    this->flag &= ~RB_NFLUSHED;
}

void readbuffer::proc_mchar(int pre_mode,
                            int width, const char **str, Lineprop mode)
{
    check_breakpoint(pre_mode, *str);
    this->pos += width;
    this->line->Push(*str, get_mclen(*str));
    if (width > 0)
    {
        this->prevchar->CopyFrom(*str, 1);
        if (**str != ' ')
            this->prev_ctype = mode;
    }
    (*str) += get_mclen(*str);
    this->flag |= RB_NFLUSHED;
}

void readbuffer::append_tags()
{
    int i;
    int len = this->line->Size();
    int set_bp = 0;

    for (i = 0; i < this->tag_sp; i++)
    {
        switch (this->tag_stack[i]->cmd)
        {
        case HTML_A:
        case HTML_IMG_ALT:
        case HTML_B:
        case HTML_U:
        case HTML_I:
        case HTML_S:
            push_link(this->tag_stack[i]->cmd, this->line->Size(), this->pos);
            break;
        }
        this->line->Push(this->tag_stack[i]->cmdname);
        switch (this->tag_stack[i]->cmd)
        {
        case HTML_NOBR:
            if (this->nobr_level > 1)
                break;
        case HTML_WBR:
            set_bp = 1;
            break;
        }
    }
    this->tag_sp = 0;
    if (set_bp)
        this->bp.set(this, this->line->Size() - len);
}

void readbuffer::push_tag(const char *cmdname, HtmlTags cmd)
{
    this->tag_stack[this->tag_sp] = New(struct cmdtable);
    this->tag_stack[this->tag_sp]->cmdname = allocStr(cmdname, -1);
    this->tag_stack[this->tag_sp]->cmd = cmd;
    this->tag_sp++;
    if (this->tag_sp >= TAG_STACK_SIZE || this->flag & (RB_SPECIAL & ~RB_NOBR))
        append_tags();
}

void readbuffer::save_fonteffect()
{
    if (this->fontstat_sp < FONT_STACK_SIZE)
        this->fontstat_stack[this->fontstat_sp] = this->fontstat;

    this->fontstat_sp++;
    if (this->fontstat.in_bold)
        push_tag("</b>", HTML_N_B);
    if (this->fontstat.in_italic)
        push_tag("</i>", HTML_N_I);
    if (this->fontstat.in_under)
        push_tag("</u>", HTML_N_U);
    if (this->fontstat.in_strike)
        push_tag("</s>", HTML_N_S);
    if (this->fontstat.in_ins)
        push_tag("</ins>", HTML_N_INS);
    this->fontstat = {};
}

void readbuffer::restore_fonteffect()
{
    if (this->fontstat_sp > 0)
        this->fontstat_sp--;
    if (this->fontstat_sp < FONT_STACK_SIZE)
    {
        this->fontstat = this->fontstat_stack[this->fontstat_sp];
    }
    if (this->fontstat.in_bold)
        push_tag("<b>", HTML_B);
    if (this->fontstat.in_italic)
        push_tag("<i>", HTML_I);
    if (this->fontstat.in_under)
        push_tag("<u>", HTML_U);
    if (this->fontstat.in_strike)
        push_tag("<s>", HTML_S);
    if (this->fontstat.in_ins)
        push_tag("<ins>", HTML_INS);
}

void readbuffer::clear_ignore_p_flag(int cmd)
{
    static int clear_flag_cmd[] = {
        HTML_HR, HTML_UNKNOWN};
    int i;

    for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++)
    {
        if (cmd == clear_flag_cmd[i])
        {
            this->flag &= ~RB_IGNORE_P;
            return;
        }
    }
}

void readbuffer::set_alignment(struct parsed_tag *tag)
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

    auto obuf = this;
    RB_SAVE_FLAG(obuf);
    if (flag != -1)
    {
        RB_SET_ALIGN(obuf, flag);
    }
}
