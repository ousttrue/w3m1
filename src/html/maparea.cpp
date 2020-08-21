#include "maparea.h"

#include "indep.h"
#include "gc_helper.h"
#include "html/frame.h"
#include "file.h"
#include "ctrlcode.h"
#include "charset.h"
#include "textlist.h"
#include "frontend/menu.h"
#include "frontend/tabbar.h"
#include "html/anchor.h"
#include "html/image.h"
#include "html/html_processor.h"
#include <math.h>

MapList *searchMapList(BufferPtr buf, const char *name)
{
    MapList *ml;

    if (name == NULL)
        return NULL;
    for (ml = buf->maplist; ml != NULL; ml = ml->next)
    {
        if (ml->name->Cmp(name) == 0)
            break;
    }
    return ml;
}

enum ShapeTypes
{
    SHAPE_UNKNOWN = 0,
    SHAPE_DEFAULT = 1,
    SHAPE_RECT = 2,
    SHAPE_CIRCLE = 3,
    SHAPE_POLY = 4,
};

static int
inMapArea(MapArea *a, int x, int y)
{
    int i;
    double r1, r2, s, c, t;

    if (!a)
        return false;
    switch (a->shape)
    {
    case SHAPE_RECT:
        if (x >= a->coords[0] && y >= a->coords[1] &&
            x <= a->coords[2] && y <= a->coords[3])
            return true;
        break;
    case SHAPE_CIRCLE:
        if ((x - a->coords[0]) * (x - a->coords[0]) + (y - a->coords[1]) * (y - a->coords[1]) <= a->coords[2] * a->coords[2])
            return true;
        break;
    case SHAPE_POLY:
        for (t = 0, i = 0; i < a->ncoords; i += 2)
        {
            r1 = sqrt((double)(x - a->coords[i]) * (x - a->coords[i]) + (double)(y - a->coords[i + 1]) * (y -
                                                                                                          a->coords[i + 1]));
            r2 = sqrt((double)(x - a->coords[i + 2]) * (x - a->coords[i + 2]) + (double)(y - a->coords[i + 3]) * (y -
                                                                                                                  a->coords[i + 3]));
            if (r1 == 0 || r2 == 0)
                return true;
            s = ((double)(x - a->coords[i]) * (y - a->coords[i + 3]) - (double)(x - a->coords[i + 2]) * (y -
                                                                                                         a->coords[i +
                                                                                                                   1])) /
                r1 / r2;
            c = ((double)(x - a->coords[i]) * (x - a->coords[i + 2]) + (double)(y - a->coords[i + 1]) * (y -
                                                                                                         a->coords[i +
                                                                                                                   3])) /
                r1 / r2;
            t += atan2(s, c);
        }
        if (fabs(t) > 2 * 3.14)
            return true;
        break;
    case SHAPE_DEFAULT:
        return true;
    default:
        break;
    }
    return false;
}

static int
nearestMapArea(MapList *ml, int x, int y)
{
    ListItem *al;
    MapArea *a;
    int i, l, n = -1, min = -1, limit = w3mApp::Instance().pixel_per_char * w3mApp::Instance().pixel_per_char + w3mApp::Instance().pixel_per_line * w3mApp::Instance().pixel_per_line;

    if (!ml || !ml->area)
        return n;
    for (i = 0, al = ml->area->first; al != NULL; i++, al = al->next)
    {
        a = (MapArea *)al->ptr;
        if (a)
        {
            l = (a->center_x - x) * (a->center_x - x) + (a->center_y - y) * (a->center_y - y);
            if ((min < 0 || l < min) && l < limit)
            {
                n = i;
                min = l;
            }
        }
    }
    return n;
}

static int
searchMapArea(BufferPtr buf, MapList *ml, const Anchor *a_img)
{
    ListItem *al;
    MapArea *a;
    int i, n;
    int px, py;

    if (!(ml && ml->area && ml->area->nitem))
        return -1;
    if (!getMapXY(buf, a_img, &px, &py))
        return -1;
    n = -ml->area->nitem;
    for (i = 0, al = ml->area->first; al != NULL; i++, al = al->next)
    {
        a = (MapArea *)al->ptr;
        if (!a)
            continue;
        if (n < 0 && inMapArea(a, px, py))
        {
            if (a->shape == SHAPE_DEFAULT)
            {
                if (n == -ml->area->nitem)
                    n = -i;
            }
            else
                n = i;
        }
    }
    if (n == -ml->area->nitem)
        return nearestMapArea(ml, px, py);
    else if (n < 0)
        return -n;
    return n;
}

