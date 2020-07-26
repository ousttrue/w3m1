/* $Id: func.c,v 1.27 2003/09/26 17:59:51 ukai Exp $ */
/*
 * w3m func.c
 */

#include <stdio.h>

#include "mouse.h"
#include "fm.h"
#include "indep.h"
#include "rc.h"
#include "myctype.h"
#include "dispatcher.h"
#include "commands.h"
#include "symbol.h"
#include "tab.h"
#include "file.h"
#include "display.h"
#include "tab.h"
#include "types.h"
#include "buffer.h"
#include "anchor.h"

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
        line->Strip();
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
        SKIP_BLANKS(p);
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
        const char **symbol = get_symbol(DisplayCharset, &w);
        mouse_action.lastline_str =
            Strnew_charp(symbol[N_GRAPH_SYMBOL + 13])->ptr;
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
    else if (y == (LINES-1))
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
        if (y == GetCurrentTab()->GetCurrentBuffer()->cursorY + GetCurrentTab()->GetCurrentBuffer()->rootY &&
            (x == GetCurrentTab()->GetCurrentBuffer()->cursorX + GetCurrentTab()->GetCurrentBuffer()->rootX || (WcOption.use_wide && GetCurrentTab()->GetCurrentBuffer()->currentLine != NULL &&
                                                                        (CharType(GetCurrentTab()->GetCurrentBuffer()->currentLine->propBuf[GetCurrentTab()->GetCurrentBuffer()->pos]) == PC_KANJI1) && x == GetCurrentTab()->GetCurrentBuffer()->cursorX + GetCurrentTab()->GetCurrentBuffer()->rootX + 1)))
        {
            if (retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer()) ||
                retrieveCurrentForm(GetCurrentTab()->GetCurrentBuffer()))
            {
                map = &mouse_action.active_map[(int)btn];
                if (!(map && map->func))
                    map = &mouse_action.anchor_map[(int)btn];
            }
        }
        else
        {
            int cx = GetCurrentTab()->GetCurrentBuffer()->cursorX, cy = GetCurrentTab()->GetCurrentBuffer()->cursorY;
            cursorXY(GetCurrentTab()->GetCurrentBuffer(), x - GetCurrentTab()->GetCurrentBuffer()->rootX, y - GetCurrentTab()->GetCurrentBuffer()->rootY);
            if (y == GetCurrentTab()->GetCurrentBuffer()->cursorY + GetCurrentTab()->GetCurrentBuffer()->rootY &&
                (x == GetCurrentTab()->GetCurrentBuffer()->cursorX + GetCurrentTab()->GetCurrentBuffer()->rootX
#ifdef USE_M17N
                 || (WcOption.use_wide && GetCurrentTab()->GetCurrentBuffer()->currentLine != NULL &&
                     (CharType(GetCurrentTab()->GetCurrentBuffer()->currentLine->propBuf[GetCurrentTab()->GetCurrentBuffer()->pos]) == PC_KANJI1) && x == GetCurrentTab()->GetCurrentBuffer()->cursorX + GetCurrentTab()->GetCurrentBuffer()->rootX + 1)
#endif
                     ) &&
                (retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer()) ||
                 retrieveCurrentForm(GetCurrentTab()->GetCurrentBuffer())))
                map = &mouse_action.anchor_map[(int)btn];
            cursorXY(GetCurrentTab()->GetCurrentBuffer(), cx, cy);
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
        (*map->func)();
        CurrentCmdData = NULL;
    }
}

static int mouse_scroll_line()
{
    if (relative_wheel_scroll)
        return (relative_wheel_scroll_ratio * (LINES-1) + 99) / 100;
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
                    moveTab(posTab(press_x, press_y), posTab(x, y),
                            (press_y == y) ? (press_x < x) : (press_y < y));
                    displayCurrentbuf(B_FORCE_REDRAW);
                    return;
                }
                else if (press_x >= GetCurrentTab()->GetCurrentBuffer()->rootX)
                {
                    BufferPtr buf = GetCurrentTab()->GetCurrentBuffer();
                    int cx = GetCurrentTab()->GetCurrentBuffer()->cursorX, cy = GetCurrentTab()->GetCurrentBuffer()->cursorY;

                    t = posTab(x, y);
                    if (t == NULL)
                        return;
                    cursorXY(GetCurrentTab()->GetCurrentBuffer(), press_x - GetCurrentTab()->GetCurrentBuffer()->rootX,
                             press_y - GetCurrentTab()->GetCurrentBuffer()->rootY);
                    if (GetCurrentTab()->GetCurrentBuffer()->cursorY == press_y - GetCurrentTab()->GetCurrentBuffer()->rootY &&
                        (GetCurrentTab()->GetCurrentBuffer()->cursorX == press_x - GetCurrentTab()->GetCurrentBuffer()->rootX
#ifdef USE_M17N
                         || (WcOption.use_wide &&
                             GetCurrentTab()->GetCurrentBuffer()->currentLine != NULL &&
                             (CharType(GetCurrentTab()->GetCurrentBuffer()->currentLine->propBuf[GetCurrentTab()->GetCurrentBuffer()->pos]) == PC_KANJI1) && GetCurrentTab()->GetCurrentBuffer()->cursorX == press_x - GetCurrentTab()->GetCurrentBuffer()->rootX - 1)
#endif
                             ))
                    {
                        displayCurrentbuf(B_NORMAL);
                        followTab(t);
                    }
                    if (buf == GetCurrentTab()->GetCurrentBuffer())
                        cursorXY(GetCurrentTab()->GetCurrentBuffer(), cx, cy);
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
                    ldown1();
                }
                else if (delta_y < 0)
                {
                    set_prec_num(-delta_y);
                    lup1();
                }
                if (delta_x > 0)
                {
                    set_prec_num(delta_x);
                    col1L();
                }
                else if (delta_x < 0)
                {
                    set_prec_num(-delta_x);
                    col1R();
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
                ldown1();
            break;
        case MouseBtnAction::BTN5_DOWN_RXVT:
            for (i = 0; i < mouse_scroll_line(); i++)
                lup1();
            break;
        }
    }
    else if (btn == MouseBtnAction::BTN4_DOWN_XTERM)
    {
        for (i = 0; i < mouse_scroll_line(); i++)
            ldown1();
    }
    else if (btn == MouseBtnAction::BTN5_DOWN_XTERM)
    {
        for (i = 0; i < mouse_scroll_line(); i++)
            lup1();
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
