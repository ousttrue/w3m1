/* $Id: func.c,v 1.27 2003/09/26 17:59:51 ukai Exp $ */
/*
 * w3m func.c
 */

#include <stdio.h>
#include "fm.h"
#include "gc_helper.h"
#include "rc.h"
#include "indep.h"
#include "myctype.h"
#include "dispatcher.h"
#include "commands.h"
#include "symbol.h"
#include "file.h"
#include "html/anchor.h"
#include "frontend/mouse.h"
#include "frontend/tab.h"
#include "frontend/display.h"
#include "frontend/tab.h"
#include "frontend/tabbar.h"
#include "frontend/buffer.h"
#include "frontend/terms.h"

struct MouseActionMap
{
    Command func;
    char *data;
};

struct MouseAction
{
    char *menu_str;
    char *lastline_str;
    int menu_width;
    int lastline_width;
    int in_action;
    int cursorX;
    int cursorY;
    MouseActionMap default_map[3];
    MouseActionMap anchor_map[3];
    MouseActionMap active_map[3];
    MouseActionMap tab_map[3];
    MouseActionMap *menu_map[3];
    MouseActionMap *lastline_map[3];
};
static MouseAction mouse_action;

static MouseAction default_mouse_action = {
    NULL,
    "<=UpDn",
    0,
    6,
    FALSE,
    0,
    0,
    {{movMs, NULL}, {backBf, NULL}, {menuMs, NULL}}, /* default */
    {{NULL, NULL}, {NULL, NULL}, {NULL, NULL}},      /* anchor */
    {{followA, NULL}, {NULL, NULL}, {NULL, NULL}},   /* active */
    {{tabMs, NULL}, {closeTMs, NULL}, {NULL, NULL}}, /* tab */
    {NULL, NULL, NULL},                              /* menu */
    {NULL, NULL, NULL}                               /* lastline */
};
static MouseActionMap default_lastline_action[6] = {
    {backBf, NULL},
    {backBf, NULL},
    {pgBack, NULL},
    {pgBack, NULL},
    {pgFore, NULL},
    {pgFore, NULL}};

void DisableMouseAction()
{
    mouse_action.in_action = FALSE;
}

bool TryGetMouseActionPosition(int *x, int *y)
{
    if (mouse_action.in_action)
    {
        if (x)
        {
            *x = mouse_action.cursorX;
        }
        if (y)
        {
            *y = mouse_action.cursorY;
        }
        return true;
    }
    return false;
}

char *GetMouseActionMenuStr()
{
    return mouse_action.menu_str;
};

int GetMouseActionMenuWidth()
{
    return mouse_action.menu_width;
};

char *GetMouseActionLastlineStr()
{
    return mouse_action.lastline_str;
}

static void
setMouseAction0(char **str, int *width, MouseActionMap **map, char *p)
{
    char *s;
    int b, w, x;

    s = getQWord(&p);
    if (!*s)
    {
        *str = NULL;
        width = 0;
        for (b = 0; b < 3; b++)
            map[b] = NULL;
        return;
    }
    w = *width;
    *str = s;
    *width = get_strwidth(s);
    if (*width >= LIMIT_MOUSE_MENU)
        *width = LIMIT_MOUSE_MENU;
    if (*width <= w)
        return;
    for (b = 0; b < 3; b++)
    {
        if (!map[b])
            continue;
        map[b] = New_Reuse(MouseActionMap, map[b], *width);
        for (x = w + 1; x < *width; x++)
        {
            map[b][x].func = NULL;
            map[b][x].data = NULL;
        }
    }
}

static void
setMouseAction1(MouseActionMap **map, int width, char *p)
{
    char *s;
    int x, x2;

    if (!*map)
    {
        *map = New_N(MouseActionMap, width);
        for (x = 0; x < width; x++)
        {
            (*map)[x].func = NULL;
            (*map)[x].data = NULL;
        }
    }
    s = getWord(&p);
    x = atoi(s);
    if (!(IS_DIGIT(*s) && x >= 0 && x < width))
        return; /* error */
    s = getWord(&p);
    x2 = atoi(s);
    if (!(IS_DIGIT(*s) && x2 >= 0 && x2 < width))
        return; /* error */
    s = getWord(&p);
    Command f = getFuncList(s);
    s = getQWord(&p);
    if (!*s)
        s = NULL;
    for (; x <= x2; x++)
    {
        (*map)[x].func = f;
        (*map)[x].data = s;
    }
}

