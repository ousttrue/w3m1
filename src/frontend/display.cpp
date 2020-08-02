/* $Id: display.c,v 1.71 2010/07/18 14:10:09 htrb Exp $ */

#include "fm.h"
#include "indep.h"
#include "public.h"
#include "html/image.h"
#include "file.h"
#include "symbol.h"
#include "html/maparea.h"
#include "frontend/terms.h"
#include "frontend/line.h"
#include "frontend/display.h"
#include "frontend/mouse.h"
#include "frontend/buffer.h"
#include "frontend/tab.h"
#include "frontend/tabbar.h"
#include "ctrlcode.h"
#include "html/anchor.h"
#include "mime/mimetypes.h"
#include "wtf.h"
#include "w3m.h"
#include <string_view>
#include <signal.h>
#include <math.h>
#include <assert.h>

/*-
 * color: 
 *     0  black 
 *     1  red 
 *     2  green 
 *     3  yellow
 *     4  blue 
 *     5  magenta 
 *     6  cyan 
 *     7  white 
 */
static void effect_anchor_start()
{
    if (w3mApp::Instance().useColor)
    {
        setfcolor(anchor_color);
    }
    else
    {
        underline();
    }
}
static void effect_anchor_end()
{
    if (w3mApp::Instance().useColor)
    {
        setfcolor(basic_color);
    }
    else
    {
        underlineend();
    }
}

static void effect_image_start()
{
    if (w3mApp::Instance().useColor)
    {
        setfcolor(image_color);
    }
    else
    {
        standout();
    }
}
static void effect_image_end()
{
    if (w3mApp::Instance().useColor)
    {
        setfcolor(basic_color);
    }
    else
    {
        standend();
    }
}

static void effect_from_start()
{
    if (w3mApp::Instance().useColor)
    {
        setfcolor(form_color);
    }
    else
    {
        standout();
    }
}
static void effect_form_end()
{
    if (w3mApp::Instance().useColor)
    {
        setfcolor(basic_color);
    }
    else
    {
        standend();
    }
}

static void effect_mark_start()
{
    if (w3mApp::Instance().useColor)
    {
        setbcolor(mark_color);
    }
    else
    {
        standout();
    }
}
static void effect_mark_end()
{
    if (w3mApp::Instance().useColor)
    {
        setbcolor(bg_color);
    }
    else
    {
        standend();
    }
}

/*****************/
static void effect_active_start()
{
    if (w3mApp::Instance().useColor)
    {
        if (useActiveColor)
        {
            setfcolor(active_color), underline();
        }
        else
        {
            underline();
        }
    }
    else
    {
        bold();
    }
}

static void
effect_active_end()
{
    if (w3mApp::Instance().useColor)
    {
        if (useActiveColor)
        {
            (setfcolor(basic_color), underlineend());
        }
        else
        {
            underlineend();
        }
    }
    else
    {
        boldend();
    }
}

static void
effect_visited_start()
{
    if (useVisitedColor)
    {
        if (w3mApp::Instance().useColor)
        {
            setfcolor(visited_color);
        }
        else
        {
            ;
        }
    }
}

static void effect_visited_end()
{
    if (useVisitedColor)
    {
        if (w3mApp::Instance().useColor)
        {
            setfcolor(basic_color);
        }
        else
        {
            ;
        }
    }
}

void fmTerm(void)
{
    if (fmInitialized)
    {
        move((LINES - 1), 0);
        clrtoeolx();
        refresh();

        if (w3mApp::Instance().activeImage)
            loadImage(NULL, IMG_FLAG_STOP);

        if (w3mApp::Instance().use_mouse)
            mouse_end();

        reset_tty();
        fmInitialized = FALSE;
    }
}

/* 
 * Initialize routine.
 */
void fmInit(void)
{
    if (!fmInitialized)
    {
        initscr();
        term_raw();
        term_noecho();
#ifdef USE_IMAGE
        if (w3mApp::Instance().displayImage)
            initImage();
#endif
    }
    fmInitialized = TRUE;
}

/* 
 * Display some lines.
 */
static Line *cline = NULL;
static int ccolumn = -1;

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;
#ifdef USE_ANSI_COLOR
static Linecolor color_mode = 0;
#endif

#ifdef USE_BUFINFO
static BufferPtr save_current_buf = NULL;
#endif

