#include <list>
#include "tagstack.h"
#include "indep.h"
#include "gc_helper.h"
#include "html.h"
#include "file.h"
#include "html/html.h"
#include "table.h"
#include "myctype.h"
#include "file.h"
#include "entity.h"
#include "symbol.h"
#include "ctrlcode.h"
#include "textlist.h"
#include "html/html_processor.h"
#include "html/html_context.h"
#include "html/tokenizer.h"
#include "frontend/buffer.h"
#include "frontend/line.h"
#include "charset.h"

static struct table *tables[MAX_TABLE];
static struct table_mode table_mode[MAX_TABLE];

void CLOSE_P(readbuffer *obuf, html_feed_environ *h_env)
{
    if (obuf->flag & RB_P)
    {
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        obuf->flag &= ~RB_P;
    }
}

void CLOSE_DT(readbuffer *obuf, html_feed_environ *h_env, HtmlContext *seq)
{
    if (obuf->flag & RB_IN_DT)
    {
        obuf->flag &= ~RB_IN_DT;
        HTMLlineproc0("</b>", h_env, true, seq);
    }
}

void html_feed_environ::Initialize(TextLineList *buf, readbuffer *obuf, int limit, int indent)
{
    this->envs.push_back({
        indent : indent
    });
    this->buf = buf;
    this->obuf = obuf;
    this->tagbuf = Strnew();
    this->limit = limit;
    this->maxlimit = 0;
    this->title = NULL;
    this->blank_lines = 0;
}

void html_feed_environ::PUSH_ENV(HtmlTags cmd)
{
    envs.push_back({
        env : cmd,
        count : 0,
        indent : envs.back().indent + w3mApp::Instance().IndentIncr
    });
}

void html_feed_environ::POP_ENV()
{
    envs.pop_back();
}

void html_feed_environ::push_render_image(Str str, int width, int limit)
{
    struct readbuffer *obuf = this->obuf;
    int indent = this->envs.back().indent;

    obuf->push_spaces(1, (limit - width) / 2);
    obuf->push_str(width, str, PC_ASCII);
    obuf->push_spaces(1, (limit - width + 1) / 2);
    if (width > 0)
        this->flushline(indent, 0, this->limit);
}