static void
setMouseAction2(MouseActionMap *map, char *p)
{
    char *s;

    s = getWord(&p);
    Command f = getFuncList(s);
    s = getQWord(&p);
    if (!*s)
        s = NULL;
    map->func = f;
    map->data = s;
}

static void
interpret_mouse_action(FILE *mf)
{
    Str line;
    char *p, *s;
    int b;

    while (!feof(mf))
    {
        line = Strfgets(mf);
        Strip(line);
        if (line->Size() == 0)
            continue;
        p = conv_from_system(line->ptr);
        s = getWord(&p);
        if (*s == '#') /* comment */
            continue;
        if (!strcmp(s, "menu"))
        {
            setMouseAction0(&mouse_action.menu_str, &mouse_action.menu_width,
                            mouse_action.menu_map, p);
            continue;
        }
        else if (!strcmp(s, "lastline"))
        {
            setMouseAction0(&mouse_action.lastline_str,
                            &mouse_action.lastline_width,
                            mouse_action.lastline_map, p);
            continue;
        }
        if (strcmp(s, "button"))
            continue; /* error */
        s = getWord(&p);
        b = atoi(s) - 1;
        if (!(b >= 0 && b <= 2))
            continue; /* error */
        SKIP_BLANKS(&p);
        if (IS_DIGIT(*p))
            s = "menu";
        else
            s = getWord(&p);
        if (!strcasecmp(s, "menu"))
        {
            if (!mouse_action.menu_str)
                continue;
            setMouseAction1(&mouse_action.menu_map[b], mouse_action.menu_width,
                            p);
        }
        else if (!strcasecmp(s, "lastline"))
        {
            if (!mouse_action.lastline_str)
                continue;
            setMouseAction1(&mouse_action.lastline_map[b],
                            mouse_action.lastline_width, p);
        }
        else if (!strcasecmp(s, "default"))
            setMouseAction2(&mouse_action.default_map[b], p);
        else if (!strcasecmp(s, "anchor"))
            setMouseAction2(&mouse_action.anchor_map[b], p);
        else if (!strcasecmp(s, "active"))
            setMouseAction2(&mouse_action.active_map[b], p);
        else if (!strcasecmp(s, "tab"))
            setMouseAction2(&mouse_action.tab_map[b], p);
    }
}

void initMouseAction(void)
{
    FILE *mf;

    bcopy((void *)&default_mouse_action, (void *)&mouse_action,
          sizeof(default_mouse_action));
    mouse_action.lastline_map[0] = New_N(MouseActionMap, 6);
    bcopy((void *)&default_lastline_action,
          (void *)mouse_action.lastline_map[0],
          sizeof(default_lastline_action));
    {

        int w = 0;
        const char **symbol = get_symbol(w3mApp::Instance().DisplayCharset, &w);
        mouse_action.lastline_str =
            Strnew(symbol[N_GRAPH_SYMBOL + 13])->ptr;
    }

    if ((mf = fopen(confFile(MOUSE_FILE), "rt")) != NULL)
    {
        interpret_mouse_action(mf);
        fclose(mf);
    }
    if ((mf = fopen(rcFile(MOUSE_FILE), "rt")) != NULL)
    {
        interpret_mouse_action(mf);
        fclose(mf);
    }
}