static char *delayed_msg = NULL;

static void drawAnchorCursor(BufferPtr buf);

static Line *redrawLine(BufferPtr buf, Line *l, int i);
#ifdef USE_IMAGE
static int image_touch = 0;
static int draw_image_flag = FALSE;
static Line *redrawLineImage(BufferPtr buf, Line *l, int i);
#endif
static int redrawLineRegion(BufferPtr buf, Line *l, int i, int bpos, int epos);
static void do_effects(Lineprop m);
#ifdef USE_ANSI_COLOR
static void do_color(Linecolor c);
#endif

static Str
make_lastline_link(BufferPtr buf, std::string_view title, char *url)
{
    Str s = NULL, u;
    URL pu;
    char *p;
    int l = COLS - 1, i;

    if (title.size() && title[0])
    {
        s = Strnew_m_charp("[", title, "]");
        s->Replace([](char &c) {
            if (IS_CNTRL(c) || IS_SPACE(c))
            {
                c = ' ';
            }
        });
        if (url)
            s->Push(" ");
        l -= get_Str_strwidth(s);
        if (l <= 0)
            return s;
    }
    if (!url)
        return s;
    pu.Parse2(url, buf->BaseURL());
    u = pu.ToStr();
    if (DecodeURL)
        u = Strnew(url_unquote_conv(u->c_str(), buf->document_charset));

    Lineprop *pr;
    u = checkType(u, &pr, nullptr);

    if (l <= 4 || l >= get_Str_strwidth(u))
    {
        if (!s)
            return u;
        s->Push(u);
        return s;
    }
    if (!s)
        s = Strnew_size(COLS);
    i = (l - 2) / 2;

    while (i && pr[i] & PC_WCHAR2)
        i--;

    s->Push(u->c_str(), i);
    s->Push("..");
    i = get_Str_strwidth(u) - (COLS - 1 - get_Str_strwidth(s));
#ifdef USE_M17N
    while (i < u->Size() && pr[i] & PC_WCHAR2)
        i++;
#endif
    s->Push(&u->c_str()[i]);
    return s;
}

static Str
make_lastline_message(BufferPtr buf)
{
    Str msg, s = NULL;
    int sl = 0;

    if (displayLink)
    {
        MapArea *a = retrieveCurrentMapArea(buf);
        if (a)
            s = make_lastline_link(buf, a->alt, a->url);
        else
        {
            auto a = retrieveCurrentAnchor(buf);
            std::string p = NULL;
            if (a && a->title.size() && a->title[0])
                p = a->title;
            else
            {
                auto a_img = retrieveCurrentImg(buf);
                if (a_img && a_img->title.size() && a_img->title[0])
                    p = a_img->title;
            }
            if (p.size() || a)
                s = make_lastline_link(buf,
                                       p,
                                       a ? const_cast<char *>(a->url.c_str()) : NULL);
        }
        if (s)
        {
            sl = get_Str_strwidth(s);
            if (sl >= COLS - 3)
                return s;
        }
    }

    if (w3mApp::Instance().use_mouse && GetMouseActionLastlineStr())
        msg = Strnew(GetMouseActionLastlineStr());
    else
        msg = Strnew();
    if (displayLineInfo && buf->LineCount() > 0)
    {
        int cl = buf->currentLine->real_linenumber;
        int ll = buf->lastLine->real_linenumber;
        int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
        msg->Push(Sprintf("%d/%d (%d%%)", cl, ll, r));
    }
    else
        /* FIXME: gettextize? */
        msg->Push("Viewing");
    if (buf->ssl_certificate.size())
        msg->Push("[SSL]");
    msg->Push(" <");
    msg->Push(buf->buffername.c_str());

    if (s)
    {
        int l = COLS - 3 - sl;
        if (get_Str_strwidth(msg) > l)
        {
            const char *p;
            for (p = msg->c_str(); *p; p += get_mclen(p))
            {
                l -= get_mcwidth(p);
                if (l < 0)
                    break;
            }
            l = p - msg->c_str();
            msg->Truncate(l);
        }
        msg->Push("> ");
        msg->Push(s);
    }
    else
    {
        msg->Push(">");
    }
    return msg;
}

