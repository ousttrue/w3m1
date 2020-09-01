#pragma once
#include "propstring.h"
#include <memory>

struct Line
{
    PropertiedString buffer;

    // on buffer
    // 1 origin ?
    long linenumber = 0;
    // on file
    long real_linenumber = 0;

    unsigned short usrflags = 0;

private:
    int m_width = -1;

public:
    Line()
    {
    }

    Line(const PropertiedString str)
    {
        buffer = str;
        m_width = -1;
    }

    ~Line()
    {
    }

    char *lineBuf()
    {
        return const_cast<char *>(buffer.lineBuf());
    }
    Lineprop *propBuf()
    {
        return const_cast<Lineprop *>(buffer.propBuf());
    }
    Linecolor *colorBuf()
    {
        return const_cast<Linecolor *>(buffer.colorBuf());
    }
    int len() const
    {
        return buffer.len();
    }

    int width() const
    {
        return m_width;
    }

    void CalcWidth(bool force = false);

    void clear_mark();
};
using LinePtr = std::shared_ptr<Line>;

int columnPos(LinePtr line, int column);
int columnLen(LinePtr line, int column);
