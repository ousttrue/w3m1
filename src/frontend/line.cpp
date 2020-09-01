
#include "line.h"
#include "gc_helper.h"
#include "file.h"
#include "wtf.h"
#include "display.h"

void Line::CalcWidth(bool force)
{
    if (force || m_width < 0)
    {
        m_width = buffer.BytePositionToColumns(buffer.len());
    }
}