static void
drawAnchorCursor0(BufferPtr buf, AnchorList &al,
                  int hseq, int prevhseq,
                  int tline, int eline, int active)
{
    auto l = buf->topLine;
    for (int j = 0; j < al.size(); j++)
    {
        auto an = &al.anchors[j];
        if (an->start.line < tline)
            continue;
        if (an->start.line >= eline)
            return;

        for (;; l = l->next)
        {
            if (l == NULL)
                return;
            if (l->linenumber == an->start.line)
                break;
        }

        if (hseq >= 0 && an->hseq == hseq)
        {
            //
            for (int i = an->start.pos; i < an->end.pos; i++)
            {
                if (l->propBuf[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM))
                {
                    if (active)
                        l->propBuf[i] |= PE_ACTIVE;
                    else
                        l->propBuf[i] &= ~PE_ACTIVE;
                }
            }
            if (active)
                redrawLineRegion(buf, l, l->linenumber - tline + buf->rootY,
                                 an->start.pos, an->end.pos);
        }
        else if (prevhseq >= 0 && an->hseq == prevhseq)
        {
            if (active)
                redrawLineRegion(buf, l, l->linenumber - tline + buf->rootY,
                                 an->start.pos, an->end.pos);
        }
    }
}

static void
drawAnchorCursor(BufferPtr buf)
{
    if (buf->LineCount() == 0)
        return;
    if (!buf->href && !buf->formitem)
        return;

    auto an = retrieveCurrentAnchor(buf);
    if (!an)
        an = retrieveCurrentMap(buf);

    int hseq;
    if (an)
        hseq = an->hseq;
    else
        hseq = -1;

    int tline = buf->topLine->linenumber;
    int eline = tline + buf->LINES;
    int prevhseq = buf->prevhseq;

    if (buf->href)
    {
        drawAnchorCursor0(buf, buf->href, hseq, prevhseq, tline, eline, 1);
        drawAnchorCursor0(buf, buf->href, hseq, -1, tline, eline, 0);
    }
    if (buf->formitem)
    {
        drawAnchorCursor0(buf, buf->formitem, hseq, prevhseq, tline, eline, 1);
        drawAnchorCursor0(buf, buf->formitem, hseq, -1, tline, eline, 0);
    }
    buf->prevhseq = hseq;
}

///
/// term に描画する
///
static void
redrawNLine(BufferPtr buf)
{
    // lines
    {
        int i = 0;
        for (auto l = buf->topLine; i < buf->LINES; i++, l = l->next)
        {
            l = redrawLine(buf, l, i + buf->rootY);
            if (l == NULL)
                break;
        }
        move(i + buf->rootY, 0);
        clrtobotx();
    }

    if (!(w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && buf->img))
        return;

    move(buf->cursorY + buf->rootY, buf->cursorX + buf->rootX);
    {
        int i = 0;
        for (auto l = buf->topLine; i < buf->LINES && l; i++, l = l->next)
        {
            redrawLineImage(buf, l, i + buf->rootY);
        }
    }
    getAllImage(buf);
}

