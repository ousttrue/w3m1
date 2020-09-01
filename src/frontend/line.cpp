
#include "line.h"
#include "gc_helper.h"
#include "file.h"
#include "wtf.h"
#include "display.h"

int Line::columnLen(int column) const
{
    for (int i = 0, j = 0; i < this->len();)
    {
        auto colLen = this->buffer.Get(i).ColumnLen();
        j += colLen;
        if (j > column)
            return i;
        i++;

        while (i < this->len() && this->buffer.propBuf()[i] & PC_WCHAR2)
            i++;
    }
    return this->len();
}

void Line::CalcWidth(bool force)
{
    if (force || m_width < 0)
    {
        m_width = buffer.calcPosition(len());
    }
}

int Line::columnPos(int column) const
{
    int i;
    for (i = 1; i < this->len(); i++)
    {
        if (this->buffer.calcPosition(i) > column)
            break;
    }
    for (i--; i > 0 && this->buffer.propBuf()[i] & PC_WCHAR2; i--)
        ;
    return i;
}

void Line::clear_mark()
{
    for (int pos = 0; pos < len(); pos++)
        propBuf()[pos] &= ~PE_MARK;
}