void do_mouse_action(MouseBtnAction btn, int x, int y)
{
    int ny = -1;
    if (GetTabCount() > 1 || GetMouseActionMenuStr())
        ny = GetTabbarHeight();

    switch (btn)
    {
    case MouseBtnAction::BTN1_DOWN:
        // btn = 0;
        break;
    case MouseBtnAction::BTN2_DOWN:
        // btn = 1;
        break;
    case MouseBtnAction::BTN3_DOWN:
        // btn = 2;
        break;
    default:
        return;
    }

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    MouseActionMap *map = NULL;
    if (y < ny)
    {
        if (GetMouseActionMenuStr() && x >= 0 && x < mouse_action.menu_width)
        {
            if (mouse_action.menu_map[(int)btn])
                map = &mouse_action.menu_map[(int)btn][x];
        }
        else
            map = &mouse_action.tab_map[(int)btn];
    }
    else if (y == (::LINES - 1))
    {
        if (mouse_action.lastline_str && x >= 0 &&
            x < mouse_action.lastline_width)
        {
            if (mouse_action.lastline_map[(int)btn])
                map = &mouse_action.lastline_map[(int)btn][x];
        }
    }
    else if (y > ny)
    {
        auto [_x, _y] = buf->rect.globalXY();
        x = _x;
        y = _y;
        if (y &&
            (x || (WcOption.use_wide && buf->CurrentLine() != NULL &&
                   (CharType(buf->CurrentLine()->propBuf()[buf->pos]) == PC_KANJI1) && x == buf->rect.cursorX + buf->rect.rootX + 1)))
        {
            if (buf->RetrieveAnchor(buf->CurrentPoint()) || retrieveCurrentForm(buf))
            {
                map = &mouse_action.active_map[(int)btn];
                if (!(map && map->func))
                    map = &mouse_action.anchor_map[(int)btn];
            }
        }
        else
        {
            int cx = buf->rect.cursorX;
            int cy = buf->rect.cursorY;
            buf->CursorXY(x - buf->rect.rootX, y - buf->rect.rootY);

            auto [_x, _y] = buf->rect.globalXY();
            x = _x;
            y = _y;
            if (y &&
                (x || (WcOption.use_wide && buf->CurrentLine() != NULL &&
                       (CharType(buf->CurrentLine()->propBuf()[buf->pos]) == PC_KANJI1) && x == buf->rect.cursorX + buf->rect.rootX + 1)) &&
                (buf->RetrieveAnchor(buf->CurrentPoint()) || retrieveCurrentForm(buf)))
                map = &mouse_action.anchor_map[(int)btn];
            buf->CursorXY(cx, cy);
        }
    }
    else
    {
        return;
    }
    if (!(map && map->func))
        map = &mouse_action.default_map[(int)btn];
    if (map && map->func)
    {
        mouse_action.in_action = TRUE;
        mouse_action.cursorX = x;
        mouse_action.cursorY = y;
        ClearCurrentKey();
        ClearCurrentKeyData();
        CurrentCmdData = map->data;
        (*map->func)(&w3mApp::Instance());
        CurrentCmdData = NULL;
    }
}

static int mouse_scroll_line()
{
    if (relative_wheel_scroll)
        return (relative_wheel_scroll_ratio * (::LINES - 1) + 99) / 100;
    else
        return fixed_wheel_scroll_count;
}

