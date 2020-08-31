#include "maparea.h"
#include "indep.h"

#include "file.h"
#include "ctrlcode.h"
#include "textlist.h"
#include "frontend/menu.h"
#include "frontend/tabbar.h"
#include "html/image.h"
#include "html/html_context.h"
#include <math.h>

MapListPtr searchMapList(BufferPtr buf, const char *name)
{
    if (name == NULL)
        return NULL;

    for (auto &ml : buf->maplist)
    {
        if (ml->name == name)
        {
            return ml;
        }
    }
    return nullptr;
}

static int
inMapArea(MapAreaPtr a, int x, int y)
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
        for (t = 0, i = 0; i < a->coords.size(); i += 2)
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
nearestMapArea(MapListPtr ml, int x, int y)
{
    int i, l, n = -1, min = -1, limit = ImageManager::Instance().pixel_per_char * ImageManager::Instance().pixel_per_char + ImageManager::Instance().pixel_per_line * ImageManager::Instance().pixel_per_line;

    if (!ml)
        return n;
    i = 0;
    for (auto al = ml->area.begin(); al != ml->area.end(); ++al)
    {
        auto a = *al;
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
searchMapArea(BufferPtr buf, MapListPtr ml, const AnchorPtr a_img)
{
    int i, n;
    int px, py;

    if (!(ml && ml->area.size()))
        return -1;
    if (!getMapXY(buf, a_img, &px, &py))
        return -1;
    n = -ml->area.size();
    i = 0;
    for (auto al = ml->area.begin(); al != ml->area.end(); i++, ++al)
    {
        auto a = *al;
        if (!a)
            continue;
        if (n < 0 && inMapArea(a, px, py))
        {
            if (a->shape == SHAPE_DEFAULT)
            {
                if (n == -ml->area.size())
                    n = -i;
            }
            else
                n = i;
        }
    }
    if (n == -ml->area.size())
        return nearestMapArea(ml, px, py);
    else if (n < 0)
        return -n;
    return n;
}

MapAreaPtr
retrieveCurrentMapArea(const BufferPtr &buf)
{
    MapListPtr ml;
    int i, n;

    auto a_img = buf->m_document->img.RetrieveAnchor(buf->CurrentPoint());
    if (!(a_img && a_img->image && a_img->image->map))
        return NULL;
    auto a_form = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());
    if (!(a_form && a_form->url.size()))
        return NULL;
    auto fi = a_form->item;
    if (!(fi && fi->parent.lock() && fi->parent.lock()->item()))
        return NULL;
    fi = fi->parent.lock()->item();
    ml = searchMapList(buf, fi->value.c_str());
    if (!ml)
        return NULL;
    n = searchMapArea(buf, ml, a_img);
    if (n < 0)
        return NULL;
    i = 0;
    for (auto al = ml->area.begin(); al != ml->area.end(); i++, ++al)
    {
        if (i == n)
            return *al;
    }
    return NULL;
}

int getMapXY(BufferPtr buf, const AnchorPtr a, int *x, int *y)
{
    if (!buf || !a || !a->image || !x || !y)
        return 0;
    *x = (int)((buf->currentColumn + buf->rect.cursorX - buf->CurrentLine()->COLPOS(a->start.pos) + 0.5) * ImageManager::Instance().pixel_per_char) - a->image->xoffset;
    *y = (int)((buf->CurrentLine()->linenumber - a->image->y + 0.5) * ImageManager::Instance().pixel_per_line) - a->image->yoffset;
    if (*x <= 0)
        *x = 1;
    if (*y <= 0)
        *y = 1;
    return 1;
}

AnchorPtr retrieveCurrentMap(const BufferPtr &buf)
{
    auto a = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());
    if (!a || !a->url.size())
        return NULL;
    auto fi = a->item;
    if (fi->parent.lock()->method == FORM_METHOD_INTERNAL && fi->parent.lock()->action == "map")
        return a;
    return NULL;
}

