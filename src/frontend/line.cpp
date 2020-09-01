
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
    return buffer.calcPosition(c, 0, CP_AUTO);
}

void Line::CalcWidth(bool force)
{
    if (force || m_width < 0)
    {
        m_width = COLPOS(len());
    }
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
