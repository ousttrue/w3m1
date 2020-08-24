#include "textlist.h"
#include "ctrlcode.h"
#include "file.h"
#include "frontend/display.h"
#include "frontend/terminal.h"
#include "frontend/screen.h"
#include "frontend/buffer.h"
#include "frontend/line.h"
#include "frontend/mouse.h"
#include "frontend/tab.h"
#include "frontend/tabbar.h"

#include "html/anchor.h"
#include "html/image.h"
#include "html/maparea.h"
#include "html/html_processor.h"
#include "indep.h"
#include "mime/mimetypes.h"
#include "public.h"
#include "symbol.h"
#include "w3m.h"
#include "wtf.h"
#include <assert.h>
#include <math.h>
#include <signal.h>
#include <string_view>

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
        Screen::Instance().SetFGColor(w3mApp::Instance().anchor_color);
    }
    else
    {
        Screen::Instance().Enable(S_UNDERLINE);
    }
}
static void effect_anchor_end()
{
    if (w3mApp::Instance().useColor)
    {
        Screen::Instance().SetFGColor(w3mApp::Instance().basic_color);
    }
    else
    {
        Screen::Instance().Disable(S_UNDERLINE);
    }
}

static void effect_image_start()
{
    if (w3mApp::Instance().useColor)
    {
        Screen::Instance().SetFGColor(w3mApp::Instance().image_color);
    }
    else
    {
        Screen::Instance().Enable(S_STANDOUT);
    }
}
static void effect_image_end()
{
    if (w3mApp::Instance().useColor)
    {
        Screen::Instance().SetFGColor(w3mApp::Instance().basic_color);
    }
    else
    {
        Screen::Instance().Disable(S_STANDOUT);
    }
}

static void effect_from_start()
{
    if (w3mApp::Instance().useColor)
    {
        Screen::Instance().SetFGColor(w3mApp::Instance().form_color);
    }
    else
    {
        Screen::Instance().Enable(S_STANDOUT);
    }
}
static void effect_form_end()
{
    if (w3mApp::Instance().useColor)
    {
        Screen::Instance().SetFGColor(w3mApp::Instance().basic_color);
    }
    else
    {
        Screen::Instance().Disable(S_STANDOUT);
    }
}

static void effect_mark_start()
{
    if (w3mApp::Instance().useColor)
    {
        Screen::Instance().SetBGColor(w3mApp::Instance().mark_color);
    }
    else
    {
        Screen::Instance().Enable(S_STANDOUT);
    }
}
static void effect_mark_end()
{
    if (w3mApp::Instance().useColor)
    {
        Screen::Instance().SetBGColor(w3mApp::Instance().bg_color);
    }
    else
    {
        Screen::Instance().Disable(S_STANDOUT);
    }
}

/*****************/
static void effect_active_start()
{
    if (w3mApp::Instance().useColor)
    {
        if (w3mApp::Instance().useActiveColor)
        {
            Screen::Instance().SetFGColor(w3mApp::Instance().active_color), Screen::Instance().Enable(S_UNDERLINE);
        }
        else
        {
            Screen::Instance().Enable(S_UNDERLINE);
        }
    }
    else
    {
        Screen::Instance().Enable(S_BOLD);
    }
}

static void effect_active_end()
{
    if (w3mApp::Instance().useColor)
    {
        if (w3mApp::Instance().useActiveColor)
        {
            (Screen::Instance().SetFGColor(w3mApp::Instance().basic_color), Screen::Instance().Disable(S_UNDERLINE));
        }
        else
        {
            Screen::Instance().Disable(S_UNDERLINE);
        }
    }
    else
    {
        Screen::Instance().Disable(S_BOLD);
    }
}

static void effect_visited_start()
{
    if (w3mApp::Instance().useVisitedColor)
    {
        if (w3mApp::Instance().useColor)
        {
            Screen::Instance().SetFGColor(w3mApp::Instance().visited_color);
        }
        else
        {
            ;
        }
    }
}