static Line *
redrawLine(BufferPtr buf, Line *l, int i)
{
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char *p;
    Lineprop *pr;
    Linecolor *pc;
    URL url;
    int k, vpos = -1;

    if (l == NULL)
    {
        if (buf->pagerSource)
        {
            l = getNextPage(buf, buf->LINES + buf->rootY - i);
            if (l == NULL)
                return NULL;
        }
        else
            return NULL;
    }
    move(i, 0);
    if (w3mApp::Instance().showLineNum)
    {
        char tmp[16];
        if (!buf->rootX)
        {
            if (buf->lastLine->real_linenumber > 0)
                buf->rootX = (int)(log(buf->lastLine->real_linenumber + 0.1) / log(10)) + 2;
            if (buf->rootX < 5)
                buf->rootX = 5;
            if (buf->rootX > COLS)
                buf->rootX = COLS;
            buf->COLS = COLS - buf->rootX;
        }
        if (l->real_linenumber && !l->bpos)
            sprintf(tmp, "%*ld:", buf->rootX - 1, l->real_linenumber);
        else
            sprintf(tmp, "%*s ", buf->rootX - 1, "");
        addstr(tmp);
    }
    move(i, buf->rootX);
    if (l->width < 0)
        l->CalcWidth();
    if (l->len == 0 || l->width - 1 < column)
    {
        clrtoeolx();
        return l;
    }
    /* need_clrtoeol(); */
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
    if (w3mApp::Instance().useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;

    rcol = l->COLPOS(pos);

    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta)
    {
#ifdef USE_COLOR
        if (useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED))
        {
            auto a = buf->href.RetrieveAnchor(l->linenumber, pos + j);
            if (a)
            {
                url.Parse2(a->url, buf->BaseURL());
                if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->c_str()))
                {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
#endif
#ifdef USE_M17N
        delta = wtf_len((uint8_t *)&p[j]);
#endif
        ncol = l->COLPOS(pos + j + delta);
        if (ncol - column > buf->COLS)
            break;
#ifdef USE_ANSI_COLOR
        if (pc)
            do_color(pc[j]);
#endif
        if (rcol < column)
        {
            for (rcol = column; rcol < ncol; rcol++)
                addChar(' ', 0);
            continue;
        }
        if (p[j] == '\t')
        {
            for (; rcol < ncol; rcol++)
                addChar(' ', 0);
        }
        else
        {
#ifdef USE_M17N
            addMChar(&p[j], pr[j], delta);
#else
            addChar(p[j], pr[j]);
#endif
        }
        rcol = ncol;
    }
    if (somode)
    {
        somode = FALSE;
        standend();
    }
    if (ulmode)
    {
        ulmode = FALSE;
        underlineend();
    }
    if (bomode)
    {
        bomode = FALSE;
        boldend();
    }
    if (emph_mode)
    {
        emph_mode = FALSE;
        boldend();
    }

    if (anch_mode)
    {
        anch_mode = FALSE;
        effect_anchor_end();
    }
    if (imag_mode)
    {
        imag_mode = FALSE;
        effect_image_end();
    }
    if (form_mode)
    {
        form_mode = FALSE;
        effect_form_end();
    }
    if (visited_mode)
    {
        visited_mode = FALSE;
        effect_visited_end();
    }
    if (active_mode)
    {
        active_mode = FALSE;
        effect_active_end();
    }
    if (mark_mode)
    {
        mark_mode = FALSE;
        effect_mark_end();
    }
    if (graph_mode)
    {
        graph_mode = FALSE;
        graphend();
    }
#ifdef USE_ANSI_COLOR
    if (color_mode)
        do_color(0);
#endif
    if (rcol - column < buf->COLS)
        clrtoeolx();
    return l;
}

#ifdef USE_IMAGE
static Line *
redrawLineImage(BufferPtr buf, Line *l, int i)
{
    int j, pos, rcol;
    int column = buf->currentColumn;
    int x, y, sx, sy, w, h;

    if (l == NULL)
        return NULL;
    if (l->width < 0)
        l->CalcWidth();
    if (l->len == 0 || l->width - 1 < column)
        return l;
    pos = columnPos(l, column);
    rcol = l->COLPOS(pos);
    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j++)
    {
        if (rcol - column < 0)
        {
            rcol = l->COLPOS(pos + j + 1);
            continue;
        }
        auto a = buf->img.RetrieveAnchor(l->linenumber, pos + j);
        if (a && a->image && a->image->touch < image_touch)
        {
            Image *image = a->image;
            auto cache = image->cache = getImage(image, buf->BaseURL(), buf->image_flag);
            if (cache)
            {
                if ((image->width < 0 && cache->width > 0) ||
                    (image->height < 0 && cache->height > 0))
                {
                    image->width = cache->width;
                    image->height = cache->height;
                    buf->need_reshape = TRUE;
                }
                x = (int)((rcol - column + buf->rootX) * w3mApp::Instance().pixel_per_char);
                y = (int)(i * w3mApp::Instance().pixel_per_line);
                sx = (int)((rcol - l->COLPOS(a->start.pos)) * w3mApp::Instance().pixel_per_char);
                sy = (int)((l->linenumber - image->y) * w3mApp::Instance().pixel_per_line);
                if (sx == 0 && x + image->xoffset >= 0)
                    x += image->xoffset;
                else
                    sx -= image->xoffset;
                if (sy == 0 && y + image->yoffset >= 0)
                    y += image->yoffset;
                else
                    sy -= image->yoffset;
                if (image->width > 0)
                    w = image->width - sx;
                else
                    w = (int)(8 * w3mApp::Instance().pixel_per_char - sx);
                if (image->height > 0)
                    h = image->height - sy;
                else
                    h = (int)(w3mApp::Instance().pixel_per_line - sy);
                if (w > (int)((buf->rootX + buf->COLS) * w3mApp::Instance().pixel_per_char - x))
                    w = (int)((buf->rootX + buf->COLS) * w3mApp::Instance().pixel_per_char - x);
                if (h > (int)((LINES - 1) * w3mApp::Instance().pixel_per_line - y))
                    h = (int)((LINES - 1) * w3mApp::Instance().pixel_per_line - y);
                addImage(cache, x, y, sx, sy, w, h);
                image->touch = image_touch;
                draw_image_flag = TRUE;
            }
        }
        rcol = l->COLPOS(pos + j + 1);
    }
    return l;
}
#endif