MapArea *
retrieveCurrentMapArea(const BufferPtr &buf)
{
    FormItemList *fi;
    MapList *ml;
    ListItem *al;
    MapArea *a;
    int i, n;

    auto a_img = buf->img.RetrieveAnchor(buf->CurrentPoint());
    if (!(a_img && a_img->image && a_img->image->map))
        return NULL;
    auto a_form = buf->formitem.RetrieveAnchor(buf->CurrentPoint());
    if (!(a_form && a_form->url.size()))
        return NULL;
    fi = a_form->item;
    if (!(fi && fi->parent && fi->parent->item))
        return NULL;
    fi = fi->parent->item;
    ml = searchMapList(buf, fi->value.c_str());
    if (!ml)
        return NULL;
    n = searchMapArea(buf, ml, a_img);
    if (n < 0)
        return NULL;
    for (i = 0, al = ml->area->first; al != NULL; i++, al = al->next)
    {
        a = (MapArea *)al->ptr;
        if (a && i == n)
            return a;
    }
    return NULL;
}

int getMapXY(BufferPtr buf, const Anchor *a, int *x, int *y)
{
    if (!buf || !a || !a->image || !x || !y)
        return 0;
    *x = (int)((buf->currentColumn + buf->rect.cursorX - buf->CurrentLine()->COLPOS(a->start.pos) + 0.5) * w3mApp::Instance().pixel_per_char) - a->image->xoffset;
    *y = (int)((buf->CurrentLine()->linenumber - a->image->y + 0.5) * w3mApp::Instance().pixel_per_line) - a->image->yoffset;
    if (*x <= 0)
        *x = 1;
    if (*y <= 0)
        *y = 1;
    return 1;
}

const Anchor *
retrieveCurrentMap(const BufferPtr &buf)
{
    FormItemList *fi;

    auto a = buf->formitem.RetrieveAnchor(buf->CurrentPoint());
    if (!a || !a->url.size())
        return NULL;
    fi = a->item;
    if (fi->parent->method == FORM_METHOD_INTERNAL &&
        fi->parent->action->Cmp("map") == 0)
        return a;
    return NULL;
}

MapArea *follow_map_menu(BufferPtr buf, const char *name, const Anchor *a_img, int x, int y)
{
    auto ml = searchMapList(buf, name);
    if (ml == NULL || ml->area == NULL || ml->area->nitem == 0)
        return NULL;

    auto initial = searchMapArea(buf, ml, a_img);
    int selected = -1;
    bool use_label = true;
    if (initial < 0)
    {
        initial = 0;
    }
    else if (!w3mApp::Instance().image_map_list)
    {
        selected = initial;
        use_label = false;
    }

    if (use_label)
    {
        std::vector<std::string> label;
        int i = 0;
        for (auto al = ml->area->first; al != NULL; i++, al = al->next)
        {
            auto a = (MapArea *)al->ptr;
            if (a)
                label.push_back(*a->alt ? const_cast<char *>(a->alt) : const_cast<char *>(a->url));
            else
                label.push_back("");
        }

        optionMenu(x, y, label, &selected, initial, NULL);
    }

    if (selected >= 0)
    {
        int i = 0;
        for (auto al = ml->area->first; al != NULL; i++, al = al->next)
        {
            if (al->ptr && i == selected)
                return (MapArea *)al->ptr;
        }
    }
    return NULL;
}

#ifndef MENU_MAP
char *map1 = "<HTML><HEAD><TITLE>Image map links</TITLE></HEAD>\
<BODY><H1>Image map links</H1>\
<table>";

