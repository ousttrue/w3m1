#include "types.h"
#include "etc.h"

// text position to column position ?
int Line::COLPOS(int c)
{
    return calcPosition(lineBuf, propBuf, len, c, 0, CP_AUTO);
}

void Line::CalcWidth()
{
    width = COLPOS(len);
}
