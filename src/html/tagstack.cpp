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

#include "html/html_context.h"

#include "frontend/buffer.h"
#include "frontend/line.h"
#include "charset.h"

html_feed_environ::html_feed_environ(readbuffer *obuf, TextLineList *buf, int limit, int indent)
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
        struct readbuffer o;
        o.line = Strnew_size(width + 20);
        o.pos = obuf->pos;
        o.flag = obuf->flag;
        o.top_margin = -1;
        o.bottom_margin = -1;
        o.line->Push("<pre_int>");
        for (int i = 0; i < o.pos; i++)
            o.line->Push(' ');
        o.line->Push("</pre_int>");

        html_feed_environ h(&o, nullptr, width, indent);
        for (int i = 0; i < obuf->top_margin; i++)
            // flushline(h_env, &o, indent, force, width);
            // TODO
            flushline(indent, force, width);
    }

    if (force == 1 || obuf->flag & RB_NFLUSHED)
    {
        TextLine *lbuf = newTextLine(line, obuf->pos);
        if (obuf->RB_GET_ALIGN() == RB_CENTER)
        {
            align(lbuf, width, ALIGN_CENTER);
        }
        else if (obuf->RB_GET_ALIGN() == RB_RIGHT)
        {
            align(lbuf, width, ALIGN_RIGHT);
        }
        else if (obuf->RB_GET_ALIGN() == RB_LEFT && obuf->flag & RB_INTABLE)
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
        struct readbuffer o;
        html_feed_environ h(&o, NULL, width, indent);
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