MapAreaPtr follow_map_menu(BufferPtr buf, const char *name, const AnchorPtr &a_img, int x, int y)
{
    auto ml = searchMapList(buf, name);
    if (ml == NULL || ml->area.empty())
        return NULL;

    auto initial = searchMapArea(buf, ml, a_img);
    int selected = -1;
    bool use_label = true;
    if (initial < 0)
    {
        initial = 0;
    }
    else if (!ImageManager::Instance().image_map_list)
    {
        selected = initial;
        use_label = false;
    }

    if (use_label)
    {
        std::vector<std::string> label;
        int i = 0;
        for (auto al = ml->area.begin(); al != ml->area.end(); i++, ++al)
        {
            auto a = *al;
            if (a)
                label.push_back(a->alt.size() ? a->alt : a->url);
            else
                label.push_back("");
        }

        optionMenu(x, y, label, &selected, initial, NULL);
    }

    if (selected >= 0)
    {
        int i = 0;
        for (auto al = ml->area.begin(); al != ml->area.end(); i++, ++al)
        {
            if (i == selected)
                return *al;
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
    MapListPtr ml;
    ListItem *al;
    MapAreaPtr a;
    URL pu;
    char *p, *q;
    BufferPtr newBuf;

    ml = searchMapList(buf, name);
    if (ml == NULL)
        return NULL;

    mappage = Strnew(map1);
    for (al = ml->area->first; al != NULL; al = al->next)
    {
        a = (MapAreaPtr)al->ptr;
        if (!a)
            continue;
        parseURL2(a->url, &pu, &buf->currentURL);
        p = pu.ToStr()->ptr;
        q = html_quote(p);
        if (DecodeURL)
            p = html_quote(url_unquote_conv(p, buf->m_document->document_charset));
        else
            p = q;
        Strcat_m_charp(mappage, "<tr valign=top><td><a href=\"", q, "\">",
                       html_quote(*a->alt ? a->alt : mybasename(a->url)),
                       "</a><td>", p, NULL);
    }
    mappage->Push("</table></body></html>");

    newBuf = loadHTMLString(mappage);
#ifdef USE_M17N
    if (newBuf)
        newBuf->m_document->document_charset = buf->m_document->document_charset;
#endif
    return newBuf;
}
#endif

MapAreaPtr newMapArea(const char *url, const char *target, const char *alt, const char *shape, const char *coords)
{
    auto a = std::make_shared<MapArea>();
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
        a->coords.resize(4);
    }
    else if (a->shape == SHAPE_CIRCLE)
    {
        a->coords.resize(3);
    }

    auto p = coords;
    for (; *p;)
    {
        while (IS_SPACE(*p))
            p++;
        if (!IS_DIGIT(*p) && *p != '-' && *p != '+')
            break;
        a->coords.push_back((short)atoi(p));
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
    if (a->shape == SHAPE_CIRCLE)
    {
        a->center_x = a->coords[0];
        a->center_y = a->coords[1];
    }
    else
    {
        for (int i = 0; i < a->coords.size() / 2; i++)
        {
            a->center_x += a->coords[2 * i];
            a->center_y += a->coords[2 * i + 1];
        }
        a->center_x /= a->coords.size() / 2;
        a->center_y /= a->coords.size() / 2;
    }

    return a;
}

/* append image map links */
static void
append_map_info(BufferPtr buf, Str tmp, FormItemPtr fi)
{
    auto ml = searchMapList(buf, fi->value.c_str());
    if (ml == NULL)
        return;

    Strcat_m_charp(tmp,
                   "<tr valign=top><td colspan=2>Links of current image map",
                   "<tr valign=top><td colspan=2><table>", NULL);
    for (auto al = ml->area.begin(); al != ml->area.end(); ++al)
    {
        auto a = *al;
        if (!a)
            continue;
        auto pu = URL::Parse(a->url, &buf->url);
        auto q = html_quote(pu.ToStr()->ptr);
        char *p;
        if (w3mApp::Instance().DecodeURL)
            p = html_quote(url_unquote_conv(a->url, buf->m_document->document_charset));
        else
            p = html_quote(a->url.c_str());
        Strcat_m_charp(tmp, "<tr valign=top><td>&nbsp;&nbsp;<td><a href=\"",
                       q, "\">",
                       html_quote(a->alt.size() ? a->alt : mybasename(a->url)),
                       "</a><td>", p, "\n", NULL);
    }
    tmp->Push("</table>");
}

/* append links */
static void
append_link_info(BufferPtr buf, Str html)
{
    if (buf->m_document->linklist.empty())
        return;

    html->Push("<hr width=50%><h1>Link information</h1><table>\n");
    for (auto &l : buf->m_document->linklist)
    {
        html->Push(l.toHtml(*&buf->url, buf->m_document->document_charset));
    }
    html->Push("</table>\n");
}

/* 
 * information of current page and link 
 */
BufferPtr
page_info_panel(const BufferPtr &buf)
{
    Str tmp = Strnew_size(1024);
    AnchorPtr a;
    TextListItem *ti;

    char *p, *q;

    wc_ces_list *list;
    char charset[16];

    BufferPtr newBuf;

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

    p = buf->url.ToStr()->ptr;
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

    if (buf->m_document->document_charset != w3mApp::Instance().InnerCharset)
    {
        list = wc_get_ces_list();
        tmp->Push(
            "<tr><td nowrap>Document Charset<td><select name=charset>");
        for (; list->name != NULL; list++)
        {
            sprintf(charset, "%d", (unsigned int)list->id);
            Strcat_m_charp(tmp, "<option value=", charset,
                           (buf->m_document->document_charset == list->id) ? " selected>"
                                                               : ">",
                           list->desc, NULL);
        }
        tmp->Push("</select>");
        tmp->Push("<tr><td><td><input type=submit value=Change>");
    }

    Strcat_m_charp(tmp,
                   "<tr valign=top><td nowrap>Number of lines<td>",
                   Sprintf("%d", all)->ptr,
                //    "<tr valign=top><td nowrap>Transferred bytes<td>",
                //    Sprintf("%d", buf->trbyte)->ptr, 
                   NULL);

    a = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (a != NULL)
    {
        auto pu = URL::Parse(a->url, &buf->url);
        p = pu.ToStr()->ptr;
        q = html_quote(p);
        if (w3mApp::Instance().DecodeURL)
            p = html_quote(url_unquote_conv(p, buf->m_document->document_charset));
        else
            p = q;
        Strcat_m_charp(tmp,
                       "<tr valign=top><td nowrap>URL of current anchor<td><a href=\"",
                       q, "\">", p, "</a>", NULL);
    }
    a = buf->m_document->img.RetrieveAnchor(buf->CurrentPoint());
    if (a != NULL)
    {
        auto pu = URL::Parse(a->url, &buf->url);
        p = pu.ToStr()->ptr;
        q = html_quote(p);
        if (w3mApp::Instance().DecodeURL)
            p = html_quote(url_unquote_conv(p, buf->m_document->document_charset));
        else
            p = q;
        Strcat_m_charp(tmp,
                       "<tr valign=top><td nowrap>URL of current image<td><a href=\"",
                       q, "\">", p, "</a>", NULL);
    }
    a = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());
    if (a != NULL)
    {
        auto fi = a->item;
        p = fi->ToStr()->ptr;
        if (w3mApp::Instance().DecodeURL)
            p = html_quote(url_unquote_conv(p, buf->m_document->document_charset));
        else
            p = html_quote(p);
        Strcat_m_charp(tmp,
                       "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>",
                       p, NULL);
        if (fi->parent.lock()->method == FORM_METHOD_INTERNAL && fi->parent.lock()->action == "map")
            append_map_info(buf, tmp, fi->parent.lock()->item());
    }
    tmp->Push("</table>\n");
    tmp->Push("</form>");

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

    // if (f_set)
    // {
    //     tmp->Push("<hr width=50%><h1>Frame information</h1>\n");
    //     append_frame_info(buf, tmp, f_set, 0);
    // }

    if (buf->ssl_certificate.size())
        Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
                       html_quote(buf->ssl_certificate), "</pre>\n", NULL);

end:
    tmp->Push("</body></html>");
    newBuf = loadHTMLStream(URL::Parse("w3m://pageinfo"), StrStream::Create(tmp->ptr), WC_CES_UTF_8, true);

    if (newBuf)
        newBuf->m_document->document_charset = buf->m_document->document_charset;

    return newBuf;
}