static int
redrawLineRegion(BufferPtr buf, Line *l, int i, int bpos, int epos)
{
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char *p;
    Lineprop *pr;
    Linecolor *pc;
    int bcol, ecol;
    URL url;
    int k, vpos = -1;

    if (l == NULL)
        return 0;
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
#ifdef USE_ANSI_COLOR
    if (w3mApp::Instance().useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;
#endif
    rcol = l->COLPOS(pos);
    bcol = bpos - pos;
    ecol = epos - pos;

    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta)
    {
#ifdef USE_COLOR
        if (useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED))
        {
            auto a = buf->href.RetrieveAnchor(l->linenumber, pos + j);
            if (a)
            {
                url.Parse2(a->url, buf->BaseURL());
                if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->c_str()))
                {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
#endif
#ifdef USE_M17N
        delta = wtf_len((uint8_t *)&p[j]);
#endif
        ncol = l->COLPOS(pos + j + delta);
        if (ncol - column > buf->COLS)
            break;
#ifdef USE_ANSI_COLOR
        if (pc)
            do_color(pc[j]);
#endif
        if (j >= bcol && j < ecol)
        {
            if (rcol < column)
            {
                move(i, buf->rootX);
                for (rcol = column; rcol < ncol; rcol++)
                    addChar(' ', 0);
                continue;
            }
            move(i, rcol - column + buf->rootX);
            if (p[j] == '\t')
            {
                for (; rcol < ncol; rcol++)
                    addChar(' ', 0);
            }
            else
#ifdef USE_M17N
                addMChar(&p[j], pr[j], delta);
#else
                addChar(p[j], pr[j]);
#endif
        }
        rcol = ncol;
    }
    if (somode)
    {
        somode = FALSE;
        standend();
    }
    if (ulmode)
    {
        ulmode = FALSE;
        underlineend();
    }
    if (bomode)
    {
        bomode = FALSE;
        boldend();
    }
    if (emph_mode)
    {
        emph_mode = FALSE;
        boldend();
    }

    if (anch_mode)
    {
        anch_mode = FALSE;
        effect_anchor_end();
    }
    if (imag_mode)
    {
        imag_mode = FALSE;
        effect_image_end();
    }
    if (form_mode)
    {
        form_mode = FALSE;
        effect_form_end();
    }
    if (visited_mode)
    {
        visited_mode = FALSE;
        effect_visited_end();
    }
    if (active_mode)
    {
        active_mode = FALSE;
        effect_active_end();
    }
    if (mark_mode)
    {
        mark_mode = FALSE;
        effect_mark_end();
    }
    if (graph_mode)
    {
        graph_mode = FALSE;
        graphend();
    }
#ifdef USE_ANSI_COLOR
    if (color_mode)
        do_color(0);
#endif
    return rcol - column;
}

#define do_effect1(effect, modeflag, action_start, action_end) \
    if (m & effect)                                            \
    {                                                          \
        if (!modeflag)                                         \
        {                                                      \
            action_start;                                      \
            modeflag = TRUE;                                   \
        }                                                      \
    }

#define do_effect2(effect, modeflag, action_start, action_end) \
    if (modeflag)                                              \
    {                                                          \
        action_end;                                            \
        modeflag = FALSE;                                      \
    }

