#include "fm.h"
#include "indep.h"
#include "types.h"
#include "etc.h"
#include "anchor.h"
#include "myctype.h"
#include "regex.h"
#include "file.h"
#include "form.h"
#include "buffer.h"

#define FIRST_ANCHOR_SIZE 30

Anchor *AnchorList::Put(const Anchor &a)
{
    if (anchors.empty() || anchors.back().start.Cmp(a.start) < 0)
    {
        // push_back
        anchors.push_back(a);
        return &anchors.back();
    }

    // search insert position
    auto n = anchors.size();
    int i = 0;
    for (; i < n; i++)
    {
        if (anchors[i].start.Cmp(a.start) >= 0)
        {
            break;
        }
    }
    // move back for insert
    anchors.resize(n + 1);
    for (int j = n; j > i; j--)
    {
        anchors[j] = anchors[j - 1];
    }
    // insert
    anchors[i] = a;
    return &anchors[i];
}

int Anchor::CmpOnAnchor(const BufferPoint &bp) const
{
    if (bp.Cmp(start) < 0)
        return -1;
    if (end.Cmp(bp) <= 0)
        return 1;
    return 0;
}

const Anchor *
AnchorList::RetrieveAnchor(const BufferPoint &bp) const
{
    if (anchors.empty())
        return nullptr;

    if (m_acache < 0 || m_acache >= anchors.size())
        m_acache = 0;

    size_t b, e;
    for (b = 0, e = anchors.size() - 1; b <= e; m_acache = (b + e) / 2)
    {
        auto a = &anchors[m_acache];
        auto cmp = a->CmpOnAnchor(bp);
        if (cmp == 0)
            return a;
        else if (cmp > 0)
            b = m_acache + 1;
        else if (m_acache == 0)
            return NULL;
        else
            e = m_acache - 1;
    }

    return NULL;
}

const Anchor *
retrieveCurrentAnchor(BufferPtr buf)
{
    if (buf->currentLine == NULL)
        return NULL;
    return buf->href.RetrieveAnchor(buf->currentLine->linenumber, buf->pos);
}

const Anchor *
retrieveCurrentImg(BufferPtr buf)
{
    if (buf->currentLine == NULL)
        return NULL;
    return buf->img.RetrieveAnchor(buf->currentLine->linenumber, buf->pos);
}

const Anchor *
retrieveCurrentForm(BufferPtr buf)
{
    if (buf->currentLine == NULL)
        return NULL;
    return buf->formitem.RetrieveAnchor(buf->currentLine->linenumber, buf->pos);
}

const Anchor *
AnchorList::SearchByUrl(const char *str) const
{
    for (auto &a : anchors)
    {
        if (a.hseq < 0)
            continue;
        if (a.url == str)
            return &a;
    }
    return nullptr;
}

const Anchor *
searchURLLabel(BufferPtr buf, char *url)
{
    return buf->name.SearchByUrl(url);
}

static Anchor *
_put_anchor_news(BufferPtr buf, char *p1, char *p2, int line, int pos)
{
    if (*p1 == '<')
    {
        p1++;
        if (*(p2 - 1) == '>')
            p2--;
    }
    auto tmp = wc_Str_conv_strict(Strnew_charp_n(p1, p2 - p1), InnerCharset,
                                  buf->document_charset);
    tmp = Sprintf("news:%s", file_quote(tmp->ptr));

    auto a = Anchor::CreateHref(tmp->ptr, NULL, NO_REFERER, NULL, '\0', line, pos);
    return buf->href.Put(a);
}

static Anchor *
_put_anchor_all(BufferPtr buf, char *p1, char *p2, int line, int pos)
{
    auto tmp = wc_Str_conv_strict(Strnew_charp_n(p1, p2 - p1), InnerCharset,
                                  buf->document_charset);
    auto a = Anchor::CreateHref(url_quote(tmp->ptr), NULL, NO_REFERER, NULL,
                                '\0', line, pos);
    return buf->href.Put(a);
}

