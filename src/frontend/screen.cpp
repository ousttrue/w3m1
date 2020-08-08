#include "screen.h"
#include "termcap_str.h"
#include "terms.h"
#include "w3m.h"
#include <myctype.h>
#include <gc_helper.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <wc.h>

/* Line status */
enum LineStatus : uint16_t
{
    L_NONE = 0,
    L_DIRTY = 0x01,
    L_UNUSED = 0x02,
    L_NEED_CE = 0x04,
    L_CLRTOEOL = 0x08,
};

struct ScreenLine
{
    char **lineimage;
    l_prop *lineprop;
    LineStatus isdirty;
    short eol;
};

// termcap
extern "C" char *tgoto(char *, int, int);

#define SETCH(var, ch, len) ((var) = New_Reuse(char, (var), (len) + 1), \
                             strncpy((var), (ch), (len)), (var)[len] = '\0')
#define SETPROP(var, prop) (var = (((var)&S_DIRTY) | prop))
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define CHMODE(c) ((c)&C_WHICHCHAR)
#define MOVE(line, column) writestr(tgoto(T_cm, column, line));
#define SPACE " "

const int tab_step = 8;

static int
need_redraw(char *c1, l_prop pr1, const char *c2, l_prop pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

static char *
color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + 30);
    return seqbuf;
}

static char *
bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}

enum RefreshStatus
{
    RF_NEED_TO_MOVE = 0,
    RF_CR_OK = 1,
    RF_NONEED_TO_MOVE = 2,
};

class ScreenImpl
{
    int m_lines = 0;
    int m_cols = 0;
    int CurLine = 0;
    int CurColumn = 0;

    int max_LINES = 0;
    int max_COLS = 0;
    ScreenLine *ScreenElem = nullptr;
    ScreenLine **ScreenImage = nullptr;

    int graph_enabled = 0;

public:
    void Setup(int LINES, int COLS)
    {
        int i;

        if (LINES + 1 > max_LINES)
        {
            m_lines = LINES;

            max_LINES = LINES + 1;
            max_COLS = 0;
            ScreenElem = New_N(ScreenLine, max_LINES);
            ScreenImage = New_N(ScreenLine *, max_LINES);
        }
        if (COLS + 1 > max_COLS)
        {
            m_cols = COLS;

            max_COLS = COLS + 1;
            for (i = 0; i < max_LINES; i++)
            {
                ScreenElem[i].lineimage = New_N(char *, max_COLS);
                bzero((void *)ScreenElem[i].lineimage, max_COLS * sizeof(char *));
                ScreenElem[i].lineprop = New_N(l_prop, max_COLS);
            }
        }
        for (i = 0; i < LINES; i++)
        {
            ScreenImage[i] = &ScreenElem[i];
            ScreenImage[i]->lineprop[0] = S_EOL;
            ScreenImage[i]->isdirty = L_NONE;
        }
        for (; i < max_LINES; i++)
        {
            ScreenElem[i].isdirty = L_UNUSED;
        }
    }

    void Wrap()
    {
        if (CurLine == (m_lines - 1))
            return;
        CurLine++;
        CurColumn = 0;
    }

    void Move(int line, int column)
    {
        if (line >= 0 && line < m_lines)
            CurLine = line;
        if (column >= 0 && column < m_cols)
            CurColumn = column;
    }

    void Clear()
    {
        int i, j;
        l_prop *p;
        writestr(T_cl);
        move(0, 0);
        for (i = 0; i < m_lines; i++)
        {
            ScreenImage[i]->isdirty = L_NONE;
            p = ScreenImage[i]->lineprop;
            for (j = 0; j < m_cols; j++)
            {
                p[j] = S_EOL;
            }
        }
    }

    void TouchColumn(int col)
    {
        if (col >= 0 && col < COLS)
            ScreenImage[CurLine]->lineprop[col] |= S_DIRTY;
    }

    void TouchCurrentLine()
    {
        if (!(ScreenImage[CurLine]->isdirty & L_DIRTY))
        {
            int i;
            for (i = 0; i < COLS; i++)
                ScreenImage[CurLine]->lineprop[i] &= ~S_DIRTY;
            ScreenImage[CurLine]->isdirty |= L_DIRTY;
        }
    }

