
#include "line.h"
#include "gc_helper.h"
#include "file.h"
#include "wtf.h"
#include "display.h"

int columnLen(LinePtr line, int column)
{
    int i, j;

    for (i = 0, j = 0; i < line->len();)
    {
        auto colLen = line->buffer.Get(i).ColumnLen();
        j += colLen;
        if (j > column)
            return i;
        i++;

        while (i < line->len() && line->propBuf()[i] & PC_WCHAR2)
            i++;
    }
    return line->len();
}

// text position to column position ?
int Line::COLPOS(int c)
{
    return calcPosition(buffer, c, 0, CP_AUTO);
}

void Line::CalcWidth(bool force)
{
    if (force || m_width < 0)
    {
        m_width = COLPOS(len());
    }
}

int calcPosition(const PropertiedString &str, int pos, int bpos, CalcPositionMode mode)
{
    static int *realColumn = nullptr;
    static int size = 0;
    static char *prevl = nullptr;
    int i, j;

    auto l = const_cast<char *>(str.lineBuf());
    auto pr = str.propBuf();
    auto len = str.len();

    if (l == nullptr || len == 0)
        return bpos;
    if (l == prevl && mode == CP_AUTO)
    {
        if (pos <= len)
            return realColumn[pos];
    }
    if (size < len + 1)
    {
        size = (len + 1 > LINELEN) ? (len + 1) : LINELEN;
        realColumn = New_N(int, size);
    }
    prevl = l;
    i = 0;
    j = bpos;
    if (pr[i] & PC_WCHAR2)
    {
        for (; i < len && pr[i] & PC_WCHAR2; i++)
            realColumn[i] = j;
        if (i > 0 && pr[i - 1] & PC_KANJI && WcOption.use_wide)
            j++;
    }
    while (1)
    {
        realColumn[i] = j;
        if (i == len)
            break;
        j += PropertiedCharacter{l + i, pr[i]}.ColumnLen();
        i++;
        for (; i < len && pr[i] & PC_WCHAR2; i++)
            realColumn[i] = realColumn[i - 1];
    }
    if (pos >= i)
        return j;
    return realColumn[pos];
}

int columnPos(LinePtr line, int column)
{
    int i;

    for (i = 1; i < line->len(); i++)
    {
        if (line->COLPOS(i) > column)
            break;
    }
    for (i--; i > 0 && line->propBuf()[i] & PC_WCHAR2; i--)
        ;
    return i;
}

void Line::clear_mark()
{
    for (int pos = 0; pos < len(); pos++)
        propBuf()[pos] &= ~PE_MARK;
}