void html_feed_environ::flushline(int indent, int force, int width)
{
    // TextLineList *buf = h_env->buf;
    // FILE *f = h_env->f;
    Str line = obuf->line;
    Str pass = NULL;
    char *hidden_anchor = NULL, *hidden_img = NULL, *hidden_bold = NULL,
         *hidden_under = NULL, *hidden_italic = NULL, *hidden_strike = NULL,
         *hidden_ins = NULL, *hidden = NULL;

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && line->Back() == ' ')
    {
        line->Pop(1);
        obuf->pos--;
    }

    obuf->append_tags();

    if (obuf->anchor.url.size())
        hidden = hidden_anchor = obuf->has_hidden_link(HTML_A);
    if (obuf->img_alt)
    {
        if ((hidden_img = obuf->has_hidden_link(HTML_IMG_ALT)) != NULL)
        {
            if (!hidden || hidden_img < hidden)
                hidden = hidden_img;
        }
    }
    if (obuf->fontstat.in_bold)
    {
        if ((hidden_bold = obuf->has_hidden_link(HTML_B)) != NULL)
        {
            if (!hidden || hidden_bold < hidden)
                hidden = hidden_bold;
        }
    }
    if (obuf->fontstat.in_italic)
    {
        if ((hidden_italic = obuf->has_hidden_link(HTML_I)) != NULL)
        {
            if (!hidden || hidden_italic < hidden)
                hidden = hidden_italic;
        }
    }
    if (obuf->fontstat.in_under)
    {
        if ((hidden_under = obuf->has_hidden_link(HTML_U)) != NULL)
        {
            if (!hidden || hidden_under < hidden)
                hidden = hidden_under;
        }
    }
    if (obuf->fontstat.in_strike)
    {
        if ((hidden_strike = obuf->has_hidden_link(HTML_S)) != NULL)
        {
            if (!hidden || hidden_strike < hidden)
                hidden = hidden_strike;
        }
    }
    if (obuf->fontstat.in_ins)
    {
        if ((hidden_ins = obuf->has_hidden_link(HTML_INS)) != NULL)
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
    if (obuf->fontstat.in_bold && !hidden_bold)
        line->Push("</b>");
    if (obuf->fontstat.in_italic && !hidden_italic)
        line->Push("</i>");
    if (obuf->fontstat.in_under && !hidden_under)
        line->Push("</u>");
    if (obuf->fontstat.in_strike && !hidden_strike)
        line->Push("</s>");
    if (obuf->fontstat.in_ins && !hidden_ins)
        line->Push("</ins>");

    if (obuf->top_margin > 0)
    {
        int i;
        struct html_feed_environ h;
        struct readbuffer o;

        init_henv(&h, &o, NULL, width, indent);
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
            // flushline(h_env, &o, indent, force, width);
            // TODO
            flushline(indent, force, width);
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
        if (lbuf->pos > this->maxlimit)
            this->maxlimit = lbuf->pos;
        if (buf)
            pushTextLine(buf, lbuf);
        if (obuf->flag & RB_SPECIAL || obuf->flag & RB_NFLUSHED)
            this->blank_lines = 0;
        else
            this->blank_lines++;
    }
    else
    {
        char *p = line->ptr, *q;
        Str tmp = Strnew(), tmp2 = Strnew();

        while (*p)
        {
            q = p;
            if (sloppy_parse_line(&p))
            {
                tmp->Push(q, p - q);
                if (force == 2 && buf)
                {
                    appendTextLine(buf, tmp, 0);
                }
                else
                    tmp2->Push(tmp);
                tmp->Clear();
            }
        }
        if (force == 2)
        {
            if (pass && buf)
            {
                appendTextLine(buf, tmp, 0);
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

        init_henv(&h, &o, NULL, width, indent);
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
        {
            // flushline(h_env, &o, indent, force, width);
            flushline(indent, force, width);
        }
    }
    if (obuf->top_margin < 0 || obuf->bottom_margin < 0)
        return;

    obuf->reset();

    obuf->fillline(indent);
    if (pass)
        obuf->passthrough(pass->ptr, 0);
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
        if (obuf->anchor.referer == HttpReferrerPolicy::NoReferer)
        {
            tmp->Push("\" REFERER=NOREFERER\"");
        }
        if (obuf->anchor.title.size())
        {
            tmp->Push("\" TITLE=\"");
            tmp->Push(html_quote(obuf->anchor.title));
        }
        if (obuf->anchor.accesskey)
        {
            const char *c = html_quote_char(obuf->anchor.accesskey);
            tmp->Push("\" ACCESSKEY=\"");
            if (c)
                tmp->Push(c);
            else
                tmp->Push(obuf->anchor.accesskey);
        }
        tmp->Push("\">");
        obuf->push_tag(tmp->ptr, HTML_A);
    }
    if (!hidden_img && obuf->img_alt)
    {
        Str tmp = Strnew("<IMG_ALT SRC=\"");
        tmp->Push(html_quote(obuf->img_alt->ptr));
        tmp->Push("\">");
        obuf->push_tag(tmp->ptr, HTML_IMG_ALT);
    }
    if (!hidden_bold && obuf->fontstat.in_bold)
        obuf->push_tag("<B>", HTML_B);
    if (!hidden_italic && obuf->fontstat.in_italic)
        obuf->push_tag("<I>", HTML_I);
    if (!hidden_under && obuf->fontstat.in_under)
        obuf->push_tag("<U>", HTML_U);
    if (!hidden_strike && obuf->fontstat.in_strike)
        obuf->push_tag("<S>", HTML_S);
    if (!hidden_ins && obuf->fontstat.in_ins)
        obuf->push_tag("<INS>", HTML_INS);
}

void do_blankline(struct html_feed_environ *h_env, struct readbuffer *obuf,
                  int indent, int indent_incr, int width)
{
    if (h_env->blank_lines == 0)
        h_env->flushline(indent, 1, width);
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
close_anchor(struct html_feed_environ *h_env, struct readbuffer *obuf, HtmlContext *seq)
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
                HTMLlineproc0(ANSP, h_env, true, seq);
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

void CLOSE_A(readbuffer *obuf, html_feed_environ *h_env, HtmlContext *seq)
{
    CLOSE_P(obuf, h_env);
    close_anchor(h_env, obuf, seq);
}

int REAL_WIDTH(int w, int limit)
{
    return (((w) >= 0) ? (int)((w) / ImageManager::Instance().pixel_per_char) : -(w) * (limit) / 100);
}

static Str process_hr(struct parsed_tag *tag, int width, int indent_width, HtmlContext *seq)
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

int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env, HtmlContext *seq)
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
        HTMLlineproc0("<i>", h_env, true, seq);
        return 1;
    }
    case HTML_N_EM:
    {
        HTMLlineproc0("</i>", h_env, true, seq);
        return 1;
    }
    case HTML_STRONG:
    {
        HTMLlineproc0("<b>", h_env, true, seq);
        return 1;
    }
    case HTML_N_STRONG:
    {
        HTMLlineproc0("</b>", h_env, true, seq);
        return 1;
    }
    case HTML_Q:
    {
        HTMLlineproc0("`", h_env, true, seq);
        return 1;
    }
    case HTML_N_Q:
    {
        HTMLlineproc0("'", h_env, true, seq);
        return 1;
    }
    case HTML_P:
    case HTML_N_P:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 1, h_env->limit);
            do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
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
            do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
                         h_env->limit);
        }
        HTMLlineproc0("<b>", h_env, true, seq);
        h_env->obuf->set_alignment(tag);
        return 1;
    }
    case HTML_N_H:
    {
        HTMLlineproc0("</b>", h_env, true, seq);
        if (!(h_env->obuf->flag & RB_PREMODE))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        }
        do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(h_env->obuf);
        close_anchor(h_env, h_env->obuf, seq);
        h_env->obuf->flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_UL:
    case HTML_OL:
    case HTML_BLQ:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            if (!(h_env->obuf->flag & RB_PREMODE) &&
                (h_env->envs.empty() || tag->tagid == HTML_BLQ))
                do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
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
        CLOSE_DT;
        CLOSE_A(h_env->obuf, h_env, seq);
        if (h_env->envs.size())
        {
            h_env->flushline(h_env->envs.back().indent, 0,
                             h_env->limit);
            h_env->POP_ENV();
            if (!(h_env->obuf->flag & RB_PREMODE) &&
                (h_env->envs.empty() || tag->tagid == HTML_N_DL || tag->tagid == HTML_N_BLQ))
            {
                do_blankline(h_env, h_env->obuf,
                             h_env->envs.back().indent,
                             w3mApp::Instance().IndentIncr, h_env->limit);
                h_env->obuf->flag |= RB_IGNORE_P;
            }
        }
        close_anchor(h_env, h_env->obuf, seq);
        return 1;
    }
    case HTML_DL:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            if (!(h_env->obuf->flag & RB_PREMODE))
                do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
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
        CLOSE_A(h_env->obuf, h_env, seq);
        CLOSE_DT;
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
                                UL_SYMBOL((h_env->envs.size() - 1) % MAX_UL_LEVEL),
                                seq->SymbolWidth(),
                                1);
                    break;
                }
                if (seq->SymbolWidth() == 1)
                    h_env->obuf->push_charp(1, NBSP, PC_ASCII);
                h_env->obuf->push_str(seq->SymbolWidth(), tmp, PC_ASCII);
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
        CLOSE_A(h_env->obuf, h_env, seq);
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
            HTMLlineproc0("<b>", h_env, true, seq);
            h_env->obuf->flag |= RB_IN_DT;
        }
        h_env->obuf->flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_DD:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        CLOSE_DT;
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
        close_anchor(h_env, h_env->obuf, seq);
        seq->TitleOpen(tag);
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
        auto tmp = seq->TitleClose(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true, seq);
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
        CLOSE_A(h_env->obuf, h_env, seq);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->flag |= (RB_NOFRAMES | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    }
    case HTML_N_NOFRAMES:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
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
            h_env->obuf->push_tag(Sprintf("<a hseq=\"%d\" href=\"%s\">", seq->Increment(), q)->ptr, HTML_A);
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
        close_anchor(h_env, h_env->obuf, seq);
        auto tmp = process_hr(tag, h_env->limit, h_env->envs.back().indent, seq);
        HTMLlineproc0(tmp->ptr, h_env, true, seq);
        h_env->obuf->set_space_to_prevchar();
        return 1;
    }
    case HTML_PRE:
    {
        auto x = tag->HasAttribute(ATTR_FOR_TABLE);
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            if (!x)
                do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
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
            do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
                         h_env->limit);
            h_env->obuf->flag |= RB_IGNORE_P;
            h_env->blank_lines++;
        }
        h_env->obuf->flag &= ~RB_PRE;
        close_anchor(h_env, h_env->obuf, seq);
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
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
                         h_env->limit);
        }
        h_env->obuf->flag |= (RB_PRE | RB_IGNORE_P);
        return 1;
    }
    case HTML_N_PRE_PLAIN:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
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
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
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
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
        {
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
            do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
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
            close_anchor(h_env, h_env->obuf, seq);
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
            h_env->obuf->anchor.hseq = seq->Get();
            auto tmp = seq->process_anchor(tag, h_env->tagbuf->ptr);
            h_env->obuf->push_tag(tmp->ptr, HTML_A);
            if (w3mApp::Instance().displayLinkNumber)
                HTMLlineproc0(seq->GetLinkNumberStr(-1)->ptr, h_env, true, seq);
            return 1;
        }
        return 0;
    }
    case HTML_N_A:
    {
        close_anchor(h_env, h_env->obuf, seq);
        return 1;
    }
    case HTML_IMG:
    {
        auto tmp = seq->process_img(tag, h_env->limit);
        HTMLlineproc0(tmp->ptr, h_env, true, seq);
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
        close_anchor(h_env, h_env->obuf, seq);
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
        tables[h_env->obuf->table_level] = begin_table(w, x, y, z, seq);
        if (id != NULL)
            tables[h_env->obuf->table_level]->id = Strnew(id);

        table_mode[h_env->obuf->table_level].pre_mode = 0;
        table_mode[h_env->obuf->table_level].indent_level = 0;
        table_mode[h_env->obuf->table_level].nobr_level = 0;
        table_mode[h_env->obuf->table_level].caption = 0;
        table_mode[h_env->obuf->table_level].end_tag = 0; /* HTML_UNKNOWN */
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
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        RB_SAVE_FLAG(h_env->obuf);
        RB_SET_ALIGN(h_env->obuf, RB_CENTER);
        return 1;
    }
    case HTML_N_CENTER:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_PREMODE))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(h_env->obuf);
        return 1;
    }
    case HTML_DIV:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->set_alignment(tag);
        return 1;
    }
    case HTML_N_DIV:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(h_env->obuf);
        return 1;
    }
    case HTML_DIV_INT:
    {
        CLOSE_P(h_env->obuf, h_env);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->set_alignment(tag);
        return 1;
    }
    case HTML_N_DIV_INT:
    {
        CLOSE_P(h_env->obuf, h_env);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        RB_RESTORE_FLAG(h_env->obuf);
        return 1;
    }
    case HTML_FORM:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        if (!(h_env->obuf->flag & RB_IGNORE_P))
            h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        auto tmp = seq->FormOpen(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true, seq);
        return 1;
    }
    case HTML_N_FORM:
    {
        CLOSE_A(h_env->obuf, h_env, seq);
        h_env->flushline(h_env->envs.back().indent, 0, h_env->limit);
        h_env->obuf->flag |= RB_IGNORE_P;
        seq->FormClose();
        return 1;
    }
    case HTML_INPUT:
    {
        close_anchor(h_env, h_env->obuf, seq);
        auto tmp = seq->process_input(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true, seq);
        return 1;
    }
    case HTML_SELECT:
    {
        close_anchor(h_env, h_env->obuf, seq);
        auto tmp = seq->process_select(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true, seq);
        h_env->obuf->flag |= RB_INSELECT;
        h_env->obuf->end_tag = HTML_N_SELECT;
        return 1;
    }
    case HTML_N_SELECT:
    {
        h_env->obuf->flag &= ~RB_INSELECT;
        h_env->obuf->end_tag = 0;
        auto tmp = seq->process_n_select();
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true, seq);
        return 1;
    }
    case HTML_OPTION:
        /* nothing */
        return 1;
    case HTML_TEXTAREA:
    {
        close_anchor(h_env, h_env->obuf, seq);
        auto tmp = seq->process_textarea(tag, h_env->limit);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true, seq);
        h_env->obuf->flag |= RB_INTXTA;
        h_env->obuf->end_tag = HTML_N_TEXTAREA;
        return 1;
    }
    case HTML_N_TEXTAREA:
    {
        h_env->obuf->flag &= ~RB_INTXTA;
        h_env->obuf->end_tag = 0;
        auto tmp = seq->process_n_textarea();
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true, seq);
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
        HTMLlineproc0(tmp->ptr, h_env, true, seq);
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
                seq->SetMetaCharset(wc_guess_charset(q, WC_CES_NONE));
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
                HTMLlineproc0(tmp->ptr, h_env, true, seq);
                do_blankline(h_env, h_env->obuf, h_env->envs.back().indent, 0,
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
            HTMLlineproc0("<U>[DEL:</U>", h_env, true, seq);
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
            HTMLlineproc0("<U>:DEL]</U>", h_env, true, seq);
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
            HTMLlineproc0("<U>[S:</U>", h_env, true, seq);
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
            HTMLlineproc0("<U>:S]</U>", h_env, true, seq);
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
            HTMLlineproc0("<U>[INS:</U>", h_env, true, seq);
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
            HTMLlineproc0("<U>:INS]</U>", h_env, true, seq);
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
            HTMLlineproc0("^", h_env, true, seq);
        return 1;
    }
    case HTML_N_SUP:
        return 1;
    case HTML_SUB:
    {
        if (!(h_env->obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc0("[", h_env, true, seq);
        return 1;
    }
    case HTML_N_SUB:
    {
        if (!(h_env->obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc0("]", h_env, true, seq);
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
                HTMLlineproc0(s->ptr, h_env, true, seq);
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
                HTMLlineproc0(s->ptr, h_env, true, seq);
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
                HTMLlineproc0(s->ptr, h_env, true, seq);
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
                HTMLlineproc0(s->ptr, h_env, true, seq);
            }
        }
        return 1;
    }
    case HTML_N_HEAD:
    {
        if (h_env->obuf->flag & RB_TITLE)
            HTMLlineproc0("</title>", h_env, true, seq);
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

// HTML processing first pass
//
// * from loadHtmlStream
//
void HTMLlineproc0(const char *line, struct html_feed_environ *h_env, bool internal, HtmlContext *seq)
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
                seq->TitleContent(str);
                continue;
            }
            /* select */
            if (pre_mode & RB_INSELECT)
            {
                if (obuf->table_level >= 0)
                    goto proc_normal;
                seq->feed_select(str);
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
                seq->feed_textarea(str);
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
                    feed_table(tbl, str, tbl_mode, tbl_width, true, seq);
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
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                }
                h_env->obuf->save_fonteffect();
                renderTable(tbl, tbl_width, h_env, seq);
                h_env->obuf->restore_fonteffect();
                obuf->flag &= ~RB_IGNORE_P;
                if (tbl->vspace > 0)
                {
                    int indent = h_env->envs.back().indent;
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
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
            if (HTMLtagproc1(tag, h_env, seq) == 0)
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

            obuf->bp.initialize();
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
#ifdef FORMAT_NICE
                    if (obuf->pos - i > h_env->limit)
                        obuf->flag |= RB_FILL;
#endif /* FORMAT_NICE */
                    obuf->bp.back_to(obuf);
                    h_env->flushline(indent, 0, h_env->limit);
#ifdef FORMAT_NICE
                    obuf->flag &= ~RB_FILL;
#endif /* FORMAT_NICE */
                    HTMLlineproc0(line->ptr, h_env, true, seq);
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
#ifdef FORMAT_NICE
            obuf->flag |= RB_FILL;
#endif /* FORMAT_NICE */
            h_env->flushline(indent, 0, h_env->limit);
#ifdef FORMAT_NICE
            obuf->flag &= ~RB_FILL;
#endif /* FORMAT_NICE */
        }
    }
}

void init_henv(struct html_feed_environ *h_env, struct readbuffer *obuf, TextLineList *buf,
               int limit, int indent)
{
    h_env->Initialize(buf, obuf, limit, indent);
}

void completeHTMLstream(struct html_feed_environ *h_env, struct readbuffer *obuf, HtmlContext *seq)
{
    close_anchor(h_env, obuf, seq);
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
        HTMLlineproc0("</textarea>", h_env, true, seq);
    /* for unbalanced select tag */
    if (obuf->flag & RB_INSELECT)
        HTMLlineproc0("</select>", h_env, true, seq);
    if (obuf->flag & RB_TITLE)
        HTMLlineproc0("</title>", h_env, true, seq);

    /* for unbalanced table tag */
    if (obuf->table_level >= MAX_TABLE)
        obuf->table_level = MAX_TABLE - 1;

    while (obuf->table_level >= 0)
    {
        table_mode[obuf->table_level].pre_mode &= ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
        HTMLlineproc0("</table>", h_env, true, seq);
    }
}