    void CtrlToEol()
    {
        int i;
        l_prop *lprop = ScreenImage[CurLine]->lineprop;

        if (lprop[CurColumn] & S_EOL)
            return;

        if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) ||
            ScreenImage[CurLine]->eol > CurColumn)
            ScreenImage[CurLine]->eol = CurColumn;

        ScreenImage[CurLine]->isdirty |= L_CLRTOEOL;
        touch_line();
        for (i = CurColumn; i < COLS && !(lprop[i] & S_EOL); i++)
        {
            lprop[i] = S_EOL | S_DIRTY;
        }
    }

    void CtrlToEolWithBGColor()
    {
        int cli = CurLine;
        int cco = CurColumn;
        for (int i = CurColumn; i < COLS; i++)
            addch(' ');
        move(cli, cco);
    }

    void CtrlToBottomEol()
    {
        int l, c;

        l = CurLine;
        c = CurColumn;
        (*clrtoeol)();
        CurColumn = 0;
        CurLine++;
        for (; CurLine < LINES; CurLine++)
            (*clrtoeol)();
        CurLine = l;
        CurColumn = c;
    }

    void TouchCursor()
    {

        int i;

        touch_line();

        for (i = CurColumn; i >= 0; i--)
        {
            TouchColumn(i);
            if (CHMODE(ScreenImage[CurLine]->lineprop[i]) != C_WCHAR2)
                break;
        }
        for (i = CurColumn + 1; i < COLS; i++)
        {
            if (CHMODE(ScreenImage[CurLine]->lineprop[i]) != C_WCHAR2)
                break;
            TouchColumn(i);
        }
    }

    void StandToggle()
    {
        int i;
        l_prop *pr = ScreenImage[CurLine]->lineprop;
        pr[CurColumn] ^= S_STANDOUT;
        if (CHMODE(pr[CurColumn]) != C_WCHAR2)
        {
            for (i = CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
                pr[i] ^= S_STANDOUT;
        }
    }

    void AddChar(l_prop CurrentMode, const char *pc, int len)
    {
        l_prop *pr;
        int dest, i;

        static Str tmp = NULL;
        char **p;
        char c = *pc;
        int width = wtf_width((uint8_t *)pc);

        if (tmp == NULL)
            tmp = Strnew();
        tmp->CopyFrom(pc, len);
        pc = tmp->ptr;

        if (CurColumn == m_cols)
            Wrap();
        if (CurColumn >= m_cols)
            return;
        p = ScreenImage[CurLine]->lineimage;
        pr = ScreenImage[CurLine]->lineprop;
        auto dirty = &ScreenImage[CurLine]->isdirty;

        if (pr[CurColumn] & S_EOL)
        {
            if (c == ' ' && !(CurrentMode & M_SPACE))
            {
                CurColumn++;
                return;
            }
            for (i = CurColumn; i >= 0 && (pr[i] & S_EOL); i--)
            {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            }
        }

        if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
            SETCHMODE(CurrentMode, C_CTRL);
        else if (len > 1)
            SETCHMODE(CurrentMode, C_WCHAR1);
        else if (!IS_CNTRL(c))
            SETCHMODE(CurrentMode, C_ASCII);
        else
            return;

        /* Required to erase bold or underlined character for some * terminal
     * emulators. */
        i = CurColumn + width - 1;
        if (i < COLS &&
            (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], pc, CurrentMode)) ||
             ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE))))
        {
            touch_line();
            i++;
            if (i < COLS)
            {
                TouchColumn(i);
                if (pr[i] & S_EOL)
                {
                    SETCH(p[i], SPACE, 1);
                    SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
                }
                else
                {
                    for (i++; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++)
                        TouchColumn(i);
                }
            }
        }

        if (CurColumn + width > COLS)
        {
            touch_line();
            for (i = CurColumn; i < COLS; i++)
            {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                TouchColumn(i);
            }
            wrap();
            if (CurColumn + width > COLS)
                return;
            p = ScreenImage[CurLine]->lineimage;
            pr = ScreenImage[CurLine]->lineprop;
        }
        if (CHMODE(pr[CurColumn]) == C_WCHAR2)
        {
            touch_line();
            for (i = CurColumn - 1; i >= 0; i--)
            {
                l_prop l = CHMODE(pr[i]);
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                TouchColumn(i);
                if (l != C_WCHAR2)
                    break;
            }
        }

        if (CHMODE(CurrentMode) != C_CTRL)
        {
            if (need_redraw(p[CurColumn], pr[CurColumn], pc, CurrentMode))
            {
                SETCH(p[CurColumn], pc, len);
                SETPROP(pr[CurColumn], CurrentMode);
                touch_line();
                TouchColumn(CurColumn);
                SETCHMODE(CurrentMode, C_WCHAR2);
                for (i = CurColumn + 1; i < CurColumn + width; i++)
                {
                    SETCH(p[i], SPACE, 1);
                    SETPROP(pr[i], (pr[CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                    TouchColumn(i);
                }
                for (; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++)
                {
                    SETCH(p[i], SPACE, 1);
                    SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                    TouchColumn(i);
                }
            }
            CurColumn += width;
        }
        else if (c == '\t')
        {
            dest = (CurColumn + tab_step) / tab_step * tab_step;
            if (dest >= COLS)
            {
                wrap();
                touch_line();
                dest = tab_step;
                p = ScreenImage[CurLine]->lineimage;
                pr = ScreenImage[CurLine]->lineprop;
            }
            for (i = CurColumn; i < dest; i++)
            {
                if (need_redraw(p[i], pr[i], SPACE, CurrentMode))
                {
                    SETCH(p[i], SPACE, 1);
                    SETPROP(pr[i], CurrentMode);
                    touch_line();
                    TouchColumn(i);
                }
            }
            CurColumn = i;
        }
        else if (c == '\n')
        {
            wrap();
        }
        else if (c == '\r')
        { /* Carriage return */
            CurColumn = 0;
        }
        else if (c == '\b' && CurColumn > 0)
        { /* Backspace */
            CurColumn--;
            while (CurColumn > 0 && CHMODE(pr[CurColumn]) == C_WCHAR2)
                CurColumn--;
        }
    }

    void Refresh(FILE *ttyf)
    {
        int col, pcol;
        int pline = CurLine;
        int moved = RF_NEED_TO_MOVE;
        char **pc;
        l_prop *pr, mode = 0;
        l_prop color = COL_FTERM;
        l_prop bcolor = COL_BTERM;

        WCWriter writer(w3mApp::Instance().InnerCharset, w3mApp::Instance().DisplayCharset, ttyf);

        for (auto line = 0; line <= (LINES - 1); line++)
        {
            auto dirty = &ScreenImage[line]->isdirty;
            if (*dirty & L_DIRTY)
            {
                *dirty &= ~L_DIRTY;
                pc = ScreenImage[line]->lineimage;
                pr = ScreenImage[line]->lineprop;
                for (col = 0; col < COLS && !(pr[col] & S_EOL); col++)
                {
                    if (*dirty & L_NEED_CE && col >= ScreenImage[line]->eol)
                    {
                        if (need_redraw(pc[col], pr[col], SPACE, 0))
                            break;
                    }
                    else
                    {
                        if (pr[col] & S_DIRTY)
                            break;
                    }
                }
                if (*dirty & (L_NEED_CE | L_CLRTOEOL))
                {
                    pcol = ScreenImage[line]->eol;
                    if (pcol >= COLS)
                    {
                        *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
                        pcol = col;
                    }
                }
                else
                {
                    pcol = col;
                }
                if (line < LINES - 2 && pline == line - 1 && pcol == 0)
                {
                    switch (moved)
                    {
                    case RF_NEED_TO_MOVE:
                        MOVE(line, 0);
                        moved = RF_CR_OK;
                        break;
                    case RF_CR_OK:
                        write1('\n');
                        write1('\r');
                        break;
                    case RF_NONEED_TO_MOVE:
                        moved = RF_CR_OK;
                        break;
                    }
                }
                else
                {
                    MOVE(line, pcol);
                    moved = RF_CR_OK;
                }
                if (*dirty & (L_NEED_CE | L_CLRTOEOL))
                {
                    writestr(T_ce);
                    if (col != pcol)
                        MOVE(line, col);
                }
                pline = line;
                pcol = col;
                for (; col < COLS; col++)
                {
                    if (pr[col] & S_EOL)
                        break;

                    /* 
                * some terminal emulators do linefeed when a
                * character is put on COLS-th column. this behavior
                * is different from one of vt100, but such terminal
                * emulators are used as vt100-compatible
                * emulators. This behaviour causes scroll when a
                * character is drawn on (COLS-1,LINES-1) point.  To
                * avoid the scroll, I prohibit to draw character on
                * (COLS-1,LINES-1).
                */
                    if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) ||
                        (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) ||
                        (!(pr[col] & S_BOLD) && (mode & S_BOLD)) ||
                        (!(pr[col] & S_COLORED) && (mode & S_COLORED)) || (!(pr[col] & S_BCOLORED) && (mode & S_BCOLORED)) || (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS)))
                    {
                        if ((mode & S_COLORED) || (mode & S_BCOLORED))
                            writestr(T_op);
                        if (mode & S_GRAPHICS)
                            writestr(T_ae);
                        writestr(T_me);
                        mode &= ~M_MEND;
                    }
                    if ((*dirty & L_NEED_CE && col >= ScreenImage[line]->eol) ? need_redraw(pc[col], pr[col], SPACE,
                                                                                            0)
                                                                              : (pr[col] & S_DIRTY))
                    {
                        if (pcol == col - 1)
                            writestr(T_nd);
                        else if (pcol != col)
                            MOVE(line, col);

                        if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT))
                        {
                            writestr(T_so);
                            mode |= S_STANDOUT;
                        }
                        if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE))
                        {
                            writestr(T_us);
                            mode |= S_UNDERLINE;
                        }
                        if ((pr[col] & S_BOLD) && !(mode & S_BOLD))
                        {
                            writestr(T_md);
                            mode |= S_BOLD;
                        }
                        if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR)
                        {
                            color = (pr[col] & COL_FCOLOR);
                            mode = ((mode & ~COL_FCOLOR) | color);
                            writestr(color_seq(color));
                        }

                        if ((pr[col] & S_BCOLORED) && (pr[col] ^ mode) & COL_BCOLOR)
                        {
                            bcolor = (pr[col] & COL_BCOLOR);
                            mode = ((mode & ~COL_BCOLOR) | bcolor);
                            writestr(bcolor_seq(bcolor));
                        }

                        if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS))
                        {
                            writer.end();
                            if (!graph_enabled)
                            {
                                graph_enabled = 1;
                                writestr(T_eA);
                            }
                            writestr(T_as);
                            mode |= S_GRAPHICS;
                        }

                        if (pr[col] & S_GRAPHICS)
                            write1(graphchar(*pc[col]));
                        else if (CHMODE(pr[col]) != C_WCHAR2)
                            writer.putc(pc[col]);
                        pcol = col + 1;
                    }
                }
                if (col == COLS)
                    moved = RF_NEED_TO_MOVE;
                for (; col < COLS && !(pr[col] & S_EOL); col++)
                    pr[col] |= S_EOL;
            }
            *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
            if (mode & M_MEND)
            {
                if (mode & (S_COLORED | S_BCOLORED))
                {
                    writestr(T_op);
                }
                if (mode & S_GRAPHICS)
                {
                    writestr(T_ae);
                    writer.clear_status();
                }
                writestr(T_me);
                mode &= ~M_MEND;
            }
        }

        writer.end();

        MOVE(CurLine, CurColumn);
    }
};

