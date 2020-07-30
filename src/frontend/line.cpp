#include "fm.h"
#include "line.h"
#include "indep.h"
#include "file.h"

static int nextColumn(int n, char *p, Lineprop *pr)
{
    if (*pr & PC_CTRL)
    {
        if (*p == '\t')
            return (n + Tabstop) / Tabstop * Tabstop;
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

int columnLen(Line *line, int column)
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

int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos, int mode)
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

Line *
currentLineSkip(BufferPtr buf, Line *line, int offset, int last)
{
    int i, n;
    Line *l = line;

    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE))
    {
        n = line->linenumber + offset + buf->LINES;
        if (buf->lastLine->linenumber < n)
            getNextPage(buf, n - buf->lastLine->linenumber);
        while ((last || (buf->lastLine->linenumber < n)) &&
               (getNextPage(buf, 1) != NULL))
            ;
        if (last)
            l = buf->lastLine;
    }

    if (offset == 0)
        return l;
    if (offset > 0)
        for (i = 0; i < offset && l->next != NULL; i++, l = l->next)
            ;
    else
        for (i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
            ;
    return l;
}

Line *
lineSkip(BufferPtr buf, Line *line, int offset, int last)
{
    int i;
    Line *l;

    l = currentLineSkip(buf, line, offset, last);
    if (!nextpage_topline)
        for (i = buf->LINES - 1 - (buf->lastLine->linenumber - l->linenumber);
             i > 0 && l->prev != NULL; i--, l = l->prev)
            ;
    return l;
}