static void effect_visited_end()
{
    if (w3mApp::Instance().useVisitedColor)
    {
        if (w3mApp::Instance().useColor)
        {
            Screen::Instance().SetFGColor(w3mApp::Instance().basic_color);
        }
        else
        {
            ;
        }
    }
}

void fmTerm(void)
{
    if (w3mApp::Instance().fmInitialized)
    {
        Screen::Instance().Move((Terminal::lines() - 1), 0);
        Screen::Instance().CtrlToEolWithBGColor();
        Screen::Instance().Refresh();
        Terminal::flush();

        if (w3mApp::Instance().activeImage)
            loadImage(NULL, IMG_FLAG_STOP);

        Terminal::mouse_off();

        w3mApp::Instance().fmInitialized = false;
    }
}

/*
 * Initialize routine.
 */
void fmInit(void)
{
    if (!w3mApp::Instance().fmInitialized)
    {
        Screen::Instance().Setup();
        Terminal::term_raw();
        Terminal::term_noecho();

        ImageManager::Instance().initImage();
    }
    w3mApp::Instance().fmInitialized = true;
}

/*
 * Display some lines.
 */
static LinePtr cline = NULL;
static int ccolumn = -1;

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;
static Linecolor color_mode = 0;

#ifdef USE_BUFINFO
static BufferPtr save_current_buf = NULL;
#endif

static char *delayed_msg = NULL;

void clear_effect()
{
    if (somode)
    {
        somode = false;
        Screen::Instance().Disable(S_STANDOUT);
    }
    if (ulmode)
    {
        ulmode = false;
        Screen::Instance().Disable(S_UNDERLINE);
    }
    if (bomode)
    {
        bomode = false;
        Screen::Instance().Disable(S_BOLD);
    }
    if (emph_mode)
    {
        emph_mode = false;
        Screen::Instance().Disable(S_BOLD);
    }

    if (anch_mode)
    {
        anch_mode = false;
        effect_anchor_end();
    }
    if (imag_mode)
    {
        imag_mode = false;
        effect_image_end();
    }
    if (form_mode)
    {
        form_mode = false;
        effect_form_end();
    }
    if (visited_mode)
    {
        visited_mode = false;
        effect_visited_end();
    }
    if (active_mode)
    {
        active_mode = false;
        effect_active_end();
    }
    if (mark_mode)
    {
        mark_mode = false;
        effect_mark_end();
    }
    if (graph_mode)
    {
        graph_mode = false;
        Screen::Instance().Disable(S_GRAPHICS);
    }

    if (color_mode)
        do_color(0);
}

static void drawAnchorCursor(const BufferPtr &buf);

static int image_touch = 0;
static int draw_image_flag = false;
static LinePtr redrawLineImage(BufferPtr buf, LinePtr l, int i);

static int redrawLineRegion(BufferPtr buf, LinePtr l, int i, int bpos,
                            int epos);
static void do_effects(Lineprop m);

static Str make_lastline_link(BufferPtr buf, std::string_view title, std::string_view url)
{
    int l = Terminal::columns() - 1;
    Str s = NULL;
    if (title.size() && title[0])
    {
        s = Strnew_m_charp("[", title, "]");
        for (auto c = s->ptr; *c; ++c)
        {
            if (IS_CNTRL(*c) || IS_SPACE(*c))
            {
                *c = ' ';
            }
        }
        if (url.size())
            s->Push(" ");
        l -= get_Str_strwidth(s);
        if (l <= 0)
            return s;
    }
    if (url.empty())
        return s;

    auto pu = URL::Parse(url, buf->BaseURL());
    auto u = pu.ToStr();
    if (w3mApp::Instance().DecodeURL)
        u = Strnew(url_unquote_conv(u->c_str(), buf->document_charset));

    auto propstr = PropertiedString::create(u);

    if (l <= 4 || l >= get_Str_strwidth(u))
    {
        if (!s)
            return u;
        s->Push(u);
        return s;
    }
    if (!s)
        s = Strnew_size(Terminal::columns());
    auto i = (l - 2) / 2;

    while (i && propstr.propBuf()[i] & PC_WCHAR2)
        i--;

    s->Push(u->c_str(), i);
    s->Push("..");
    i = get_Str_strwidth(u) - (Terminal::columns() - 1 - get_Str_strwidth(s));

    while (i < u->Size() && propstr.propBuf()[i] & PC_WCHAR2)
        i++;

    s->Push(&u->c_str()[i]);
    return s;
}