static void
reseq_anchor0(AnchorList &al, short *seqmap)
{
    for (int i = 0; i < al.size(); i++)
    {
        auto a = &al.anchors[i];
        if (a->hseq >= 0)
        {
            a->hseq = seqmap[a->hseq];
        }
    }
}

/* renumber anchor */
static void
reseq_anchor(BufferPtr buf)
{
    int i, j, n, nmark = (buf->hmarklist) ? buf->hmarklist->nmark : 0;
    short *seqmap;
    HmarkerList *ml = NULL;

    if (!buf->href)
        return;

    n = nmark;
    for (i = 0; i < buf->href.size(); i++)
    {
        auto a = &buf->href.anchors[i];
        if (a->hseq == -2)
            n++;
    }

    if (n == nmark)
        return;

    seqmap = NewAtom_N(short, n);

    for (i = 0; i < n; i++)
        seqmap[i] = i;

    n = nmark;
    for (i = 0; i < buf->href.size(); i++)
    {
        auto a = &buf->href.anchors[i];
        if (a->hseq == -2)
        {
            a->hseq = n;
            auto a1 = buf->href.ClosestNext(NULL, a->start.pos,
                                            a->start.line);
            a1 = buf->formitem.ClosestNext(a1, a->start.pos,
                                           a->start.line);
            if (a1 && a1->hseq >= 0)
            {
                seqmap[n] = seqmap[a1->hseq];
                for (j = a1->hseq; j < nmark; j++)
                    seqmap[j]++;
            }
            ml = putHmarker(ml, a->start.line, a->start.pos, seqmap[n]);
            n++;
        }
    }

    for (i = 0; i < nmark; i++)
    {
        ml = putHmarker(ml, buf->hmarklist->marks[i].line,
                        buf->hmarklist->marks[i].pos, seqmap[i]);
    }
    buf->hmarklist = ml;

    reseq_anchor0(buf->href, seqmap);
    reseq_anchor0(buf->formitem, seqmap);
}

static char *
reAnchorPos(BufferPtr buf, Line *l, char *p1, char *p2,
            Anchor *(*anchorproc)(BufferPtr, char *, char *, int, int))
{
    Anchor *a;
    int spos, epos;
    int i, hseq = -2;

    spos = p1 - l->lineBuf;
    epos = p2 - l->lineBuf;
    for (i = spos; i < epos; i++)
    {
        if (l->propBuf[i] & (PE_ANCHOR | PE_FORM))
            return p2;
    }
    for (i = spos; i < epos; i++)
        l->propBuf[i] |= PE_ANCHOR;
    while (spos > l->len && l->next && l->next->bpos)
    {
        spos -= l->len;
        epos -= l->len;
        l = l->next;
    }
    while (1)
    {
        a = anchorproc(buf, p1, p2, l->linenumber, spos);
        a->hseq = hseq;
        if (hseq == -2)
        {
            reseq_anchor(buf);
            hseq = a->hseq;
        }
        a->end.line = l->linenumber;
        if (epos > l->len && l->next && l->next->bpos)
        {
            a->end.pos = l->len;
            spos = 0;
            epos -= l->len;
            l = l->next;
        }
        else
        {
            a->end.pos = epos;
            break;
        }
    }
    return p2;
}

void reAnchorWord(BufferPtr buf, Line *l, int spos, int epos)
{
    reAnchorPos(buf, l, &l->lineBuf[spos], &l->lineBuf[epos], _put_anchor_all);
}