Screen::Screen()
    : m_impl(new ScreenImpl)
{
}

Screen::~Screen()
{
    delete m_impl;
}

void Screen::Setup(int lines, int cols)
{
    m_impl->Setup(lines, cols);
}

void Screen::AddChar(const char *pc, int len)
{
    m_impl->AddChar(CurrentMode, pc, len);
}

void Screen::Wrap()
{
    m_impl->Wrap();
}

void Screen::TouchColumn(int col)
{
    m_impl->TouchColumn(col);
}

void Screen::TouchCurrentLine()
{
    m_impl->TouchCurrentLine();
}

void Screen::TouchCursor()
{
    m_impl->TouchCursor();
}

void Screen::StandToggle()
{
    m_impl->StandToggle();
}

void Screen::Move(int line, int column)
{
    m_impl->Move(line, column);
}

void Screen::Refresh(FILE *ttyf)
{
    m_impl->Refresh(ttyf);
}

void Screen::Clear()
{
    m_impl->Clear();
    CurrentMode = C_ASCII;
}

void Screen::CtrlToEol()
{
    m_impl->CtrlToEol();
}

void Screen::CtrlToBottomEol()
{
    m_impl->CtrlToBottomEol();
}

void Screen::CtrlToEolWithBGColor()
{
    if (!(CurrentMode & S_BCOLORED))
    {
        clrtoeol();
        return;
    }
    auto pr = CurrentMode;
    CurrentMode = (CurrentMode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    m_impl->CtrlToEolWithBGColor();
    CurrentMode = pr;
}
