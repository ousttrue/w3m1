/* $Id: menu.h,v 1.2 2001/11/20 17:49:23 ukai Exp $ */
/*
 * w3m menu.h
 */

#ifndef MENU_H
#define MENU_H

#include "w3m.h"
#include "frontend/tab.h"
#include "frontend/link.h"
#include <tcb/span.hpp>

enum MenuTypes
{
    MENU_ERROR = -1,
    MENU_END = 0,
    MENU_NOP = 1,
    MENU_VALUE = 2,
    MENU_FUNC = 4,
    MENU_POPUP = 8,
};

#define MENU_NOTHING -1
#define MENU_CANCEL -2
#define MENU_CLOSE -3

/* Addition:mouse event */
enum MouseEventTypes
{
    MOUSE_BTN1_DOWN = 0,
    MOUSE_BTN2_DOWN = 1,
    MOUSE_BTN3_DOWN = 2,
    MOUSE_BTN4_DOWN_RXVT = 3,
    MOUSE_BTN5_DOWN_RXVT = 4,
    MOUSE_BTN4_DOWN_XTERM = 64,
    MOUSE_BTN5_DOWN_XTERM = 65,
    MOUSE_BTN_UP = 3,
    MOUSE_BTN_RESET = -1,
};

struct Menu;

struct MenuItem
{
    int type;
    std::string label;
    int *variable;
    int value;
    Command func;
    Menu *popup;
    std::string keys;
    std::string data;

    MenuTypes setMenuItem(std::string_view type, std::string_view line);
};

struct Menu
{
    Menu *parent;
    int cursorX;
    int cursorY;
    int x;
    int y;
    int width;
    int height;
    int nitem;
    MenuItem *item;
    int initial;
    int select;
    int offset;
    int active;
    int (*keymap[128])(char c);
    int keyselect[128];
};

struct MenuList
{
    const char *id;
    Menu *menu;
    MenuItem *item;
};

void new_menu(Menu *menu, MenuItem *item);
void geom_menu(Menu *menu, int x, int y, int mselect);
void draw_all_menu(Menu *menu);
void draw_menu(Menu *menu);
void draw_menu_item(Menu *menu, int mselect);
int select_menu(Menu *menu, int mselect);
void goto_menu(Menu *menu, int mselect, int down);
void up_menu(Menu *menu, int n);
void down_menu(Menu *menu, int n);
int action_menu(Menu *menu);
void popup_menu(Menu *parent, Menu *menu);
void guess_menu_xy(Menu *menu, int width, int *x, int *y);
void new_option_menu(Menu *menu, tcb::span<std::string> label, int *variable, Command func);

int addMenuList(MenuList **list, const char *id);
int getMenuN(MenuList *list, std::string_view id);
void popupMenu(int x, int y, Menu *menu);
void optionMenu(int x, int y, tcb::span<std::string> label, int *variable, int initial, Command func);
void mainMenu(int x, int y);
void initMenu(void);

void PopupMenu(std::string_view data);
void PopupBufferMenu();
void PopupTabMenu();
Link *link_menu(const BufferPtr &buf);
AnchorPtr accesskey_menu(const BufferPtr &buf);
AnchorPtr list_menu(const BufferPtr &buf);

#endif /* not MENU_H */