/* search regexp and register them as anchors */
/* returns error message if any               */
static char *
reAnchorAny(BufferPtr buf, char *re,
            Anchor *(*anchorproc)(BufferPtr, char *, char *, int, int))
{
    Line *l;
    char *p = NULL, *p1, *p2;

    if (re == NULL || *re == '\0')
    {
        return NULL;
    }
    if ((re = regexCompile(re, 1)) != NULL)
    {
        return re;
    }
    for (l = MarkAllPages ? buf->firstLine : buf->topLine; l != NULL &&
                                                           (MarkAllPages || l->linenumber < buf->topLine->linenumber + LINES - 1);
         l = l->next)
    {
        if (p && l->bpos)
            goto next_line;
        p = l->lineBuf;
        for (;;)
        {
            if (regexMatch(p, &l->lineBuf[l->size] - p, p == l->lineBuf) == 1)
            {
                matchedPosition(&p1, &p2);
                p = reAnchorPos(buf, l, p1, p2, anchorproc);
            }
            else
                break;
        }
    next_line:
        if (MarkAllPages && l->next == NULL && buf->pagerSource &&
            !(buf->bufferprop & BP_CLOSE))
            getNextPage(buf, PagerMax);
    }
    return NULL;
}

char *
reAnchor(BufferPtr buf, char *re)
{
    return reAnchorAny(buf, re, _put_anchor_all);
}

#ifdef USE_NNTP
char *
reAnchorNews(BufferPtr buf, char *re)
{
    return reAnchorAny(buf, re, _put_anchor_news);
}

char *
reAnchorNewsheader(BufferPtr buf)
{
    Line *l;
    char *p, *p1, *p2;
    static const char *header_mid[] = {
        "Message-Id:", "References:", "In-Reply-To:", NULL};
    static const char *header_group[] = {
        "Newsgroups:", NULL};
    const char **header;
    const char **q;
    int i, search = FALSE;

    if (!buf || !buf->firstLine)
        return NULL;
    for (i = 0; i <= 1; i++)
    {
        if (i == 0)
        {
            regexCompile("<[!-;=?-~]+@[a-zA-Z0-9\\.\\-_]+>", 1);
            header = header_mid;
        }
        else
        {
            regexCompile("[a-zA-Z0-9\\.\\-_]+", 1);
            header = header_group;
        }
        for (l = buf->firstLine; l != NULL && l->real_linenumber == 0;
             l = l->next)
        {
            if (l->bpos)
                continue;
            p = l->lineBuf;
            if (!IS_SPACE(*p))
            {
                search = FALSE;
                for (q = header; *q; q++)
                {
                    if (!strncasecmp(p, *q, strlen(*q)))
                    {
                        search = TRUE;
                        p = strchr(p, ':') + 1;
                        break;
                    }
                }
            }
            if (!search)
                continue;
            for (;;)
            {
                if (regexMatch(p, &l->lineBuf[l->size] - p, p == l->lineBuf) == 1)
                {
                    matchedPosition(&p1, &p2);
                    p = reAnchorPos(buf, l, p1, p2, _put_anchor_news);
                }
                else
                    break;
            }
        }
    }
    reseq_anchor(buf);
    return NULL;
}
#endif /* USE_NNTP */

#define FIRST_MARKER_SIZE 30
HmarkerList *
putHmarker(HmarkerList *ml, int line, int pos, int seq)
{
    if (ml == NULL)
    {
        ml = New(HmarkerList);
        ml->marks = NULL;
        ml->nmark = 0;
        ml->markmax = 0;
    }
    if (ml->markmax == 0)
    {
        ml->markmax = FIRST_MARKER_SIZE;
        ml->marks = NewAtom_N(BufferPoint, ml->markmax);
        bzero(ml->marks, sizeof(BufferPoint) * ml->markmax);
    }
    if (seq + 1 > ml->nmark)
        ml->nmark = seq + 1;
    if (ml->nmark >= ml->markmax)
    {
        ml->markmax = ml->nmark * 2;
        ml->marks = New_Reuse(BufferPoint, ml->marks, ml->markmax);
    }
    ml->marks[seq].line = line;
    ml->marks[seq].pos = pos;
    ml->marks[seq].invalid = 0;
    return ml;
}