static void
do_effects(Lineprop m)
{
    /* effect end */
    do_effect2(PE_UNDER, ulmode, underline(), underlineend());
    do_effect2(PE_STAND, somode, standout(), standend());
    do_effect2(PE_BOLD, bomode, bold(), boldend());
    do_effect2(PE_EMPH, emph_mode, bold(), boldend());
    do_effect2(PE_ANCHOR, anch_mode, effect_anchor_start(), effect_anchor_end());
    do_effect2(PE_IMAGE, imag_mode, effect_image_start(), effect_image_end());
    do_effect2(PE_FORM, form_mode, effect_from_start(), effect_form_end());
    do_effect2(PE_VISITED, visited_mode, effect_visited_start(), effect_visited_end());
    do_effect2(PE_ACTIVE, active_mode, effect_active_start(), effect_active_end());
    do_effect2(PE_MARK, mark_mode, effect_mark_start(), effect_mark_end());
    if (graph_mode)
    {
        graphend();
        graph_mode = FALSE;
    }

    /* effect start */
    do_effect1(PE_UNDER, ulmode, underline(), underlineend());
    do_effect1(PE_STAND, somode, standout(), standend());
    do_effect1(PE_BOLD, bomode, bold(), boldend());
    do_effect1(PE_EMPH, emph_mode, bold(), boldend());
    do_effect1(PE_ANCHOR, anch_mode, effect_anchor_start(), effect_anchor_end());
    do_effect1(PE_IMAGE, imag_mode, effect_image_start(), effect_image_end());
    do_effect1(PE_FORM, form_mode, effect_from_start(), effect_form_end());
    do_effect1(PE_VISITED, visited_mode, effect_visited_start(),
               EFFECT_VISITED_END);
    do_effect1(PE_ACTIVE, active_mode, effect_active_start(), effect_active_end());
    do_effect1(PE_MARK, mark_mode, effect_mark_start(), effect_mark_end());
}

#ifdef USE_ANSI_COLOR
static void
do_color(Linecolor c)
{
    if (c & 0x8)
        setfcolor(c & 0x7);
    else if (color_mode & 0x8)
        setfcolor(basic_color);
#ifdef USE_BG_COLOR
    if (c & 0x80)
        setbcolor((c >> 4) & 0x7);
    else if (color_mode & 0x80)
        setbcolor(bg_color);
#endif
    color_mode = c;
}
#endif

#ifdef USE_M17N
void addChar(char c, Lineprop mode)
{
    addMChar(&c, mode, 1);
}

void addMChar(char *p, Lineprop mode, size_t len)
#else
void addChar(char c, Lineprop mode)
#endif
{
    Lineprop m = CharEffect(mode);
#ifdef USE_M17N
    char c = *p;

    if (mode & PC_WCHAR2)
        return;
#endif
    do_effects(m);
    if (mode & PC_SYMBOL)
    {
        const char **symbol;
#ifdef USE_M17N
        int w = (mode & PC_KANJI) ? 2 : 1;

        c = ((char)wtf_get_code((uint8_t *)p) & 0x7f) - SYMBOL_BASE;
#else
        c -= SYMBOL_BASE;
#endif
        if (graph_ok() && c < N_GRAPH_SYMBOL)
        {
            if (!graph_mode)
            {
                graphstart();
                graph_mode = TRUE;
            }
#ifdef USE_M17N
            if (w == 2 && WcOption.use_wide)
                addstr(graph2_symbol[(int)c]);
            else
#endif
                addch(*graph_symbol[(int)c]);
        }
        else
        {
#ifdef USE_M17N
            symbol = get_symbol(w3mApp::Instance().DisplayCharset, &w);
            addstr(symbol[(int)c]);
#else
            symbol = get_symbol();
            addch(*symbol[(int)c]);
#endif
        }
    }
    else if (mode & PC_CTRL)
    {
        switch (c)
        {
        case '\t':
            addch(c);
            break;
        case '\n':
            addch(' ');
            break;
        case '\r':
            break;
        case DEL_CODE:
            addstr("^?");
            break;
        default:
            addch('^');
            addch(c + '@');
            break;
        }
    }
#ifdef USE_M17N
    else if (mode & PC_UNKNOWN)
    {
        char buf[5];
        sprintf(buf, "[%.2X]",
                (unsigned char)wtf_get_code((uint8_t *)p) | 0x80);
        addstr(buf);
    }
    else
        addmch(p, len);
#else
    else if (0x80 <= (unsigned char)c && (unsigned char)c <= NBSP_CODE)
        addch(' ');
    else
        addch(c);
#endif
}