void process_mouse(MouseBtnAction btn, int x, int y)
{
    int delta_x, delta_y, i;
    static auto press_btn = MouseBtnAction::BTN_RESET;
    static int press_x, press_y;
    TabPtr t;
    int ny = -1;

    if (GetTabCount() > 1 || GetMouseActionMenuStr())
        ny = GetTabbarHeight();
    if (btn == MouseBtnAction::BTN_UP)
    {
        switch (press_btn)
        {
        case MouseBtnAction::BTN1_DOWN:
            if (press_y == y && press_x == x)
                do_mouse_action(press_btn, x, y);
            else if (ny > 0 && y < ny)
            {
                if (press_y < ny)
                {
                    moveTab(GetTabByPosition(press_x, press_y), GetTabByPosition(x, y),
                            (press_y == y) ? (press_x < x) : (press_y < y));
                    displayCurrentbuf(B_FORCE_REDRAW);
                    return;
                }
                else if (press_x >= GetCurrentTab()->GetCurrentBuffer()->rect.rootX)
                {
                    BufferPtr buf = GetCurrentTab()->GetCurrentBuffer();
                    int cx = GetCurrentTab()->GetCurrentBuffer()->rect.cursorX;
                    int cy = GetCurrentTab()->GetCurrentBuffer()->rect.cursorY;

                    t = GetTabByPosition(x, y);
                    if (t == NULL)
                        return;
                    GetCurrentTab()->GetCurrentBuffer()->CursorXY(press_x - GetCurrentTab()->GetCurrentBuffer()->rect.rootX,
                                                                  press_y - GetCurrentTab()->GetCurrentBuffer()->rect.rootY);
                    if (GetCurrentTab()->GetCurrentBuffer()->rect.cursorY == press_y - GetCurrentTab()->GetCurrentBuffer()->rect.rootY &&
                        (GetCurrentTab()->GetCurrentBuffer()->rect.cursorX == press_x - GetCurrentTab()->GetCurrentBuffer()->rect.rootX

                         || (WcOption.use_wide &&
                             GetCurrentTab()->GetCurrentBuffer()->CurrentLine() != NULL &&
                             (CharType(GetCurrentTab()->GetCurrentBuffer()->CurrentLine()->propBuf()[GetCurrentTab()->GetCurrentBuffer()->pos]) == PC_KANJI1) && GetCurrentTab()->GetCurrentBuffer()->rect.cursorX == press_x - GetCurrentTab()->GetCurrentBuffer()->rect.rootX - 1)

                             ))
                    {
                        displayCurrentbuf(B_NORMAL);
                        followTab(t);
                    }
                    if (buf == GetCurrentTab()->GetCurrentBuffer())
                        GetCurrentTab()->GetCurrentBuffer()->CursorXY(cx, cy);
                }
                return;
            }
            else
            {
                delta_x = x - press_x;
                delta_y = y - press_y;

                if (abs(delta_x) < abs(delta_y) / 3)
                    delta_x = 0;
                if (abs(delta_y) < abs(delta_x) / 3)
                    delta_y = 0;
                if (reverse_mouse)
                {
                    delta_y = -delta_y;
                    delta_x = -delta_x;
                }
                if (delta_y > 0)
                {
                    set_prec_num(delta_y);
                    ldown1(&w3mApp::Instance());
                }
                else if (delta_y < 0)
                {
                    set_prec_num(-delta_y);
                    lup1(&w3mApp::Instance());
                }
                if (delta_x > 0)
                {
                    set_prec_num(delta_x);
                    col1L(&w3mApp::Instance());
                }
                else if (delta_x < 0)
                {
                    set_prec_num(-delta_x);
                    col1R(&w3mApp::Instance());
                }
            }
            break;
        case MouseBtnAction::BTN2_DOWN:
        case MouseBtnAction::BTN3_DOWN:
            if (press_y == y && press_x == x)
                do_mouse_action(press_btn, x, y);
            break;
        case MouseBtnAction::BTN4_DOWN_RXVT:
            for (i = 0; i < mouse_scroll_line(); i++)
                ldown1(&w3mApp::Instance());
            break;
        case MouseBtnAction::BTN5_DOWN_RXVT:
            for (i = 0; i < mouse_scroll_line(); i++)
                lup1(&w3mApp::Instance());
            break;
        }
    }
    else if (btn == MouseBtnAction::BTN4_DOWN_XTERM)
    {
        for (i = 0; i < mouse_scroll_line(); i++)
            ldown1(&w3mApp::Instance());
    }
    else if (btn == MouseBtnAction::BTN5_DOWN_XTERM)
    {
        for (i = 0; i < mouse_scroll_line(); i++)
            lup1(&w3mApp::Instance());
    }

    if (btn != MouseBtnAction::BTN4_DOWN_RXVT || press_btn == MouseBtnAction::BTN_RESET)
    {
        press_btn = btn;
        press_x = x;
        press_y = y;
    }
    else
    {
        press_btn = MouseBtnAction::BTN_RESET;
    }
}
