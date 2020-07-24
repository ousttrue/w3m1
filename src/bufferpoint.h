#pragma once

struct BufferPoint
{
    int line;
    int pos;
    int invalid;

    bool operator==(const BufferPoint &rhs) const
    {
        return Cmp(rhs) == 0;
    }

    int Cmp(const BufferPoint &b) const
    {
        return ((line - (b).line) ? (line - (b).line) : (pos - (b).pos));
    }
};