BufferPtr
follow_map_panel(BufferPtr buf, char *name)
{
    Str mappage;
    MapList *ml;
    ListItem *al;
    MapArea *a;
    URL pu;
    char *p, *q;
    BufferPtr newbuf;

    ml = searchMapList(buf, name);
    if (ml == NULL)
        return NULL;

    mappage = Strnew(map1);
    for (al = ml->area->first; al != NULL; al = al->next)
    {
        a = (MapArea *)al->ptr;
        if (!a)
            continue;
        parseURL2(a->url, &pu, buf->BaseURL());
        p = pu.ToStr()->ptr;
        q = html_quote(p);
        if (DecodeURL)
            p = html_quote(url_unquote_conv(p, buf->document_charset));
        else
            p = q;
        Strcat_m_charp(mappage, "<tr valign=top><td><a href=\"", q, "\">",
                       html_quote(*a->alt ? a->alt : mybasename(a->url)),
                       "</a><td>", p, NULL);
    }
    mappage->Push("</table></body></html>");

    newbuf = loadHTMLString(mappage);
#ifdef USE_M17N
    if (newbuf)
        newbuf->document_charset = buf->document_charset;
#endif
    return newbuf;
}
#endif

MapArea *
newMapArea(const char *url, const char *target, const char *alt, const char *shape, const char *coords)
{
    MapArea *a = New(MapArea);

    a->url = url;
    a->target = target;
    a->alt = alt ? alt : (char *)"";

    a->shape = SHAPE_RECT;
    if (shape)
    {
        if (!strcasecmp(shape, "default"))
            a->shape = SHAPE_DEFAULT;
        else if (!strncasecmp(shape, "rect", 4))
            a->shape = SHAPE_RECT;
        else if (!strncasecmp(shape, "circ", 4))
            a->shape = SHAPE_CIRCLE;
        else if (!strncasecmp(shape, "poly", 4))
            a->shape = SHAPE_POLY;
        else
            a->shape = SHAPE_UNKNOWN;
    }
    a->coords = NULL;
    a->ncoords = 0;
    a->center_x = 0;
    a->center_y = 0;
    if (a->shape == SHAPE_UNKNOWN || a->shape == SHAPE_DEFAULT)
        return a;
    if (!coords)
    {
        a->shape = SHAPE_UNKNOWN;
        return a;
    }
    if (a->shape == SHAPE_RECT)
    {
        a->coords = New_N(short, 4);
        a->ncoords = 4;
    }
    else if (a->shape == SHAPE_CIRCLE)
    {
        a->coords = New_N(short, 3);
        a->ncoords = 3;
    }

    auto max = a->ncoords;
    auto p = coords;
    int i = 0;
    for (; (a->shape == SHAPE_POLY || i < a->ncoords) && *p;)
    {
        while (IS_SPACE(*p))
            p++;
        if (!IS_DIGIT(*p) && *p != '-' && *p != '+')
            break;
        if (a->shape == SHAPE_POLY)
        {
            if (max <= i)
            {
                max = i ? i * 2 : 6;
                a->coords = New_Reuse(short, a->coords, max + 2);
            }
            a->ncoords++;
        }
        a->coords[i] = (short)atoi(p);
        i++;
        if (*p == '-' || *p == '+')
            p++;
        while (IS_DIGIT(*p))
            p++;
        if (*p != ',' && !IS_SPACE(*p))
            break;
        while (IS_SPACE(*p))
            p++;
        if (*p == ',')
            p++;
    }
    if (i != a->ncoords || (a->shape == SHAPE_POLY && a->ncoords < 6))
    {
        a->shape = SHAPE_UNKNOWN;
        a->coords = NULL;
        a->ncoords = 0;
        return a;
    }
    if (a->shape == SHAPE_POLY)
    {
        a->ncoords = a->ncoords / 2 * 2;
        a->coords[a->ncoords] = a->coords[0];
        a->coords[a->ncoords + 1] = a->coords[1];
    }
    if (a->shape == SHAPE_CIRCLE)
    {
        a->center_x = a->coords[0];
        a->center_y = a->coords[1];
    }
    else
    {
        for (i = 0; i < a->ncoords / 2; i++)
        {
            a->center_x += a->coords[2 * i];
            a->center_y += a->coords[2 * i + 1];
        }
        a->center_x /= a->ncoords / 2;
        a->center_y /= a->ncoords / 2;
    }

    return a;
}

