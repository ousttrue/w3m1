#include "screen.h"
#include "termcap_str.h"
#include "terms.h"
#include "terminal.h"
#include "w3m.h"
#include <myctype.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <wc.h>

/* Line status */
enum LineStatus
{
    L_NONE = 0,
    L_DIRTY = 0x01,
    L_UNUSED = 0x02,
    L_NEED_CE = 0x04,
    L_CLRTOEOL = 0x08,
};
inline bool ISDIRTY(LineStatus d)
{
    return d & L_DIRTY;
}

#define ISUNUSED(d) ((d)&L_UNUSED)
#define NEED_CE(d) ((d)&L_NEED_CE)

// termcap
extern "C" char *tgoto(char *, int, int);

void SETCH(SingleCharacter &var, const char *ch, int len)
{
    // (*var) = New_Reuse(char, (*var), (len) + 1);
    // strncpy((&var), (ch), (len));

    var = SingleCharacter(ch, len);

    // (*var)[len] = '\0';
}

#define SETPROP(var, prop) (var = (((var)&S_DIRTY) | prop))
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define CHMODE(c) ((c)&C_WHICHCHAR)
#define MOVE(line, column) Terminal::Terminal::writestr(tgoto(T_cm, column, line));

const auto SPACE = " ";

const int tab_step = 8;

