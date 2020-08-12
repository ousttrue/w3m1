#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "html/anchor.h"
#include "myctype.h"
#include "regex.h"
#include "file.h"
#include "html/form.h"
#include "frontend/buffer.h"
#include "html/maparea.h"
#include "html/image.h"
#include "html/html_processor.h"
#include "transport/loader.h"
#include "frontend/terms.h"

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
    if (buf->CurrentLine() == NULL)
        return NULL;
    return buf->href.RetrieveAnchor(buf->CurrentLine()->linenumber, buf->pos);
}

const Anchor *
retrieveCurrentImg(BufferPtr buf)
{
    if (buf->CurrentLine() == NULL)
        return NULL;
    return buf->img.RetrieveAnchor(buf->CurrentLine()->linenumber, buf->pos);
}

const Anchor *
retrieveCurrentForm(BufferPtr buf)
{
    if (buf->CurrentLine() == NULL)
        return NULL;
    return buf->formitem.RetrieveAnchor(buf->CurrentLine()->linenumber, buf->pos);
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
    auto tmp = wc_Str_conv_strict(Strnew_charp_n(p1, p2 - p1), w3mApp::Instance().InnerCharset,
                                  buf->document_charset);
    tmp = Sprintf("news:%s", file_quote(tmp->ptr));

    auto a = Anchor::CreateHref(tmp->ptr, NULL, HttpReferrerPolicy::NoReferer, NULL, '\0', line, pos);
    return buf->href.Put(a);
}

static Anchor *
_put_anchor_all(BufferPtr buf, char *p1, char *p2, int line, int pos)
{
    auto tmp = wc_Str_conv_strict(Strnew_charp_n(p1, p2 - p1), w3mApp::Instance().InnerCharset,
                                  buf->document_charset);
    auto a = Anchor::CreateHref(url_quote(tmp->ptr), NULL, HttpReferrerPolicy::NoReferer, NULL,
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
    int i, j, n, nmark = buf->hmarklist.size();
    short *seqmap;

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
            buf->putHmarker(a->start.line, a->start.pos, seqmap[n]);
            n++;
        }
    }

    for (i = 0; i < nmark; i++)
    {
        buf->putHmarker(buf->hmarklist[i].line, buf->hmarklist[i].pos, seqmap[i]);
    }

    reseq_anchor0(buf->href, seqmap);
    reseq_anchor0(buf->formitem, seqmap);
}

static char *
reAnchorPos(BufferPtr buf, LinePtr l, char *p1, char *p2,
            Anchor *(*anchorproc)(BufferPtr, char *, char *, int, int))
{
    Anchor *a;
    int spos, epos;
    int i, hseq = -2;

    spos = p1 - l->lineBuf();
    epos = p2 - l->lineBuf();
    for (i = spos; i < epos; i++)
    {
        if (l->propBuf()[i] & (PE_ANCHOR | PE_FORM))
            return p2;
    }
    for (i = spos; i < epos; i++)
        l->propBuf()[i] |= PE_ANCHOR;
    while (spos > l->len() && buf->NextLine(l) && buf->NextLine(l)->bpos)
    {
        spos -= l->len();
        epos -= l->len();
        l = buf->NextLine(l);
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
        if (epos > l->len() && buf->NextLine(l) && buf->NextLine(l)->bpos)
        {
            a->end.pos = l->len();
            spos = 0;
            epos -= l->len();
            l = buf->NextLine(l);
        }
        else
        {
            a->end.pos = epos;
            break;
        }
    }
    return p2;
}

void reAnchorWord(BufferPtr buf, LinePtr l, int spos, int epos)
{
    reAnchorPos(buf, l, &l->lineBuf()[spos], &l->lineBuf()[epos], _put_anchor_all);
}

/* search regexp and register them as anchors */
/* returns error message if any               */
static char *
reAnchorAny(BufferPtr buf, char *re,
            Anchor *(*anchorproc)(BufferPtr, char *, char *, int, int))
{
    LinePtr l;
    char *p = NULL, *p1, *p2;

    if (re == NULL || *re == '\0')
    {
        return NULL;
    }
    if ((re = regexCompile(re, 1)) != NULL)
    {
        return re;
    }
    for (l = MarkAllPages ? buf->FirstLine() : buf->TopLine(); l != NULL &&
                                                           (MarkAllPages || l->linenumber < buf->TopLine()->linenumber + ::LINES - 1);
         l = buf->NextLine(l))
    {
        if (p && l->bpos)
            goto next_line;
        p = l->lineBuf();
        for (;;)
        {
            if (regexMatch(p, &l->lineBuf()[l->len()] - p, p == l->lineBuf()) == 1)
            {
                matchedPosition(&p1, &p2);
                p = reAnchorPos(buf, l, p1, p2, anchorproc);
            }
            else
                break;
        }
    next_line:
        if (MarkAllPages && buf->NextLine(l) == NULL && buf->pagerSource &&
            !(buf->bufferprop & BP_CLOSE))
            getNextPage(buf, w3mApp::Instance().PagerMax);
    }
    return NULL;
}

