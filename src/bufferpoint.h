#pragma once
#include <stdint.h>

struct BufferPoint
{
    int64_t line = 0;
    int pos = 0;
    int invalid = 0;

    bool operator==(const BufferPoint &rhs) const
    {
        return Cmp(rhs) == 0;
    }

    int Cmp(const BufferPoint &b) const
    {
        return ((line - (b).line) ? (line - (b).line) : (pos - (b).pos));
    }
};
