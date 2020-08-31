#include <array>
#include <stdio.h>
#include <string_view_util.h>
#include "rc.h"
#include "indep.h"
#include "myctype.h"
#include "command_dispatcher.h"
#include "commands.h"
#include "symbol.h"
#include "file.h"
#include "frontend/anchor.h"
#include "frontend/mouse.h"
#include "frontend/tab.h"
#include "frontend/display.h"
#include "frontend/tab.h"
#include "frontend/tabbar.h"
#include "frontend/buffer.h"
#include "frontend/terminal.h"

#define LIMIT_MOUSE_MENU 100

struct MouseActionMap
{
    Command func;
    std::string data;
};

struct MouseAction
{
    std::string menu_str;
    std::string lastline_str;
    int menu_width;
    int lastline_width;
    int in_action;
    int cursorX;
    int cursorY;
    std::array<MouseActionMap, 3> default_map;
    std::array<MouseActionMap, 3> anchor_map;
    std::array<MouseActionMap, 3> active_map;
    std::array<MouseActionMap, 3> tab_map;
    std::array<std::vector<MouseActionMap>, 3> menu_map;
    std::array<std::vector<MouseActionMap>, 3> lastline_map;
};
static MouseAction mouse_action;

static MouseAction default_mouse_action = {
    "",
    "<=UpDn",
    0,
    6,
    false,
    0,
    0,
    {MouseActionMap{movMs, ""}, MouseActionMap{backBf, ""}, MouseActionMap{menuMs, ""}}, /* default */
    {MouseActionMap{NULL, ""}, MouseActionMap{NULL, ""}, MouseActionMap{NULL, ""}},      /* anchor */
    {MouseActionMap{followA, ""}, MouseActionMap{NULL, ""}, MouseActionMap{NULL, ""}},   /* active */
    {MouseActionMap{tabMs, ""}, MouseActionMap{closeTMs, ""}, MouseActionMap{NULL, ""}}, /* tab */
};
static MouseActionMap default_lastline_action[6] = {
    {backBf, ""},
    {backBf, ""},
    {pgBack, ""},
    {pgBack, ""},
    {pgFore, ""},
    {pgFore, ""}};

void DisableMouseAction()
{
    mouse_action.in_action = false;
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

std::string_view GetMouseActionMenuStr()
{
    return mouse_action.menu_str;
};

int GetMouseActionMenuWidth()
{
    return mouse_action.menu_width;
};

const char *GetMouseActionLastlineStr()
{
    return mouse_action.lastline_str.c_str();
}

static std::string
setMouseAction0(int *width, std::array<std::vector<MouseActionMap>, 3> &map, std::string_view p)
{
    std::string s;
    std::tie(p, s) = getQWord(p);
    if (s.empty())
    {
        width = 0;
        for (int b = 0; b < 3; b++)
            map[b].clear();
        return "";
    }

    int w = *width;
    auto str = s;
    *width = get_strwidth(s.data());
    if (*width >= LIMIT_MOUSE_MENU)
        *width = LIMIT_MOUSE_MENU;
    if (*width <= w)
        return str;
    for (int b = 0; b < 3; b++)
    {
        map[b].resize(*width);
        for (int x = w + 1; x < *width; x++)
        {
            map[b][x].func = NULL;
            map[b][x].data.clear();
        }
    }
}

static void setMouseAction1(std::vector<MouseActionMap> &map, int width, std::string_view p)
{
    map.resize(width);
    for (int x = 0; x < width; x++)
    {
        map[x].func = NULL;
        map[x].data.clear();
    }

    std::string s;
    std::tie(p, s) = getWord(p);
    auto x = atoi(s.c_str());
    if (!(IS_DIGIT(s[0]) && x >= 0 && x < width))
        return; /* error */
    std::tie(p, s) = getWord(p);
    int x2 = atoi(s.c_str());
    if (!(IS_DIGIT(s[0]) && x2 >= 0 && x2 < width))
        return; /* error */
    std::tie(p, s) = getWord(p);
    Command f = getFuncList(s);
    std::tie(p, s) = getQWord(p);
    for (; x <= x2; x++)
    {
        map[x].func = f;
        map[x].data = s;
    }
}

static void
setMouseAction2(MouseActionMap *map, std::string_view p)
{
    std::string s;
    std::tie(p, s) = getWord(p);
    Command f = getFuncList(s);
    std::tie(p, s) = getQWord(p);
    map->func = f;
    map->data = s;
}

static void
interpret_mouse_action(FILE *mf)
{
    while (!feof(mf))
    {
        auto line = Strfgets(mf);
        Strip(line);
        if (line->Size() == 0)
            continue;
        std::string_view p = conv_from_system(line->ptr);
        std::string s;
        std::tie(p, s) = getWord(p);
        if (s.size() && s[0] == '#') /* comment */
            continue;
        if (s == "menu")
        {
            mouse_action.menu_str = setMouseAction0(&mouse_action.menu_width, mouse_action.menu_map, p);
            continue;
        }
        else if (s == "lastline")
        {
            mouse_action.lastline_str = setMouseAction0(&mouse_action.lastline_width, mouse_action.lastline_map, p);
            continue;
        }
        if (s == "button")
            continue; /* error */
        std::tie(p, s) = getWord(p);
        auto b = atoi(s.c_str()) - 1;
        if (!(b >= 0 && b <= 2))
            continue; /* error */
        p = svu::strip_left(p);
        if (IS_DIGIT(p[0]))
            s = "menu";
        else
            std::tie(p, s) = getWord(p);
        if (svu::ic_eq(s, "menu"))
        {
            if (mouse_action.menu_str.empty())
                continue;
            setMouseAction1(mouse_action.menu_map[b], mouse_action.menu_width, p);
        }
        else if (svu::ic_eq(s, "lastline"))
        {
            if (mouse_action.lastline_str.empty())
                continue;
            setMouseAction1(mouse_action.lastline_map[b], mouse_action.lastline_width, p);
        }
        else if (svu::ic_eq(s, "default"))
            setMouseAction2(&mouse_action.default_map[b], p);
        else if (svu::ic_eq(s, "anchor"))
            setMouseAction2(&mouse_action.anchor_map[b], p);
        else if (svu::ic_eq(s, "active"))
            setMouseAction2(&mouse_action.active_map[b], p);
        else if (svu::ic_eq(s, "tab"))
            setMouseAction2(&mouse_action.tab_map[b], p);
    }
}

void initMouseAction(void)
{
    FILE *mf;

    // bcopy((void *)&default_mouse_action, (void *)&mouse_action,
    //       sizeof(default_mouse_action));
    mouse_action.lastline_map[0].resize(6);
    // bcopy((void *)&default_lastline_action, (void *)mouse_action.lastline_map[0], sizeof(default_lastline_action));
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
    if (GetTabCount() > 1 || GetMouseActionMenuStr().size())
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
    auto buf = GetCurrentBuffer();
    MouseActionMap *map = NULL;
    if (y < ny)
    {
        if (GetMouseActionMenuStr().size() && x >= 0 && x < mouse_action.menu_width)
        {
            if (mouse_action.menu_map[(int)btn].size())
                map = &mouse_action.menu_map[(int)btn][x];
        }
        else
            map = &mouse_action.tab_map[(int)btn];
    }
    else if (y == (::Terminal::lines() - 1))
    {
        if (mouse_action.lastline_str.size() && x >= 0 &&
            x < mouse_action.lastline_width)
        {
            if (mouse_action.lastline_map[(int)btn].size())
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
            if (buf->m_document->href.RetrieveAnchor(buf->CurrentPoint()) || buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint()))
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
                (buf->m_document->href.RetrieveAnchor(buf->CurrentPoint()) || buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint())))
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
        mouse_action.in_action = true;
        mouse_action.cursorX = x;
        mouse_action.cursorY = y;
        (*map->func)(&w3mApp::Instance(), {
            data: map->data,
        });
    }
}

