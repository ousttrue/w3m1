#pragma once
#include <stdio.h>

typedef unsigned short l_prop;

/* Screen properties */
enum ScreenProperties
{
    S_SCREENPROP = 0x0f,
    S_NORMAL = 0x00,
    S_STANDOUT = 0x01,
    S_UNDERLINE = 0x02,
    S_BOLD = 0x04,
    S_EOL = 0x08,
    S_GRAPHICS = 0x10,
    S_DIRTY = 0x20,

    /* Sort of Character */
    C_WHICHCHAR = 0xc0,
    C_ASCII = 0x00,
    C_WCHAR1 = 0x40,
    C_WCHAR2 = 0x80,
    C_CTRL = 0xc0,

    /* Charactor Color */
    S_COLORED = 0xf00,
    COL_FCOLOR = 0xf00,
    COL_FBLACK = 0x800,
    COL_FRED = 0x900,
    COL_FGREEN = 0xa00,
    COL_FYELLOW = 0xb00,
    COL_FBLUE = 0xc00,
    COL_FMAGENTA = 0xd00,
    COL_FCYAN = 0xe00,
    COL_FWHITE = 0xf00,
    COL_FTERM = 0x000,

    /* Background Color */
    S_BCOLORED = 0xf000,
    COL_BCOLOR = 0xf000,
    COL_BBLACK = 0x8000,
    COL_BRED = 0x9000,
    COL_BGREEN = 0xa000,
    COL_BYELLOW = 0xb000,
    COL_BBLUE = 0xc000,
    COL_BMAGENTA = 0xd000,
    COL_BCYAN = 0xe000,
    COL_BWHITE = 0xf000,
    COL_BTERM = 0x0000,

    M_MEND = (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_BCOLORED | S_GRAPHICS),
    M_SPACE = (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS),
    M_CEOL = (~(M_SPACE | C_WHICHCHAR)),
};

class Screen
{
    class ScreenImpl *m_impl;
    l_prop CurrentMode = 0;

    Screen(const Screen &) = delete;
    Screen &operator=(const Screen &) = delete;

public:
    Screen();
    ~Screen();

    static Screen &Instance();

    void Setup();
    void Clear();
    void Move(int line, int column);
    void Puts(const char *c, int len);
    void Wrap();
    void TouchColumn(int col);
    void TouchCurrentLine();
    void TouchCursor();
    void Enable(ScreenProperties sp)
    {
        CurrentMode |= sp;
    }
    void Disable(ScreenProperties sp)
    {
        CurrentMode &= ~sp;
    }
    void StandToggle();
    void SetFGColor(int color)
    {
        CurrentMode &= ~COL_FCOLOR;
        if ((color & 0xf) <= 7)
            CurrentMode |= (((color & 7) | 8) << 8);
    }

    void SetBGColor(int color)
    {
        CurrentMode &= ~COL_BCOLOR;
        if ((color & 0xf) <= 7)
            CurrentMode |= (((color & 7) | 8) << 12);
    }

    void CtrlToEol();
    void CtrlToEolWithBGColor();
    void CtrlToBottomEol();
    void Refresh();
};