static GeneralList *message_list = NULL;

void record_err_message(const char *s)
{
    if (fmInitialized)
    {
        if (!message_list)
            message_list = newGeneralList();
        if (message_list->nitem >= LINES)
            popValue(message_list);
        pushValue(message_list, allocStr(s, -1));
    }
}

/* 
 * List of error messages
 */
BufferPtr
message_list_panel(void)
{
    Str tmp = Strnew_size(LINES * COLS);
    ListItem *p;

    /* FIXME: gettextize? */
    tmp->Push("<html><head><title>List of error messages</title></head><body>"
              "<h1>List of error messages</h1><table cellpadding=0>\n");
    if (message_list)
        for (p = message_list->last; p; p = p->prev)
            Strcat_m_charp(tmp, "<tr><td><pre>", html_quote((const char *)p->ptr),
                           "</pre></td></tr>\n", NULL);
    else
        tmp->Push("<tr><td>(no message recorded)</td></tr>\n");
    tmp->Push("</table></body></html>");
    return loadHTMLString(tmp);
}

void message(const char *s, int return_x, int return_y)
{
    if (!fmInitialized)
        return;
    move((LINES - 1), 0);
    addnstr(s, COLS - 1);
    clrtoeolx();
    move(return_y, return_x);
}

void disp_err_message(const char *s, int redraw_current)
{
    record_err_message(s);
    disp_message(s, redraw_current);
}

void disp_message_nsec(const char *s, int redraw_current, int sec, int purge, int mouse)
{
    if (QuietMessage)
        return;
    if (!fmInitialized)
    {
        fprintf(stderr, "%s\n", conv_to_system(s));
        return;
    }
    if (GetCurrentTab() != NULL && GetCurrentTab()->GetCurrentBuffer() != NULL)
        message(s, GetCurrentTab()->GetCurrentBuffer()->cursorX + GetCurrentTab()->GetCurrentBuffer()->rootX,
                GetCurrentTab()->GetCurrentBuffer()->cursorY + GetCurrentTab()->GetCurrentBuffer()->rootY);
    else
        message(s, (LINES - 1), 0);
    refresh();
#ifdef w3mApp::Instance().use_mouse
    if (mouse && w3mApp::Instance().use_mouse)
        mouse_active();
#endif
    sleep_till_anykey(sec, purge);
#ifdef w3mApp::Instance().use_mouse
    if (mouse && w3mApp::Instance().use_mouse)
        mouse_inactive();
#endif
    if (GetCurrentTab() != NULL && GetCurrentTab()->GetCurrentBuffer() != NULL && redraw_current)
        displayCurrentbuf(B_NORMAL);
}

void disp_message(const char *s, int redraw_current)
{
    disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
}

void disp_message_nomouse(const char *s, int redraw_current)
{
    disp_message_nsec(s, redraw_current, 10, FALSE, FALSE);
}

void set_delayed_message(const char *s)
{
    delayed_msg = allocStr(s, -1);
}



void restorePosition(BufferPtr buf, BufferPtr orig)
{
    buf->LineSkip(buf->firstLine, TOP_LINENUMBER(orig) - 1,
                  FALSE);
    buf->GotoLine(CUR_LINENUMBER(orig));
    buf->pos = orig->pos;
    if (buf->currentLine && orig->currentLine)
        buf->pos += orig->currentLine->bpos - buf->currentLine->bpos;
    buf->currentColumn = orig->currentColumn;
    buf->ArrangeCursor();
}