char *
reAnchor(BufferPtr buf, char *re)
{
    return reAnchorAny(buf, re, _put_anchor_all);
}

char *
reAnchorNews(BufferPtr buf, char *re)
{
    return reAnchorAny(buf, re, _put_anchor_news);
}

char *
reAnchorNewsheader(BufferPtr buf)
{
    LinePtr l;
    char *p, *p1, *p2;
    static const char *header_mid[] = {
        "Message-Id:", "References:", "In-Reply-To:", NULL};
    static const char *header_group[] = {
        "Newsgroups:", NULL};
    const char **header;
    const char **q;
    int i, search = FALSE;

    if (!buf || !buf->FirstLine())
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
        for (l = buf->FirstLine(); l != NULL && l->real_linenumber == 0;
             l = buf->NextLine(l))
        {
            if (l->bpos)
                continue;
            p = l->lineBuf();
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
                if (regexMatch(p, &l->lineBuf()[l->len()] - p, p == l->lineBuf()) == 1)
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
        auto l = buf->FirstLine();
        for (; l != NULL; l = buf->NextLine(l))
        {
            if (l->linenumber == img->y)
                break;
        }
        if (!l)
            continue;

        LinePtr ls;
        if (a_img.y == a_img.start.line)
            ls = l;
        else
        {
            for (ls = l; ls != NULL;
                 ls = (a_img.y < a_img.start.line) ? buf->NextLine(ls) : buf->PrevLine(ls))
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
        for (int j = 0; l && j < img->rows; l = buf->NextLine(l), j++)
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
                    l->propBuf()[k] |= PE_IMAGE;
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
                    l->propBuf()[k] |= PE_ANCHOR;
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
        auto l = buf->FirstLine();
        for (; l != NULL; l = buf->NextLine(l))
        {
            if (l->linenumber == a_form.y)
                break;
        }
        if (!l)
            continue;
        LinePtr ls;
        if (a_form.y == a_form.start.line)
            ls = l;
        else
        {
            for (ls = l; ls != NULL;
                 ls = (a_form.y < a_form.start.line) ? buf->NextLine(ls) : buf->PrevLine(ls))
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
        for (auto j = 0; l && j < a_form.rows; l = buf->NextLine(l), j++)
        {
            auto pos = columnPos(l, col);
            if (j == 0)
            {
                buf->hmarklist[a_form.hseq].line = l->linenumber;
                buf->hmarklist[a_form.hseq].pos = pos;
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
            l->lineBuf()[pos - 1] = '[';
            l->lineBuf()[a.end.pos] = ']';
            for (int k = pos; k < a.end.pos; k++)
                l->propBuf()[k] |= PE_FORM;
        }
    }
}

char *
getAnchorText(BufferPtr buf, AnchorList &al, Anchor *a)
{
    int hseq, i;
    LinePtr l;
    Str tmp = NULL;
    char *p, *ep;

    if (!a || a->hseq < 0)
        return NULL;
    hseq = a->hseq;
    l = buf->FirstLine();
    for (i = 0; i < al.size(); i++)
    {
        a = &al.anchors[i];
        if (a->hseq != hseq)
            continue;
        for (; l; l = buf->NextLine(l))
        {
            if (l->linenumber == a->start.line)
                break;
        }
        if (!l)
            break;
        p = l->lineBuf() + a->start.pos;
        ep = l->lineBuf() + a->end.pos;
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
    if (buf->bufferprop & BP_INTERNAL ||
        (buf->linklist.empty() && !buf->href && !buf->img))
    {
        return NULL;
    }

    /* FIXME: gettextize? */
    Str tmp = Strnew("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

    if (buf->linklist.size())
    {
        tmp->Push("<hr><h2>Links</h2>\n<ol>\n");
        for (auto &l : buf->linklist)
        {
            const char *u;
            const char *p;
            if (l.url().size())
            {
                auto pu = URL::Parse(l.url(), buf->BaseURL());
                p = pu.ToStr()->ptr;
                u = html_quote(p);
                if (DecodeURL)
                    p = html_quote(url_unquote_conv(p, buf->document_charset));
                else
                    p = u;
            }
            else
                u = p = "";

            auto t = Strnew_m_charp(l.title(), l.type(), "\n")->ptr;
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
            auto pu = URL::Parse(const_cast<char *>(a->url.c_str()), buf->BaseURL());
            auto p = pu.ToStr()->ptr;
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
            auto pu = URL::Parse(a->url, buf->BaseURL());
            auto p = pu.ToStr()->ptr;
            auto u = html_quote(p);
            if (DecodeURL)
                p = html_quote(url_unquote_conv(p, buf->document_charset));
            else
                p = u;
            const char *t;
            if (a->title.size() && a->title[0])
                t = html_quote(a->title.c_str());
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
                    auto pu = URL::Parse(m->url, buf->BaseURL());
                    p = pu.ToStr()->ptr;
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