const Anchor *
AnchorList::ClosestNext(const Anchor *an, int x, int y) const
{
    for (auto &a : anchors)
    {
        if (a.hseq < 0)
            continue;
        if (a.start.line > y ||
            (a.start.line == y && a.start.pos > x))
        {
            if (an == NULL || an->start.line > a.start.line ||
                (an->start.line == a.start.line &&
                 an->start.pos > a.start.pos))
                an = &a;
        }
    }
    return an;
}

const Anchor *
AnchorList::ClosestPrev(const Anchor *an, int x, int y) const
{
    for (auto &a : anchors)
    {
        if (a.hseq < 0)
            continue;
        if (a.end.line < y ||
            (a.end.line == y && a.end.pos <= x))
        {
            if (an == NULL || an->end.line < a.end.line ||
                (an->end.line == a.end.line &&
                 an->end.pos < a.end.pos))
                an = &a;
        }
    }
    return an;
}

void shiftAnchorPosition(AnchorList &al, HmarkerList *hl, const BufferPoint &bp,
                         int shift)
{
    if (al.size() == 0)
        return;

    auto s = al.size() / 2;
    auto e = al.size() - 1;
    for (auto b = 0; b <= e; s = (b + e + 1) / 2)
    {
        auto a = &al.anchors[s];
        auto cmp = a->CmpOnAnchor(bp);
        if (cmp == 0)
            break;
        else if (cmp > 0)
            b = s + 1;
        else if (s == 0)
            break;
        else
            e = s - 1;
    }
    for (; s < al.size(); s++)
    {
        auto a = &al.anchors[s];
        if (a->start.line > bp.line)
            break;
        if (a->start.pos > bp.pos)
        {
            a->start.pos += shift;
            if (hl->marks[a->hseq].line == bp.line)
                hl->marks[a->hseq].pos = a->start.pos;
        }
        if (a->end.pos >= bp.pos)
            a->end.pos += shift;
    }
}