static Str make_lastline_message(const BufferPtr &buf)
{
    Str msg, s = NULL;
    int sl = 0;

    if (w3mApp::Instance().displayLink)
    {
        MapAreaPtr a = retrieveCurrentMapArea(buf);
        if (a)
            s = make_lastline_link(buf, a->alt, a->url);
        else
        {
            auto a = buf->href.RetrieveAnchor(buf->CurrentPoint());
            std::string p = NULL;
            if (a && a->title.size() && a->title[0])
                p = a->title;
            else
            {
                auto a_img = buf->img.RetrieveAnchor(buf->CurrentPoint());
                if (a_img && a_img->title.size() && a_img->title[0])
                    p = a_img->title;
            }
            if (p.size() || a)
                s = make_lastline_link(buf, p, a ? a->url : "");
        }
        if (s)
        {
            sl = get_Str_strwidth(s);
            if (sl >= Terminal::columns() - 3)
                return s;
        }
    }

    if (w3mApp::Instance().use_mouse && GetMouseActionLastlineStr())
        msg = Strnew(GetMouseActionLastlineStr());
    else
        msg = Strnew();
    if (w3mApp::Instance().displayLineInfo && buf->LineCount() > 0)
    {
        int cl = buf->CurrentLine()->real_linenumber;
        int ll = buf->LastLine()->real_linenumber;
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
        int l = Terminal::columns() - 3 - sl;
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

static void drawAnchorCursor0(BufferPtr buf, AnchorList &al, int hseq,
                              int prevhseq, int tline, int eline, int active)
{
    auto l = buf->TopLine();
    for (int j = 0; j < al.size(); j++)
    {
        auto an = al.anchors[j];
        if (an->start.line < tline)
            continue;
        if (an->start.line >= eline)
            return;

        while (l)
        {
            if (l->linenumber == an->start.line)
                break;

            auto next = buf->NextLine(l);
            if (!next)
            {
                return;
            }
            if (next->linenumber < l->linenumber)
            {
                auto a = 0;
            }
            l = next;
        }

        if (hseq >= 0 && an->hseq == hseq)
        {
            //
            for (int i = an->start.pos; i < an->end.pos; i++)
            {
                if (l->propBuf()[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM))
                {
                    if (active)
                        l->propBuf()[i] |= PE_ACTIVE;
                    else
                        l->propBuf()[i] &= ~PE_ACTIVE;
                }
            }
            if (active)
                buf->DrawLineRegion(l, l->linenumber - tline + buf->rect.rootY,
                                    an->start.pos, an->end.pos);
        }
        else if (prevhseq >= 0 && an->hseq == prevhseq)
        {
            if (active)
                buf->DrawLineRegion(l, l->linenumber - tline + buf->rect.rootY,
                                    an->start.pos, an->end.pos);
        }
    }
}

static void drawAnchorCursor(const BufferPtr &buf)
{
    if (buf->LineCount() == 0)
        return;
    if (!buf->href && !buf->formitem)
        return;

    auto an = buf->href.RetrieveAnchor(buf->CurrentPoint());
    if (!an)
        an = retrieveCurrentMap(buf);

    int hseq;
    if (an)
        hseq = an->hseq;
    else
        hseq = -1;

    int tline = buf->TopLine()->linenumber;
    int eline = tline + buf->rect.lines;
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
static void redrawNLine(const BufferPtr &buf)
{
    // lines
    {
        int i = 0;
        for (auto l = buf->TopLine(); i < buf->rect.lines; i++, l = buf->NextLine(l))
        {
            if (l == NULL)
            {
                if (buf->pagerSource)
                {
                    l = getNextPage(buf, buf->rect.lines + buf->rect.rootY - i);
                }
            }
            if (l == NULL)
                break;
            buf->DrawLine(l, i + buf->rect.rootY);
        }
        Screen::Instance().Move(i + buf->rect.rootY, 0);
        Screen::Instance().CtrlToBottomEol();
    }

    if (!(w3mApp::Instance().activeImage && w3mApp::Instance().displayImage &&
          buf->img))
        return;

    auto [x, y] = buf->rect.globalXY();
    Screen::Instance().Move(y, x);

    {
        int i = 0;
        for (auto l = buf->TopLine(); i < buf->rect.lines && l;
             i++, l = buf->NextLine(l))
        {
            redrawLineImage(buf, l, i + buf->rect.rootY);
        }
    }
    getAllImage(buf);
}

static LinePtr redrawLineImage(BufferPtr buf, LinePtr l, int i)
{
    int j, pos, rcol;
    int column = buf->currentColumn;
    int x, y, sx, sy, w, h;

    if (l == NULL)
        return NULL;
    l->CalcWidth();
    if (l->len() == 0 || l->width() - 1 < column)
        return l;
    pos = columnPos(l, column);
    rcol = l->COLPOS(pos);
    for (j = 0; rcol - column < buf->rect.cols && pos + j < l->len(); j++)
    {
        if (rcol - column < 0)
        {
            rcol = l->COLPOS(pos + j + 1);
            continue;
        }
        auto a = buf->img.RetrieveAnchor({l->linenumber, pos + j});
        if (a && a->image && a->image->touch < image_touch)
        {
            Image *image = a->image;
            auto cache = image->cache =
                getImage(image, buf->BaseURL(), buf->image_flag);
            if (cache)
            {
                if ((image->width < 0 && cache->width > 0) ||
                    (image->height < 0 && cache->height > 0))
                {
                    image->width = cache->width;
                    image->height = cache->height;
                    buf->need_reshape = true;
                }
                x = (int)((rcol - column + buf->rect.rootX) *
                          w3mApp::Instance().pixel_per_char);
                y = (int)(i * w3mApp::Instance().pixel_per_line);
                sx = (int)((rcol - l->COLPOS(a->start.pos)) *
                           w3mApp::Instance().pixel_per_char);
                sy = (int)((l->linenumber - image->y) *
                           w3mApp::Instance().pixel_per_line);
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
                if (w >
                    (int)((buf->rect.right()) * w3mApp::Instance().pixel_per_char - x))
                    w = (int)((buf->rect.right()) * w3mApp::Instance().pixel_per_char - x);
                if (h > (int)((Terminal::lines() - 1) * w3mApp::Instance().pixel_per_line - y))
                    h = (int)((Terminal::lines() - 1) * w3mApp::Instance().pixel_per_line - y);
                addImage(cache, x, y, sx, sy, w, h);
                image->touch = image_touch;
                draw_image_flag = true;
            }
        }
        rcol = l->COLPOS(pos + j + 1);
    }
    return l;
}

#define do_effect1(effect, modeflag, action_start, action_end) \
    if (m & effect)                                            \
    {                                                          \
        if (!modeflag)                                         \
        {                                                      \
            action_start;                                      \
            modeflag = true;                                   \
        }                                                      \
    }

#define do_effect2(effect, modeflag, action_start, action_end) \
    if (modeflag)                                              \
    {                                                          \
        action_end;                                            \
        modeflag = false;                                      \
    }

static void do_effects(Lineprop m)
{
    /* effect end */
    do_effect2(PE_UNDER, ulmode, Screen::Instance().Enable(S_UNDERLINE), Screen::Instance().Disable(S_UNDERLINE));
    do_effect2(PE_STAND, somode, Screen::Instance().Enable(S_STANDOUT), Screen::Instance().Disable(S_STANDOUT));
    do_effect2(PE_BOLD, bomode, Screen::Instance().Enable(S_BOLD), Screen::Instance().Disable(S_BOLD));
    do_effect2(PE_EMPH, emph_mode, Screen::Instance().Enable(S_BOLD), Screen::Instance().Disable(S_BOLD));
    do_effect2(PE_ANCHOR, anch_mode, effect_anchor_start(), effect_anchor_end());
    do_effect2(PE_IMAGE, imag_mode, effect_image_start(), effect_image_end());
    do_effect2(PE_FORM, form_mode, effect_from_start(), effect_form_end());
    do_effect2(PE_VISITED, visited_mode, effect_visited_start(),
               effect_visited_end());
    do_effect2(PE_ACTIVE, active_mode, effect_active_start(),
               effect_active_end());
    do_effect2(PE_MARK, mark_mode, effect_mark_start(), effect_mark_end());
    if (graph_mode)
    {
        Screen::Instance().Disable(S_GRAPHICS);
        graph_mode = false;
    }

    /* effect start */
    do_effect1(PE_UNDER, ulmode, Screen::Instance().Enable(S_UNDERLINE), Screen::Instance().Disable(S_UNDERLINE));
    do_effect1(PE_STAND, somode, Screen::Instance().Enable(S_STANDOUT), Screen::Instance().Disable(S_STANDOUT));
    do_effect1(PE_BOLD, bomode, Screen::Instance().Enable(S_BOLD), Screen::Instance().Disable(S_BOLD));
    do_effect1(PE_EMPH, emph_mode, Screen::Instance().Enable(S_BOLD), Screen::Instance().Disable(S_BOLD));
    do_effect1(PE_ANCHOR, anch_mode, effect_anchor_start(), effect_anchor_end());
    do_effect1(PE_IMAGE, imag_mode, effect_image_start(), effect_image_end());
    do_effect1(PE_FORM, form_mode, effect_from_start(), effect_form_end());
    do_effect1(PE_VISITED, visited_mode, effect_visited_start(),
               EFFECT_VISITED_END);
    do_effect1(PE_ACTIVE, active_mode, effect_active_start(),
               effect_active_end());
    do_effect1(PE_MARK, mark_mode, effect_mark_start(), effect_mark_end());
}

void do_color(Linecolor c)
{
    if (c & 0x8)
        Screen::Instance().SetFGColor(c & 0x7);
    else if (color_mode & 0x8)
        Screen::Instance().SetFGColor(w3mApp::Instance().basic_color);
#ifdef USE_BG_COLOR
    if (c & 0x80)
        Screen::Instance().SetBGColor((c >> 4) & 0x7);
    else if (color_mode & 0x80)
        Screen::Instance().SetBGColor(w3mApp::Instance().bg_color);
#endif
    color_mode = c;
}

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
        if (Terminal::graph_ok() && c < N_GRAPH_SYMBOL)
        {
            if (!graph_mode)
            {
                Screen::Instance().Enable(S_GRAPHICS);
                graph_mode = true;
            }
#ifdef USE_M17N
            if (w == 2 && WcOption.use_wide)
                Screen::Instance().Puts(graph2_symbol[(int)c]);
            else
#endif
                Screen::Instance().PutAscii(*graph_symbol[(int)c]);
        }
        else
        {
#ifdef USE_M17N
            symbol = get_symbol(w3mApp::Instance().DisplayCharset, &w);
            Screen::Instance().Puts(symbol[(int)c]);
#else
            symbol = get_symbol();
            Screen::Instance().Putc(*symbol[(int)c]);
#endif
        }
    }
    else if (mode & PC_CTRL)
    {
        switch (c)
        {
        case '\t':
            Screen::Instance().PutAscii(c);
            break;
        case '\n':
            Screen::Instance().PutAscii(' ');
            break;
        case '\r':
            break;
        case DEL_CODE:
            Screen::Instance().Puts("^?");
            break;
        default:
            Screen::Instance().PutAscii('^');
            Screen::Instance().PutAscii(c + '@');
            break;
        }
    }

    else if (mode & PC_UNKNOWN)
    {
        char buf[5];
        sprintf(buf, "[%.2X]", (unsigned char)wtf_get_code((uint8_t *)p) | 0x80);
        Screen::Instance().Puts(buf);
    }
    else
    {
        Screen::Instance().PutChar(p, len);
    }
}

static GeneralList *message_list = NULL;

void record_err_message(const char *s)
{
    if (w3mApp::Instance().fmInitialized)
    {
        if (!message_list)
            message_list = newGeneralList();
        if (message_list->nitem >= Terminal::lines())
            popValue(message_list);
        pushValue(message_list, allocStr(s, -1));
    }
}

/*
 * List of error messages
 */
BufferPtr message_list_panel(void)
{
    Str tmp = Strnew_size(Terminal::lines() * Terminal::columns());
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
    return loadHTMLString(URL::Parse("w3m://message"), tmp->ptr);
}

void show_message(std::string_view msg)
{
    if (w3mApp::Instance().fmInitialized)
    {
        Terminal::term_cbreak();
        /* FIXME: gettextize? */
        message(msg, 0, 0);
        Screen::Instance().Refresh();
        Terminal::flush();
    }
}

void message(std::string_view s, int return_x, int return_y)
{
    if (!w3mApp::Instance().fmInitialized)
        return;
    Screen::Instance().Move((Terminal::lines() - 1), 0);
    Screen::Instance().PutsColumns(s.data(), Terminal::columns() - 1);
    Screen::Instance().CtrlToEolWithBGColor();
    Screen::Instance().Move(return_y, return_x);
}

void disp_err_message(const char *s, int redraw_current)
{
    record_err_message(s);
    disp_message(s, redraw_current);
}

void disp_message_nsec(const char *s, int redraw_current, int sec, int purge,
                       int mouse)
{
    if (w3mApp::Instance().QuietMessage)
        return;
    if (!w3mApp::Instance().fmInitialized)
    {
        fprintf(stderr, "%s\n", conv_to_system(s));
        return;
    }
    if (GetCurrentTab() != NULL && GetCurrentTab()->GetCurrentBuffer() != NULL)
        message(s, GetCurrentTab()->GetCurrentBuffer()->rect);
    else
        message(s, 0, (Terminal::lines() - 1));
    Screen::Instance().Refresh();
    Terminal::flush();

    Terminal::mouse_on();
    Terminal::sleep_till_anykey(sec, purge);
    Terminal::mouse_off();

    if (GetCurrentTab() != NULL && GetCurrentTab()->GetCurrentBuffer() != NULL &&
        redraw_current)
        displayCurrentbuf(B_NORMAL);
}

void disp_message(const char *s, int redraw_current)
{
    disp_message_nsec(s, redraw_current, 10, false, true);
}

void disp_message_nomouse(const char *s, int redraw_current)
{
    disp_message_nsec(s, redraw_current, 10, false, false);
}

void set_delayed_message(const char *s) { delayed_msg = allocStr(s, -1); }

void displayBuffer(BufferPtr buf, DisplayMode mode)
{
    Str msg;
    int ny = 0;

    if (!buf)
        return;
    if (buf->TopLine() == NULL &&
        buf->ReadBufferCache() == 0)
    { /* clear_buffer */
        mode = B_FORCE_REDRAW;
    }

    if (buf->width == 0)
        buf->width = w3mApp::Instance().INIT_BUFFER_WIDTH();
    if (buf->height == 0)
        buf->height = (Terminal::lines() - 1) + 1;
    if ((buf->width != w3mApp::Instance().INIT_BUFFER_WIDTH() &&
         (is_html_type(buf->type) || w3mApp::Instance().FoldLine)) ||
        buf->need_reshape)
    {
        buf->need_reshape = true;
        // buf->Reshape();
    }

    if (w3mApp::Instance().showLineNum)
    {
        buf->rect.updateRootX(buf->LastLine()->real_linenumber);
    }
    else
    {
        buf->rect.rootX = 0;
    }

    buf->rect.cols = Terminal::columns() - buf->rect.rootX;
    if (GetTabCount() > 1 || GetMouseActionMenuStr())
    {
        if (mode == B_FORCE_REDRAW || mode == B_REDRAW_IMAGE)
            calcTabPos();
        ny = GetTabbarHeight() + 1;
        if (ny > (Terminal::lines() - 1))
            ny = (Terminal::lines() - 1);
    }
    if (buf->rect.rootY != ny || buf->rect.lines != (Terminal::lines() - 1) - ny)
    {
        buf->rect.rootY = ny;
        buf->rect.lines = (Terminal::lines() - 1) - ny;
        buf->ArrangeCursor();
        mode = B_REDRAW_IMAGE;
    }
    if (mode == B_FORCE_REDRAW || mode == B_SCROLL || mode == B_REDRAW_IMAGE ||
        cline != buf->TopLine() || ccolumn != buf->currentColumn)
    {
        if (w3mApp::Instance().activeImage &&
            (mode == B_REDRAW_IMAGE || cline != buf->TopLine() ||
             ccolumn != buf->currentColumn))
        {
            if (draw_image_flag)
                Screen::Instance().Clear();
            ImageManager::Instance().clearImage();
            loadImage(buf, IMG_FLAG_STOP);
            image_touch++;
            draw_image_flag = false;
        }

        if (w3mApp::Instance().useColor)
        {
            Screen::Instance().SetFGColor(w3mApp::Instance().basic_color);
            Screen::Instance().SetBGColor(w3mApp::Instance().bg_color);
        }

        // TAB
        if (GetTabCount() > 1 || GetMouseActionMenuStr())
        {
            Screen::Instance().Move(0, 0);

            if (GetMouseActionMenuStr())
                Screen::Instance().Puts(GetMouseActionMenuStr());

            Screen::Instance().CtrlToEolWithBGColor();
            EachTab([](auto t) {
                auto b = t->GetCurrentBuffer();
                Screen::Instance().Move(t->Y(), t->Left());
                if (t == GetCurrentTab())
                    Screen::Instance().Enable(S_BOLD);
                Screen::Instance().PutAscii('[');
                auto l = t->Width() - get_strwidth(b->buffername.c_str());
                if (l < 0)
                    l = 0;
                if (l / 2 > 0)
                    Screen::Instance().PutsColumnsFillSpace(" ", l / 2);
                if (t == GetCurrentTab())
                    effect_active_start();
                Screen::Instance().PutsColumns(b->buffername.c_str(), t->Width());
                if (t == GetCurrentTab())
                    effect_active_end();
                if ((l + 1) / 2 > 0)
                    Screen::Instance().PutsColumnsFillSpace(" ", (l + 1) / 2);
                Screen::Instance().Move(t->Y(), t->Right());
                Screen::Instance().PutAscii(']');
                if (t == GetCurrentTab())
                    Screen::Instance().Disable(S_BOLD);
            });
            Screen::Instance().Move(GetTabbarHeight(), 0);
            for (int i = 0; i < Terminal::columns(); i++)
                Screen::Instance().PutAscii('~');
        }

        // draw
        redrawNLine(buf);

        cline = buf->TopLine();
        ccolumn = buf->currentColumn;
    }
    if (buf->TopLine() == NULL)
    {
        buf->SetTopLine(buf->FirstLine());
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
        disp_message(delayed_msg, false);
        delayed_msg = NULL;
        Screen::Instance().Refresh();
        Terminal::flush();
    }
    Screen::Instance().Enable(S_STANDOUT);
    message(msg->c_str(), buf->rect);
    Screen::Instance().Disable(S_STANDOUT);
    Terminal::title(conv_to_system(buf->buffername.c_str()));
    Screen::Instance().Refresh();
    Terminal::flush();

    if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage &&
        buf->img)
    {
        drawImage();
    }
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