/* append image map links */
static void
append_map_info(BufferPtr buf, Str tmp, FormItemList *fi)
{
    auto ml = searchMapList(buf, fi->value.c_str());
    if (ml == NULL)
        return;

    Strcat_m_charp(tmp,
                   "<tr valign=top><td colspan=2>Links of current image map",
                   "<tr valign=top><td colspan=2><table>", NULL);
    for (auto al = ml->area->first; al != NULL; al = al->next)
    {
        auto a = (MapArea *)al->ptr;
        if (!a)
            continue;
        auto pu = URL::Parse(a->url, buf->BaseURL());
        auto q = html_quote(pu.ToStr()->ptr);
        char *p;
        if (w3mApp::Instance().DecodeURL)
            p = html_quote(url_unquote_conv(a->url, buf->document_charset));
        else
            p = html_quote(a->url);
        Strcat_m_charp(tmp, "<tr valign=top><td>&nbsp;&nbsp;<td><a href=\"",
                       q, "\">",
                       html_quote(*a->alt ? a->alt : mybasename(a->url)),
                       "</a><td>", p, "\n", NULL);
    }
    tmp->Push("</table>");
}

/* append links */
static void
append_link_info(BufferPtr buf, Str html)
{
    if (buf->linklist.empty())
        return;

    html->Push("<hr width=50%><h1>Link information</h1><table>\n");
    for (auto &l : buf->linklist)
    {
        html->Push(l.toHtml(*buf->BaseURL(), buf->document_charset));
    }
    html->Push("</table>\n");
}

/* append frame URL */
static void
append_frame_info(BufferPtr buf, Str html, struct frameset *set, int level)
{
    char *p, *q;
    int i, j;

    if (!set)
        return;

    for (i = 0; i < set->col * set->row; i++)
    {
        union frameset_element frame = set->frame[i];
        if (frame.element != NULL)
        {
            switch (frame.element->attr)
            {
            case F_UNLOADED:
            case F_BODY:
                if (frame.body->url == NULL)
                    break;
                html->Push("<pre_int>");
                for (j = 0; j < level; j++)
                    html->Push("   ");
                q = html_quote(frame.body->url);
                Strcat_m_charp(html, "<a href=\"", q, "\">", NULL);
                if (frame.body->name)
                {
                    p = html_quote(url_unquote_conv(frame.body->name,
                                                    buf->document_charset));
                    html->Push(p);
                }
                if (w3mApp::Instance().DecodeURL)
                    p = html_quote(url_unquote_conv(frame.body->url,
                                                    buf->document_charset));
                else
                    p = q;
                Strcat_m_charp(html, " ", p, "</a></pre_int><br>\n", NULL);
#ifdef USE_SSL
                if (frame.body->ssl_certificate)
                    Strcat_m_charp(html,
                                   "<blockquote><h2>SSL certificate</h2><pre>\n",
                                   html_quote(frame.body->ssl_certificate),
                                   "</pre></blockquote>\n", NULL);
#endif
                break;
            case F_FRAMESET:
                append_frame_info(buf, html, frame.set, level + 1);
                break;
            }
        }
    }
}

/* 
 * information of current page and link 
 */
