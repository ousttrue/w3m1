#pragma once
#include "propstring.h"
#include <memory>

struct Line
{
    PropertiedString buffer;
    // separate start column
    int bwidth = 0;

    // on buffer
    // 1 origin ?
    long linenumber = 0;
    // on file
    long real_linenumber = 0;

    unsigned short usrflags = 0;

private:
    // sepate column size
    int m_width = -1;

public:
    Line()
    {
    }

    Line(const PropertiedString str)
    {
        buffer = str;

        m_width = -1;
        bwidth = 0;
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
    int bend() const
    {
        return bwidth + m_width;
    }

    void CalcWidth(bool force = false);
    int COLPOS(int c);

    void clear_mark();
};
using LinePtr = std::shared_ptr<Line>;

/* Flags for calcPosition() */
enum CalcPositionMode
{
    CP_AUTO = 0,
    CP_FORCE = 1,
};
int calcPosition(const PropertiedString &str, int pos, int bpos, CalcPositionMode mode);
int columnPos(LinePtr line, int column);
int columnLen(LinePtr line, int column);