void addMultirowsImg(BufferPtr buf, AnchorList &al)
{
    if (al.size() == 0)
        return;

    for (int i = 0; i < al.size(); i++)
    {
        auto a_img = al.anchors[i];
        auto img = a_img.image;
        if (a_img.hseq < 0 || !img || img->rows <= 1)
            continue;
        auto l = buf->firstLine;
        for (; l != NULL; l = l->next)
        {
            if (l->linenumber == img->y)
                break;
        }
        if (!l)
            continue;

        Line *ls;
        if (a_img.y == a_img.start.line)
            ls = l;
        else
        {
            for (ls = l; ls != NULL;
                 ls = (a_img.y < a_img.start.line) ? ls->next : ls->prev)
            {
                if (ls->linenumber == a_img.start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        Anchor a_href;
        {
            auto a = buf->href.RetrieveAnchor(a_img.start.line, a_img.start.pos);
            if (a)
                a_href = *a;
            else
                a_href.url = {};
        }
        Anchor a_form;
        {
            auto a = buf->formitem.RetrieveAnchor(a_img.start.line, a_img.start.pos);
            if (a)
                a_form = *a;
            else
                a_form.url = {};
        }
        auto col = ls->COLPOS(a_img.start.pos);
        auto ecol = ls->COLPOS(a_img.end.pos);
        for (int j = 0; l && j < img->rows; l = l->next, j++)
        {
            if (a_img.start.line == l->linenumber)
                continue;
            auto pos = columnPos(l, col);
            {
                // img
                auto a = Anchor::CreateImage(a_img.url, a_img.title, l->linenumber, pos);
                a.hseq = -a_img.hseq;
                a.slave = TRUE;
                a.image = img;
                a.end.pos = pos + ecol - col;
                buf->img.Put(a);
                for (int k = pos; k < a.end.pos; k++)
                    l->propBuf[k] |= PE_IMAGE;
            }
            if (a_href.url.size())
            {
                // href
                auto a = Anchor::CreateHref(a_href.url, a_href.target,
                                            a_href.referer, a_href.title,
                                            a_href.accesskey, l->linenumber, pos);
                a.hseq = a_href.hseq;
                a.slave = TRUE;
                a.end.pos = pos + ecol - col;
                buf->href.Put(a);
                for (int k = pos; k < a.end.pos; k++)
                    l->propBuf[k] |= PE_ANCHOR;
            }
            if (a_form.item)
            {
                Anchor a = a_form;
                a.start = BufferPoint{
                    line : l->linenumber,
                    pos : pos
                };
                a.end = BufferPoint{
                    line : l->linenumber,
                    pos : pos + ecol - col
                };
                buf->formitem.Put(a);
            }
        }
        img->rows = 0;
    }
}

void addMultirowsForm(BufferPtr buf, AnchorList &al)
{
    if (al.size() == 0)
        return;

    for (int i = 0; i < al.size(); i++)
    {
        auto a_form = al.anchors[i];
        al.anchors[i].rows = 1;
        if (a_form.hseq < 0 || a_form.rows <= 1)
            continue;
        auto l = buf->firstLine;
        for (; l != NULL; l = l->next)
        {
            if (l->linenumber == a_form.y)
                break;
        }
        if (!l)
            continue;
        Line *ls;
        if (a_form.y == a_form.start.line)
            ls = l;
        else
        {
            for (ls = l; ls != NULL;
                 ls = (a_form.y < a_form.start.line) ? ls->next : ls->prev)
            {
                if (ls->linenumber == a_form.start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        auto fi = a_form.item;
        auto col = ls->COLPOS(a_form.start.pos);
        auto ecol = ls->COLPOS(a_form.end.pos);
        for (auto j = 0; l && j < a_form.rows; l = l->next, j++)
        {
            auto pos = columnPos(l, col);
            if (j == 0)
            {
                buf->hmarklist->marks[a_form.hseq].line = l->linenumber;
                buf->hmarklist->marks[a_form.hseq].pos = pos;
            }
            if (a_form.start.line == l->linenumber)
                continue;

            auto a = a_form;
            a.start = BufferPoint{
                line : l->linenumber,
                pos : pos
            };
            a.end = BufferPoint{
                line : l->linenumber,
                pos : pos + ecol - col
            };
            buf->formitem.Put(a);
            l->lineBuf[pos - 1] = '[';
            l->lineBuf[a.end.pos] = ']';
            for (int k = pos; k < a.end.pos; k++)
                l->propBuf[k] |= PE_FORM;
        }
    }
}

char *
getAnchorText(BufferPtr buf, AnchorList &al, Anchor *a)
{
    int hseq, i;
    Line *l;
    Str tmp = NULL;
    char *p, *ep;

    if (!a || a->hseq < 0)
        return NULL;
    hseq = a->hseq;
    l = buf->firstLine;
    for (i = 0; i < al.size(); i++)
    {
        a = &al.anchors[i];
        if (a->hseq != hseq)
            continue;
        for (; l; l = l->next)
        {
            if (l->linenumber == a->start.line)
                break;
        }
        if (!l)
            break;
        p = l->lineBuf + a->start.pos;
        ep = l->lineBuf + a->end.pos;
        for (; p < ep && IS_SPACE(*p); p++)
            ;
        if (p == ep)
            continue;
        if (!tmp)
            tmp = Strnew_size(ep - p);
        else
            tmp->Push(' ');
        tmp->Push(p, ep - p);
    }
    return tmp ? tmp->ptr : NULL;
}

BufferPtr
link_list_panel(BufferPtr buf)
{
    /* FIXME: gettextize? */
    Str tmp = Strnew("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

    if (buf->bufferprop & BP_INTERNAL ||
        (buf->linklist == NULL && !buf->href && !buf->img))
    {
        return NULL;
    }

    if (buf->linklist)
    {
        tmp->Push("<hr><h2>Links</h2>\n<ol>\n");
        for (auto l = buf->linklist; l; l = l->next)
        {
            const char *u;
            const char *p;
            const char *t;
            if (l->url)
            {
                ParsedURL pu;
                pu.Parse2(l->url, baseURL(buf));
                p = parsedURL2Str(&pu)->ptr;
                u = html_quote(p);
                if (DecodeURL)
                    p = html_quote(url_unquote_conv(p, buf->document_charset));
                else
                    p = u;
            }
            else
                u = p = "";
            if (l->type == LINK_TYPE_REL)
                t = " [Rel]";
            else if (l->type == LINK_TYPE_REV)
                t = " [Rev]";
            else
                t = "";
            t = Sprintf("%s%s\n", l->title ? l->title : "", t)->ptr;
            t = html_quote(t);
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                           "\n", NULL);
        }
        tmp->Push("</ol>\n");
    }

    if (buf->href)
    {
        tmp->Push("<hr><h2>Anchors</h2>\n<ol>\n");
        auto &al = buf->href;
        for (int i = 0; i < al.size(); i++)
        {
            auto a = &al.anchors[i];
            if (a->hseq < 0 || a->slave)
                continue;
            ParsedURL pu;
            pu.Parse2(const_cast<char*>(a->url.c_str()), baseURL(buf));
            auto p = parsedURL2Str(&pu)->ptr;
            auto u = html_quote(p);
            if (DecodeURL)
                p = html_quote(url_unquote_conv(p, buf->document_charset));
            else
                p = u;
            auto t = getAnchorText(buf, al, a);
            t = t ? html_quote(t) : (char *)"";
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                           "\n", NULL);
        }
        tmp->Push("</ol>\n");
    }

    if (buf->img)
    {
        tmp->Push("<hr><h2>Images</h2>\n<ol>\n");
        auto &al = buf->img;
        for (int i = 0; i < al.size(); i++)
        {
            auto a = &al.anchors[i];
            if (a->slave)
                continue;
            ParsedURL pu;
            pu.Parse2(a->url, baseURL(buf));
            auto p = parsedURL2Str(&pu)->ptr;
            auto u = html_quote(p);
            if (DecodeURL)
                p = html_quote(url_unquote_conv(p, buf->document_charset));
            else
                p = u;
            const char *t;
            if (a->title && *a->title)
                t = html_quote(a->title);
            else if (DecodeURL)
                t = html_quote(url_unquote_conv(a->url.c_str(), buf->document_charset));
            else
                t = html_quote(a->url.c_str());
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                           "\n", NULL);
            a = const_cast<Anchor *>(buf->formitem.RetrieveAnchor(a->start.line, a->start.pos));
            if (!a)
                continue;
            auto fi = a->item;
            fi = fi->parent->item;
            if (fi->parent->method == FORM_METHOD_INTERNAL &&
                fi->parent->action->Cmp("map") == 0 && fi->value)
            {
                MapList *ml = searchMapList(buf, fi->value->ptr);
                ListItem *mi;
                MapArea *m;
                if (!ml)
                    continue;
                tmp->Push("<br>\n<b>Image map</b>\n<ol>\n");
                for (mi = ml->area->first; mi != NULL; mi = mi->next)
                {
                    m = (MapArea *)mi->ptr;
                    if (!m)
                        continue;
                    pu.Parse2(m->url, baseURL(buf));
                    p = parsedURL2Str(&pu)->ptr;
                    u = html_quote(p);
                    if (DecodeURL)
                        p = html_quote(url_unquote_conv(p,
                                                        buf->document_charset));
                    else
                        p = u;
                    if (m->alt && *m->alt)
                        t = html_quote(m->alt);
                    else if (DecodeURL)
                        t = html_quote(url_unquote_conv(m->url,
                                                        buf->document_charset));
                    else
                        t = html_quote(m->url);
                    Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t,
                                   "</a><br>", p, "\n", NULL);
                }
                tmp->Push("</ol>\n");
            }
        }
        tmp->Push("</ol>\n");
    }

    return loadHTMLString(tmp);
}