BufferPtr
page_info_panel(const BufferPtr &buf)
{
    Str tmp = Strnew_size(1024);
    const Anchor *a;
    TextListItem *ti;
    struct frameset *f_set = NULL;
    char *p, *q;

    wc_ces_list *list;
    char charset[16];

    BufferPtr newbuf;

    tmp->Push("<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");

    auto tab = GetCurrentTab();
    int all = 0;
    if (buf == NULL)
        goto end;
    all = buf->LineCount();

    tmp->Push("<form method=internal action=charset>");

    p = buf->currentURL.ToStr()->ptr;
    if (w3mApp::Instance().DecodeURL)
        p = url_unquote_conv(p, WC_CES_NONE);
    Strcat_m_charp(tmp, "<table cellpadding=0>",
                   "<tr valign=top><td nowrap>Title<td>",
                   html_quote(buf->buffername.c_str()),
                   "<tr valign=top><td nowrap>Current URL<td>",
                   html_quote(p),
                   "<tr valign=top><td nowrap>Document Type<td>",
                   buf->real_type.size() ? html_quote(buf->real_type) : "unknown",
                   "<tr valign=top><td nowrap>Last Modified<td>",
                   html_quote(last_modified(buf)), NULL);
#ifdef USE_M17N
    if (buf->document_charset != w3mApp::Instance().InnerCharset)
    {
        list = wc_get_ces_list();
        tmp->Push(
            "<tr><td nowrap>Document Charset<td><select name=charset>");
        for (; list->name != NULL; list++)
        {
            sprintf(charset, "%d", (unsigned int)list->id);
            Strcat_m_charp(tmp, "<option value=", charset,
                           (buf->document_charset == list->id) ? " selected>"
                                                               : ">",
                           list->desc, NULL);
        }
        tmp->Push("</select>");
        tmp->Push("<tr><td><td><input type=submit value=Change>");
    }
#endif
    Strcat_m_charp(tmp,
                   "<tr valign=top><td nowrap>Number of lines<td>",
                   Sprintf("%d", all)->ptr,
                   "<tr valign=top><td nowrap>Transferred bytes<td>",
                   Sprintf("%d", buf->trbyte)->ptr, NULL);

    a = buf->href.RetrieveAnchor(buf->CurrentPoint());
    if (a != NULL)
    {
        auto pu = URL::Parse(a->url, buf->BaseURL());
        p = pu.ToStr()->ptr;
        q = html_quote(p);
        if (w3mApp::Instance().DecodeURL)
            p = html_quote(url_unquote_conv(p, buf->document_charset));
        else
            p = q;
        Strcat_m_charp(tmp,
                       "<tr valign=top><td nowrap>URL of current anchor<td><a href=\"",
                       q, "\">", p, "</a>", NULL);
    }
    a = buf->img.RetrieveAnchor(buf->CurrentPoint());
    if (a != NULL)
    {
        auto pu = URL::Parse(a->url, buf->BaseURL());
        p = pu.ToStr()->ptr;
        q = html_quote(p);
        if (w3mApp::Instance().DecodeURL)
            p = html_quote(url_unquote_conv(p, buf->document_charset));
        else
            p = q;
        Strcat_m_charp(tmp,
                       "<tr valign=top><td nowrap>URL of current image<td><a href=\"",
                       q, "\">", p, "</a>", NULL);
    }
    a = buf->formitem.RetrieveAnchor(buf->CurrentPoint());
    if (a != NULL)
    {
        FormItemList *fi = a->item;
        p = form2str(fi);
        if (w3mApp::Instance().DecodeURL)
            p = html_quote(url_unquote_conv(p, buf->document_charset));
        else
            p = html_quote(p);
        Strcat_m_charp(tmp,
                       "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>",
                       p, NULL);
        if (fi->parent->method == FORM_METHOD_INTERNAL && fi->parent->action->Cmp("map") == 0)
            append_map_info(buf, tmp, fi->parent->item);
    }
    tmp->Push("</table>\n");
#ifdef USE_M17N
    tmp->Push("</form>");
#endif

    append_link_info(buf, tmp);

    // if (buf->document_header != NULL)
    // {
    //     tmp->Push("<hr width=50%><h1>Header information</h1><pre>\n");
    //     for (ti = buf->document_header->first; ti != NULL; ti = ti->next)
    //         Strcat_m_charp(tmp, "<pre_int>", html_quote(ti->ptr),
    //                        "</pre_int>\n", NULL);
    //     tmp->Push("</pre>\n");
    // }

    // TODO:
    // if (buf->frameset != NULL)
    //     f_set = buf->frameset;
    // else if (buf->bufferprop & BP_FRAME &&
    //          tab->BackBuffer(buf) != NULL && tab->BackBuffer(buf)->frameset != NULL)
    //     f_set = tab->BackBuffer(buf)->frameset;

    if (f_set)
    {
        tmp->Push("<hr width=50%><h1>Frame information</h1>\n");
        append_frame_info(buf, tmp, f_set, 0);
    }

    if (buf->ssl_certificate.size())
        Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
                       html_quote(buf->ssl_certificate), "</pre>\n", NULL);

end:
    tmp->Push("</body></html>");
    newbuf = loadHTMLString({}, tmp);
#ifdef USE_M17N
    if (newbuf)
        newbuf->document_charset = buf->document_charset;
#endif
    return newbuf;
}