static int mouse_scroll_line()
{
    if (w3mApp::Instance().relative_wheel_scroll)
        return (w3mApp::Instance().relative_wheel_scroll_ratio * (::Terminal::lines() - 1) + 99) / 100;
    else
        return w3mApp::Instance().fixed_wheel_scroll_count;
}

void process_mouse(MouseBtnAction btn, int x, int y)
{
    int delta_x, delta_y, i;
    static auto press_btn = MouseBtnAction::BTN_RESET;
    static int press_x, press_y;
    TabPtr t;
    int ny = -1;

    if (GetTabCount() > 1 || GetMouseActionMenuStr().size())
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
                    return;
                }
                else if (press_x >= GetCurrentBuffer()->rect.rootX)
                {
                    BufferPtr buf = GetCurrentBuffer();
                    int cx = GetCurrentBuffer()->rect.cursorX;
                    int cy = GetCurrentBuffer()->rect.cursorY;

                    t = GetTabByPosition(x, y);
                    if (t == NULL)
                        return;
                    GetCurrentBuffer()->CursorXY(press_x - GetCurrentBuffer()->rect.rootX,
                                                                  press_y - GetCurrentBuffer()->rect.rootY);
                    if (GetCurrentBuffer()->rect.cursorY == press_y - GetCurrentBuffer()->rect.rootY &&
                        (GetCurrentBuffer()->rect.cursorX == press_x - GetCurrentBuffer()->rect.rootX

                         || (WcOption.use_wide &&
                             GetCurrentBuffer()->CurrentLine() != NULL &&
                             (CharType(GetCurrentBuffer()->CurrentLine()->propBuf()[GetCurrentBuffer()->pos]) == PC_KANJI1) && GetCurrentBuffer()->rect.cursorX == press_x - GetCurrentBuffer()->rect.rootX - 1)

                             ))
                    {
                        // followTab(t, {});
                    }
                    if (buf == GetCurrentBuffer())
                        GetCurrentBuffer()->CursorXY(cx, cy);
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
                if (w3mApp::Instance().reverse_mouse)
                {
                    delta_y = -delta_y;
                    delta_x = -delta_x;
                }
                if (delta_y > 0)
                {
                    ldown1(&w3mApp::Instance(), {
                        prec : delta_y,
                    });
                }
                else if (delta_y < 0)
                {
                    lup1(&w3mApp::Instance(), {
                        prec : -delta_y,
                    });
                }
                if (delta_x > 0)
                {
                    col1L(&w3mApp::Instance(), {
                        prec : delta_x,
                    });
                }
                else if (delta_x < 0)
                {
                    col1R(&w3mApp::Instance(), {
                        prec : -delta_x,
                    });
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
                ldown1(&w3mApp::Instance(), {});
            break;
        case MouseBtnAction::BTN5_DOWN_RXVT:
            for (i = 0; i < mouse_scroll_line(); i++)
                lup1(&w3mApp::Instance(), {});
            break;
        }
    }
    else if (btn == MouseBtnAction::BTN4_DOWN_XTERM)
    {
        for (i = 0; i < mouse_scroll_line(); i++)
            ldown1(&w3mApp::Instance(), {});
    }
    else if (btn == MouseBtnAction::BTN5_DOWN_XTERM)
    {
        for (i = 0; i < mouse_scroll_line(); i++)
            lup1(&w3mApp::Instance(), {});
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