void displayBuffer(BufferPtr buf, DisplayMode mode)
{
    Str msg;
    int ny = 0;

    if (!buf)
        return;
    if (buf->topLine == NULL && buf->ReadBufferCache() == 0)
    { /* clear_buffer */
        mode = B_FORCE_REDRAW;
    }

    if (buf->width == 0)
        buf->width = INIT_BUFFER_WIDTH();
    if (buf->height == 0)
        buf->height = (LINES - 1) + 1;
    if ((buf->width != INIT_BUFFER_WIDTH() &&
         (is_html_type(buf->type) || w3mApp::Instance().FoldLine)) ||
        buf->need_reshape)
    {
        buf->need_reshape = TRUE;
        buf->Reshape();
    }

    if (w3mApp::Instance().showLineNum)
    {
        if (buf->lastLine && buf->lastLine->real_linenumber > 0)
            buf->rootX = (int)(log(buf->lastLine->real_linenumber + 0.1) / log(10)) + 2;
        if (buf->rootX < 5)
            buf->rootX = 5;
        if (buf->rootX > COLS)
            buf->rootX = COLS;
    }
    else
    {
        buf->rootX = 0;
    }

    buf->COLS = COLS - buf->rootX;
    if (GetTabCount() > 1 || GetMouseActionMenuStr())
    {
        if (mode == B_FORCE_REDRAW || mode == B_REDRAW_IMAGE)
            calcTabPos();
        ny = GetTabbarHeight() + 1;
        if (ny > (LINES - 1))
            ny = (LINES - 1);
    }
    if (buf->rootY != ny || buf->LINES != (LINES - 1) - ny)
    {
        buf->rootY = ny;
        buf->LINES = (LINES - 1) - ny;
        buf->ArrangeCursor();
        mode = B_REDRAW_IMAGE;
    }
    if (mode == B_FORCE_REDRAW || mode == B_SCROLL || mode == B_REDRAW_IMAGE ||
        cline != buf->topLine || ccolumn != buf->currentColumn)
    {
        if (w3mApp::Instance().activeImage &&
            (mode == B_REDRAW_IMAGE ||
             cline != buf->topLine || ccolumn != buf->currentColumn))
        {
            if (draw_image_flag)
                clear();
            clearImage();
            loadImage(buf, IMG_FLAG_STOP);
            image_touch++;
            draw_image_flag = FALSE;
        }

        if (w3mApp::Instance().useColor)
        {
            setfcolor(basic_color);
            setbcolor(bg_color);
        }

        // TAB
        if (GetTabCount() > 1 || GetMouseActionMenuStr())
        {
            move(0, 0);

            if (GetMouseActionMenuStr())
                addstr(GetMouseActionMenuStr());

            clrtoeolx();
            EachTab([](auto t) {
                auto b = t->GetCurrentBuffer();
                move(t->Y(), t->Left());
                if (t == GetCurrentTab())
                    bold();
                addch('[');
                auto l = t->Width() - get_strwidth(b->buffername.c_str());
                if (l < 0)
                    l = 0;
                if (l / 2 > 0)
                    addnstr_sup(" ", l / 2);
                if (t == GetCurrentTab())
                    effect_active_start();
                addnstr(b->buffername.c_str(), t->Width());
                if (t == GetCurrentTab())
                    effect_active_end();
                if ((l + 1) / 2 > 0)
                    addnstr_sup(" ", (l + 1) / 2);
                move(t->Y(), t->Right());
                addch(']');
                if (t == GetCurrentTab())
                    boldend();
            });
            move(GetTabbarHeight(), 0);
            for (int i = 0; i < COLS; i++)
                addch('~');
        }

        // draw
        redrawNLine(buf);

        cline = buf->topLine;
        ccolumn = buf->currentColumn;
    }
    if (buf->topLine == NULL)
    {
        buf->topLine = buf->firstLine;
    }

    if (buf->need_reshape)
    {
        displayBuffer(buf, B_FORCE_REDRAW);
        return;
    }

    drawAnchorCursor(buf);

    msg = make_lastline_message(buf);
    if (buf->LineCount() == 0)
    {
        /* FIXME: gettextize? */
        msg->Push("\tNo Line");
    }
    if (delayed_msg != NULL)
    {
        disp_message(delayed_msg, FALSE);
        delayed_msg = NULL;
        refresh();
    }
    standout();
    message(msg->c_str(), buf->cursorX + buf->rootX, buf->cursorY + buf->rootY);
    standend();
    term_title(conv_to_system(buf->buffername.c_str()));
    refresh();
#ifdef USE_IMAGE
    if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && buf->img)
    {
        drawImage();
    }
#endif
#ifdef USE_BUFINFO
    if (buf != save_current_buf)
    {
        saveBufferInfo();
        save_current_buf = buf;
    }
#endif
}

void displayCurrentbuf(DisplayMode mode)
{
    auto tab = GetCurrentTab();
    if (tab)
    {
        displayBuffer(tab->GetCurrentBuffer(), mode);
    }
    else
    {
        assert(false);
    }
}