static bool need_redraw(const SingleCharacter &c, l_prop pr1, const char *c2, l_prop pr2)
{
    auto c1 = (char *)c.bytes.data();
    if (!c1 || !c2 || strncmp(c1, c2, c.size()))
        return true;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return true;

    return false;
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
    int CurLine = 0;
    int CurColumn = 0;

    std::vector<SingleCharacter> m_chars;
    std::vector<l_prop> m_props;
    std::vector<LineStatus> m_lineStatus;
    std::vector<short> m_lineEnds;

    int graph_enabled = 0;

    int Lines() const
    {
        return m_lineStatus.size();
    }
    int Cols() const
    {
        auto lines = Lines();
        if (lines == 0)
        {
            return 0;
        }
        return m_chars.size() / lines;
    }

public:
    void Setup(int newLines, int newCols)
    {
        Terminal::Instance();
        
        auto lines = Lines();
        auto cols = Cols();
        if (newLines == lines && newCols == cols)
        {
            return;
        }

        std::vector<SingleCharacter> chars(newLines * newCols);
        std::vector<l_prop> props(newLines * newCols);
        std::vector<LineStatus> lineStatus(newLines, L_NONE);
        std::vector<short> lineEnds(newLines, S_EOL);

        auto sc = m_chars.data();
        auto sp = m_props.data();

        auto c = chars.data();
        auto p = props.data();

        // copy
        for (int i = 0; i < lines; ++i, sc += cols, sp += cols, c += newCols, p += newCols)
        {
            memcpy(c, sc, cols);
            memcpy(p, sp, cols * sizeof(l_prop));
            lineStatus[i] = m_lineStatus[i];
            lineEnds[i] = m_lineEnds[i];
        }

        // swap
        std::swap(chars, m_chars);
        std::swap(props, m_props);
        std::swap(lineStatus, m_lineStatus);
        std::swap(lineEnds, m_lineEnds);
    }

    void Wrap()
    {
        if (CurLine == (Lines() - 1))
            return;
        CurLine++;
        CurColumn = 0;
    }

    void Move(int line, int column)
    {
        if (line >= 0 && line < Lines())
            CurLine = line;
        if (column >= 0 && column < Cols())
            CurColumn = column;
    }

    void Clear()
    {
        Terminal::Terminal::writestr(T_cl);
        move(0, 0);
        std::fill(m_lineStatus.begin(), m_lineStatus.end(), L_NONE);
        std::fill(m_props.begin(), m_props.end(), S_EOL);
    }

    int LineHead() const
    {
        auto value = CurLine * Cols();
        return value;
    }

    void TouchColumn(int col)
    {
        if (col >= 0 && col < Cols())
        {
            m_props[LineHead() + col] |= S_DIRTY;
        }
    }

    void TouchCurrentLine()
    {
        if (!(m_lineStatus[CurLine] & L_DIRTY))
        {
            auto index = LineHead();
            for (int i = 0; i < Cols(); i++, ++index)
            {
                m_props[index] &= ~S_DIRTY;
            }
            m_lineStatus[CurLine] |= L_DIRTY;
        }
    }

    void CtrlToEol()
    {
        auto lprop = m_props.data() + LineHead();
        if (lprop[CurColumn] & S_EOL)
        {
            return;
        }

        if (!(m_lineStatus[CurLine] & (L_NEED_CE | L_CLRTOEOL)) ||
            m_lineEnds[CurLine] > CurColumn)
        {
            m_lineEnds[CurLine] = CurColumn;
        }

        m_lineStatus[CurLine] |= L_CLRTOEOL;
        touch_line();
        for (int i = CurColumn; i < Cols() && !(lprop[i] & S_EOL); i++)
        {
            lprop[i] = S_EOL | S_DIRTY;
        }
    }

    void CtrlToEolWithBGColor()
    {
        int cli = CurLine;
        int cco = CurColumn;
        for (int i = CurColumn; i < Cols(); i++)
            addch(' ');
        move(cli, cco);
    }

    void CtrlToBottomEol()
    {
        auto l = CurLine;
        auto c = CurColumn;
        (*clrtoeol)();
        CurColumn = 0;
        CurLine++;
        auto lines = Lines();
        for (; CurLine < lines; CurLine++)
            (*clrtoeol)();
        CurLine = l;
        CurColumn = c;
    }

    void TouchCursor()
    {
        touch_line();

        auto lineprop = m_props.data() + LineHead();
        for (int i = CurColumn; i >= 0; i--)
        {
            TouchColumn(i);
            if (CHMODE(lineprop[i]) != C_WCHAR2)
                break;
        }
        for (int i = CurColumn + 1; i < Cols(); i++)
        {
            if (CHMODE(lineprop[i]) != C_WCHAR2)
                break;
            TouchColumn(i);
        }
    }

    void StandToggle()
    {
        l_prop *pr = m_props.data() + LineHead();
        pr[CurColumn] ^= S_STANDOUT;
        if (CHMODE(pr[CurColumn]) != C_WCHAR2)
        {
            for (int i = CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
                pr[i] ^= S_STANDOUT;
        }
    }

    void AddChar(l_prop CurrentMode, const char *pc, int len)
    {
        // l_prop *pr;
        // int dest, i;
        // char **p;

        char c = *pc;
        int width = wtf_width((uint8_t *)pc);

        static Str tmp = NULL;
        if (tmp == NULL)
            tmp = Strnew();
        tmp->CopyFrom(pc, len);
        pc = tmp->ptr;

        if (CurColumn == Cols())
            Wrap();
        if (CurColumn >= Cols())
            return;

        auto p = m_chars.data() + LineHead();
        auto pr = m_props.data() + LineHead();
        auto dirty = &m_lineStatus[CurLine];

        if (pr[CurColumn] & S_EOL)
        {
            if (c == ' ' && !(CurrentMode & M_SPACE))
            {
                CurColumn++;
                return;
            }
            for (int i = CurColumn; i >= 0 && (pr[i] & S_EOL); i--)
            {
                // fill space
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
        int i = CurColumn + width - 1;
        auto cols = Cols();
        if (i < cols &&
            (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], pc, CurrentMode)) ||
             ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE))))
        {
            touch_line();
            i++;
            if (i < cols)
            {
                TouchColumn(i);
                if (pr[i] & S_EOL)
                {
                    SETCH(p[i], SPACE, 1);
                    SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
                }
                else
                {
                    for (i++; i < cols && CHMODE(pr[i]) == C_WCHAR2; i++)
                        TouchColumn(i);
                }
            }
        }

        if (CurColumn + width > cols)
        {
            touch_line();
            for (i = CurColumn; i < cols; i++)
            {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                TouchColumn(i);
            }
            wrap();
            if (CurColumn + width > cols)
                return;
            p = m_chars.data() + LineHead();
            pr = m_props.data() + LineHead();
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
                for (; i < cols && CHMODE(pr[i]) == C_WCHAR2; i++)
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
            auto dest = (CurColumn + tab_step) / tab_step * tab_step;
            if (dest >= cols)
            {
                wrap();
                touch_line();
                dest = tab_step;
                p = m_chars.data() + LineHead();
                pr = m_props.data() + LineHead();
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

    void Refresh()
    {
        int col, pcol;
        int pline = CurLine;
        int moved = RF_NEED_TO_MOVE;
        l_prop mode = 0;
        l_prop color = COL_FTERM;
        l_prop bcolor = COL_BTERM;

        WCWriter writer(w3mApp::Instance().InnerCharset, w3mApp::Instance().DisplayCharset, Terminal::file());

        auto pc = m_chars.data();
        auto pr = m_props.data();
        auto cols = Cols();
        auto lines = Lines();
        for (auto line = 0; line <= (lines - 1); line++, pc += cols, pr += cols)
        {
            auto dirty = &m_lineStatus[line];
            if (*dirty & L_DIRTY)
            {
                *dirty &= ~L_DIRTY;
                for (col = 0; col < cols && !(pr[col] & S_EOL); col++)
                {
                    if (*dirty & L_NEED_CE && col >= m_lineEnds[line])
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
                    pcol = m_lineEnds[line];
                    if (pcol >= cols)
                    {
                        *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
                        pcol = col;
                    }
                }
                else
                {
                    pcol = col;
                }
                if (line < lines - 2 && pline == line - 1 && pcol == 0)
                {
                    switch (moved)
                    {
                    case RF_NEED_TO_MOVE:
                        MOVE(line, 0);
                        moved = RF_CR_OK;
                        break;
                    case RF_CR_OK:
                        Terminal::write1('\n');
                        Terminal::write1('\r');
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
                    Terminal::writestr(T_ce);
                    if (col != pcol)
                        MOVE(line, col);
                }
                pline = line;
                pcol = col;
                for (; col < cols; col++)
                {
                    if (pr[col] & S_EOL)
                        break;

                    /* 
                * some terminal emulators do linefeed when a
                * character is put on Terminal::columns()-th column. this behavior
                * is different from one of vt100, but such terminal
                * emulators are used as vt100-compatible
                * emulators. This behaviour causes scroll when a
                * character is drawn on (Terminal::columns()-1,Terminal::lines()-1) point.  To
                * avoid the scroll, I prohibit to draw character on
                * (Terminal::columns()-1,Terminal::lines()-1).
                */
                    if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) ||
                        (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) ||
                        (!(pr[col] & S_BOLD) && (mode & S_BOLD)) ||
                        (!(pr[col] & S_COLORED) && (mode & S_COLORED)) || (!(pr[col] & S_BCOLORED) && (mode & S_BCOLORED)) || (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS)))
                    {
                        if ((mode & S_COLORED) || (mode & S_BCOLORED))
                            Terminal::writestr(T_op);
                        if (mode & S_GRAPHICS)
                            Terminal::writestr(T_ae);
                        Terminal::writestr(T_me);
                        mode &= ~M_MEND;
                    }
                    if ((*dirty & L_NEED_CE && col >= m_lineEnds[line])
                            ? need_redraw(pc[col], pr[col], SPACE, 0)
                            : (pr[col] & S_DIRTY))
                    {
                        if (pcol == col - 1)
                            Terminal::writestr(T_nd);
                        else if (pcol != col)
                            MOVE(line, col);

                        if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT))
                        {
                            Terminal::writestr(T_so);
                            mode |= S_STANDOUT;
                        }
                        if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE))
                        {
                            Terminal::writestr(T_us);
                            mode |= S_UNDERLINE;
                        }
                        if ((pr[col] & S_BOLD) && !(mode & S_BOLD))
                        {
                            Terminal::writestr(T_md);
                            mode |= S_BOLD;
                        }
                        if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR)
                        {
                            color = (pr[col] & COL_FCOLOR);
                            mode = ((mode & ~COL_FCOLOR) | color);
                            Terminal::writestr(color_seq(color));
                        }

                        if ((pr[col] & S_BCOLORED) && (pr[col] ^ mode) & COL_BCOLOR)
                        {
                            bcolor = (pr[col] & COL_BCOLOR);
                            mode = ((mode & ~COL_BCOLOR) | bcolor);
                            Terminal::writestr(bcolor_seq(bcolor));
                        }

                        if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS))
                        {
                            writer.end();
                            if (!graph_enabled)
                            {
                                graph_enabled = 1;
                                Terminal::writestr(T_eA);
                            }
                            Terminal::writestr(T_as);
                            mode |= S_GRAPHICS;
                        }

                        if (pr[col] & S_GRAPHICS)
                            Terminal::write1(graphchar(*pc[col].bytes.data()));
                        else if (CHMODE(pr[col]) != C_WCHAR2)
                            writer.putc(pc[col]);
                        pcol = col + 1;
                    }
                }
                if (col == cols)
                    moved = RF_NEED_TO_MOVE;
                for (; col < cols && !(pr[col] & S_EOL); col++)
                    pr[col] |= S_EOL;
            }
            *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
            if (mode & M_MEND)
            {
                if (mode & (S_COLORED | S_BCOLORED))
                {
                    Terminal::writestr(T_op);
                }
                if (mode & S_GRAPHICS)
                {
                    Terminal::writestr(T_ae);
                    writer.clear_status();
                }
                Terminal::writestr(T_me);
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

void Screen::Refresh()
{
    m_impl->Refresh();
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
