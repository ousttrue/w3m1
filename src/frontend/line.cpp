#include "fm.h"
#include "line.h"
#include "gc_helper.h"
#include "file.h"
#include "wtf.h"

static int nextColumn(int n, char *p, Lineprop *pr)
{
    if (*pr & PC_CTRL)
    {
        if (*p == '\t')
            return (n + w3mApp::Instance().Tabstop) / w3mApp::Instance().Tabstop * w3mApp::Instance().Tabstop;
        else if (*p == '\n')
            return n + 1;
        else if (*p != '\r')
            return n + 2;
        return n;
    }
    if (*pr & PC_UNKNOWN)
        return n + 4;
    return n + wtf_width((uint8_t *)p);
}

int columnLen(LinePtr line, int column)
{
    int i, j;

    for (i = 0, j = 0; i < line->len;)
    {
        j = nextColumn(j, &line->lineBuf[i], &line->propBuf[i]);
        if (j > column)
            return i;
        i++;
#ifdef USE_M17N
        while (i < line->len && line->propBuf[i] & PC_WCHAR2)
            i++;
#endif
    }
    return line->len;
}

// text position to column position ?
int Line::COLPOS(int c)
{
    return calcPosition(lineBuf, propBuf, len, c, 0, CP_AUTO);
}

void Line::CalcWidth()
{
    width = COLPOS(len);
}

int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos, CalcPositionMode mode)
{
    static int *realColumn = nullptr;
    static int size = 0;
    static char *prevl = nullptr;
    int i, j;

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
        j = nextColumn(j, &l[i], &pr[i]);
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

    for (i = 1; i < line->len; i++)
    {
        if (line->COLPOS(i) > column)
            break;
    }
    for (i--; i > 0 && line->propBuf[i] & PC_WCHAR2; i--)
        ;
    return i;
}

int Buffer::ColumnSkip(int offset)
{
    int i, maxColumn;
    int column = this->currentColumn + offset;
    int nlines = this->LINES + 1;

    maxColumn = 0;
    auto l = find(topLine);
    for (i = 0; i < nlines && l!=lines.end(); i++, ++l)
    {
        if ((*l)->width < 0)
            (*l)->CalcWidth();
        if ((*l)->width - 1 > maxColumn)
            maxColumn = (*l)->width - 1;
    }
    maxColumn -= this->COLS - 1;
    if (column < maxColumn)
        maxColumn = column;
    if (maxColumn < 0)
        maxColumn = 0;

    if (this->currentColumn == maxColumn)
        return 0;
    this->currentColumn = maxColumn;
    return 1;
}
