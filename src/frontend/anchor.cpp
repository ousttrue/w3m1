
#include "indep.h"
#include "gc_helper.h"
#include "frontend/anchor.h"
#include "myctype.h"
#include "regex.h"
#include "file.h"
#include "textlist.h"
#include "html/form.h"
#include "frontend/buffer.h"
#include "html/maparea.h"
#include "html/image.h"
#include "html/html_context.h"
#include "loader.h"

#include "frontend/terminal.h"

#define FIRST_ANCHOR_SIZE 30

// push anchor, keep ordered
void AnchorList::Put(const AnchorPtr &a)
{
    if (anchors.empty() || anchors.back()->start.Cmp(a->start) < 0)
    {
        // push_back
        anchors.push_back(a);
        return;
    }

    // search insert position
    auto n = anchors.size();
    int i = 0;
    for (; i < n; i++)
    {
        if (anchors[i]->start.Cmp(a->start) >= 0)
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
}

int Anchor::CmpOnAnchor(const BufferPoint &bp) const
{
    if (bp.Cmp(start) < 0)
        return -1;
    if (end.Cmp(bp) <= 0)
        return 1;
    return 0;
}

AnchorPtr AnchorList::RetrieveAnchor(const BufferPoint &bp) const
{
    if (bp.invalid)
    {
        return nullptr;
    }

    if (anchors.empty())
        return nullptr;

    if (m_acache < 0 || m_acache >= anchors.size())
        m_acache = 0;

    size_t b, e;
    for (b = 0, e = anchors.size() - 1; b <= e; m_acache = (b + e) / 2)
    {
        auto a = anchors[m_acache];
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

AnchorPtr AnchorList::SearchByUrl(const char *str) const
{
    for (auto &a : anchors)
    {
        if (a->hseq < 0)
            continue;
        if (a->url == str)
            return a;
    }
    return nullptr;
}

static AnchorPtr
_put_anchor_news(BufferPtr buf, char *p1, char *p2, int line, int pos)
{
    if (*p1 == '<')
    {
        p1++;
        if (*(p2 - 1) == '>')
            p2--;
    }
    auto tmp = wc_Str_conv_strict(Strnew_charp_n(p1, p2 - p1), w3mApp::Instance().InnerCharset,
                                  buf->m_document->document_charset);
    tmp = Sprintf("news:%s", file_quote(tmp->ptr));

    auto a = Anchor::CreateHref(tmp->ptr, "", HttpReferrerPolicy::NoReferer, "", '\0', line, pos);
    buf->m_document->href.Put(a);
    return a;
}

static AnchorPtr
_put_anchor_all(BufferPtr buf, char *p1, char *p2, int line, int pos)
{
    auto tmp = wc_Str_conv_strict(Strnew_charp_n(p1, p2 - p1), w3mApp::Instance().InnerCharset,
                                  buf->m_document->document_charset);
    auto a = Anchor::CreateHref(url_quote(tmp->ptr), "", HttpReferrerPolicy::NoReferer, "",
                                '\0', line, pos);
    buf->m_document->href.Put(a);
    return a;
}

static void
reseq_anchor0(AnchorList &al, short *seqmap)
{
    for (int i = 0; i < al.size(); i++)
    {
        auto a = al.anchors[i];
        if (a->hseq >= 0)
        {
            a->hseq = seqmap[a->hseq];
        }
    }
}

/* renumber anchor */
static void
reseq_anchor(const BufferPtr &buf)
{
    int i, j, n, nmark = buf->m_document->hmarklist.size();
    short *seqmap;

    if (!buf->m_document->href)
        return;

    n = nmark;
    for (i = 0; i < buf->m_document->href.size(); i++)
    {
        auto a = buf->m_document->href.anchors[i];
        if (a->hseq == -2)
            n++;
    }

    if (n == nmark)
        return;

    seqmap = NewAtom_N(short, n);

    for (i = 0; i < n; i++)
        seqmap[i] = i;

    n = nmark;
    for (i = 0; i < buf->m_document->href.size(); i++)
    {
        auto a = buf->m_document->href.anchors[i];
        if (a->hseq == -2)
        {
            a->hseq = n;
            auto a1 = buf->m_document->href.ClosestNext(NULL, a->start.pos,
                                            a->start.line);
            a1 = buf->m_document->formitem.ClosestNext(a1, a->start.pos,
                                           a->start.line);
            if (a1 && a1->hseq >= 0)
            {
                seqmap[n] = seqmap[a1->hseq];
                for (j = a1->hseq; j < nmark; j++)
                    seqmap[j]++;
            }
            buf->m_document->putHmarker(a->start.line, a->start.pos, seqmap[n]);
            n++;
        }
    }

    for (i = 0; i < nmark; i++)
    {
        buf->m_document->putHmarker(buf->m_document->hmarklist[i].line, buf->m_document->hmarklist[i].pos, seqmap[i]);
    }

    reseq_anchor0(buf->m_document->href, seqmap);
    reseq_anchor0(buf->m_document->formitem, seqmap);
}

static char *
reAnchorPos(BufferPtr buf, LinePtr l, char *p1, char *p2,
            AnchorPtr (*anchorproc)(BufferPtr, char *, char *, int, int))
{
    AnchorPtr a;
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
static const char *
reAnchorAny(BufferPtr buf, const char *re,
            AnchorPtr (*anchorproc)(BufferPtr, char *, char *, int, int))
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
    for (l = w3mApp::Instance().MarkAllPages ? buf->FirstLine() : buf->TopLine(); l != NULL &&
                                                                                  (w3mApp::Instance().MarkAllPages || l->linenumber < buf->TopLine()->linenumber + ::Terminal::lines() - 1);
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
        if (w3mApp::Instance().MarkAllPages && buf->NextLine(l) == NULL && buf->pagerSource &&
            !(buf->bufferprop & BP_CLOSE))
            getNextPage(buf, w3mApp::Instance().PagerMax);
    }
    return NULL;
}

const char *
reAnchor(BufferPtr buf, const char *re)
{
    return reAnchorAny(buf, re, _put_anchor_all);
}

const char *
reAnchorNews(BufferPtr buf, const char *re)
{
    return reAnchorAny(buf, re, _put_anchor_news);
}

char *
reAnchorNewsheader(const BufferPtr &buf)
{
    LinePtr l;
    char *p, *p1, *p2;
    static const char *header_mid[] = {
        "Message-Id:", "References:", "In-Reply-To:", NULL};
    static const char *header_group[] = {
        "Newsgroups:", NULL};
    const char **header;
    const char **q;
    int i, search = false;

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
                search = false;
                for (q = header; *q; q++)
                {
                    if (!strncasecmp(p, *q, strlen(*q)))
                    {
                        search = true;
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

AnchorPtr AnchorList::ClosestNext(const AnchorPtr &_an, int x, int y) const
{
    auto an = _an;
    for (auto &a : anchors)
    {
        if (a->hseq < 0)
            continue;
        if (a->start.line > y ||
            (a->start.line == y && a->start.pos > x))
        {
            if (an == NULL || an->start.line > a->start.line ||
                (an->start.line == a->start.line &&
                 an->start.pos > a->start.pos))
                an = a;
        }
    }
    return an;
}

AnchorPtr AnchorList::ClosestPrev(const AnchorPtr &_an, int x, int y) const
{
    auto an = _an;
    for (auto &a : anchors)
    {
        if (a->hseq < 0)
            continue;
        if (a->end.line < y ||
            (a->end.line == y && a->end.pos <= x))
        {
            if (an == NULL || an->end.line < a->end.line ||
                (an->end.line == a->end.line &&
                 an->end.pos < a->end.pos))
                an = a;
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
        auto img = a_img->image;
        if (a_img->hseq < 0 || !img || img->rows <= 1)
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
        if (a_img->y == a_img->start.line)
            ls = l;
        else
        {
            for (ls = l; ls != NULL;
                 ls = (a_img->y < a_img->start.line) ? buf->NextLine(ls) : buf->PrevLine(ls))
            {
                if (ls->linenumber == a_img->start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        Anchor a_href;
        {
            auto a = buf->m_document->href.RetrieveAnchor(a_img->start);
            if (a)
                a_href = *a;
            else
                a_href.url = {};
        }
        AnchorPtr a_form;
        {
            auto a = buf->m_document->formitem.RetrieveAnchor(a_img->start);
            if (a)
                a_form = a;
        }
        auto col = ls->COLPOS(a_img->start.pos);
        auto ecol = ls->COLPOS(a_img->end.pos);
        for (int j = 0; l && j < img->rows; l = buf->NextLine(l), j++)
        {
            if (a_img->start.line == l->linenumber)
                continue;
            auto pos = columnPos(l, col);
            {
                // img
                auto a = Anchor::CreateImage(a_img->url, a_img->title, l->linenumber, pos);
                a->hseq = -a_img->hseq;
                a->slave = true;
                a->image = img;
                a->end.pos = pos + ecol - col;
                buf->m_document->img.Put(a);
                for (int k = pos; k < a->end.pos; k++)
                    l->propBuf()[k] |= PE_IMAGE;
            }
            if (a_href.url.size())
            {
                // href
                auto a = Anchor::CreateHref(a_href.url, a_href.target,
                                            a_href.referer, a_href.title,
                                            a_href.accesskey, l->linenumber, pos);
                a->hseq = a_href.hseq;
                a->slave = true;
                a->end.pos = pos + ecol - col;
                buf->m_document->href.Put(a);
                for (int k = pos; k < a->end.pos; k++)
                    l->propBuf()[k] |= PE_ANCHOR;
            }
            if (a_form && a_form->item)
            {
                auto a = std::make_shared<Anchor>();
                *a = *a_form;
                a->start = BufferPoint{
                    line : l->linenumber,
                    pos : pos
                };
                a->end = BufferPoint{
                    line : l->linenumber,
                    pos : pos + ecol - col
                };
                buf->m_document->formitem.Put(a);
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
        al.anchors[i]->rows = 1;
        if (a_form->hseq < 0 || a_form->rows <= 1)
            continue;
        auto l = buf->FirstLine();
        for (; l != NULL; l = buf->NextLine(l))
        {
            if (l->linenumber == a_form->y)
                break;
        }
        if (!l)
            continue;
        LinePtr ls;
        if (a_form->y == a_form->start.line)
            ls = l;
        else
        {
            for (ls = l; ls != NULL;
                 ls = (a_form->y < a_form->start.line) ? buf->NextLine(ls) : buf->PrevLine(ls))
            {
                if (ls->linenumber == a_form->start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        auto fi = a_form->item;
        auto col = ls->COLPOS(a_form->start.pos);
        auto ecol = ls->COLPOS(a_form->end.pos);
        for (auto j = 0; l && j < a_form->rows; l = buf->NextLine(l), j++)
        {
            auto pos = columnPos(l, col);
            if (j == 0)
            {
                buf->m_document->hmarklist[a_form->hseq].line = l->linenumber;
                buf->m_document->hmarklist[a_form->hseq].pos = pos;
            }
            if (a_form->start.line == l->linenumber)
                continue;

            auto a = a_form;
            a->start = BufferPoint{
                line : l->linenumber,
                pos : pos
            };
            a->end = BufferPoint{
                line : l->linenumber,
                pos : pos + ecol - col
            };
            buf->m_document->formitem.Put(a);
            l->lineBuf()[pos - 1] = '[';
            l->lineBuf()[a->end.pos] = ']';
            for (int k = pos; k < a->end.pos; k++)
                l->propBuf()[k] |= PE_FORM;
        }
    }
}

char *
getAnchorText(BufferPtr buf, AnchorList &al, AnchorPtr a)
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
        a = al.anchors[i];
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
link_list_panel(const BufferPtr &buf)
{
    if (buf->bufferprop & BP_INTERNAL ||
        (buf->m_document->linklist.empty() && !buf->m_document->href && !buf->m_document->img))
    {
        return NULL;
    }

    /* FIXME: gettextize? */
    Str tmp = Strnew("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

    if (buf->m_document->linklist.size())
    {
        tmp->Push("<hr><h2>Links</h2>\n<ol>\n");
        for (auto &l : buf->m_document->linklist)
        {
            const char *u;
            const char *p;
            if (l.url().size())
            {
                auto pu = URL::Parse(l.url(), &buf->url);
                p = pu.ToStr()->ptr;
                u = html_quote(p);
                if (w3mApp::Instance().DecodeURL)
                    p = html_quote(url_unquote_conv(p, buf->m_document->document_charset));
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

    if (buf->m_document->href)
    {
        tmp->Push("<hr><h2>Anchors</h2>\n<ol>\n");
        auto &al = buf->m_document->href;
        for (int i = 0; i < al.size(); i++)
        {
            auto a = al.anchors[i];
            if (a->hseq < 0 || a->slave)
                continue;
            auto pu = URL::Parse(a->url, &buf->url);
            auto p = pu.ToStr()->ptr;
            auto u = html_quote(p);
            if (w3mApp::Instance().DecodeURL)
                p = html_quote(url_unquote_conv(p, buf->m_document->document_charset));
            else
                p = u;
            auto t = getAnchorText(buf, al, a);
            t = t ? html_quote(t) : (char *)"";
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                           "\n", NULL);
        }
        tmp->Push("</ol>\n");
    }

    if (buf->m_document->img)
    {
        tmp->Push("<hr><h2>Images</h2>\n<ol>\n");
        auto &al = buf->m_document->img;
        for (int i = 0; i < al.size(); i++)
        {
            auto a = al.anchors[i];
            if (a->slave)
                continue;
            auto pu = URL::Parse(a->url, &buf->url);
            auto p = pu.ToStr()->ptr;
            auto u = html_quote(p);
            if (w3mApp::Instance().DecodeURL)
                p = html_quote(url_unquote_conv(p, buf->m_document->document_charset));
            else
                p = u;
            const char *t;
            if (a->title.size() && a->title[0])
                t = html_quote(a->title.c_str());
            else if (w3mApp::Instance().DecodeURL)
                t = html_quote(url_unquote_conv(a->url.c_str(), buf->m_document->document_charset));
            else
                t = html_quote(a->url.c_str());
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                           "\n", NULL);
            a = buf->m_document->formitem.RetrieveAnchor(a->start);
            if (!a)
                continue;
            auto fi = a->item->parent.lock()->item();
            if (fi->parent.lock()->method == FORM_METHOD_INTERNAL &&
                fi->parent.lock()->action == "map" && fi->value.size())
            {
                MapListPtr ml = searchMapList(buf, fi->value.c_str());
                MapAreaPtr m;
                if (!ml)
                    continue;
                tmp->Push("<br>\n<b>Image map</b>\n<ol>\n");
                for (auto it = ml->area.begin(); it != ml->area.end(); ++it)
                {
                    auto m = *it;
                    auto pu = URL::Parse(m->url, &buf->url);
                    p = pu.ToStr()->ptr;
                    u = html_quote(p);
                    if (w3mApp::Instance().DecodeURL)
                        p = html_quote(url_unquote_conv(p,
                                                        buf->m_document->document_charset));
                    else
                        p = u;
                    if (m->alt.size())
                        t = html_quote(m->alt.c_str());
                    else if (w3mApp::Instance().DecodeURL)
                        t = html_quote(url_unquote_conv(m->url,
                                                        buf->m_document->document_charset));
                    else
                        t = html_quote(m->url.c_str());
                    Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t,
                                   "</a><br>", p, "\n", NULL);
                }
                tmp->Push("</ol>\n");
            }
        }
        tmp->Push("</ol>\n");
    }

    return loadHTMLStream(URL::Parse(std::string("w3m://links/") + buf->url.ToStr()->ptr), StrStream::Create(tmp->ptr), WC_CES_UTF_8, true);
}
