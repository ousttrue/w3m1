#pragma once
#include <wc.h>
#include <gc_cpp.h>

#define LINELEN 256 /* Initial line length */

using Linecolor = unsigned char;
using Lineprop = unsigned short;

/*
 * Line Property
 */
enum Lineproperties
{
    P_UNKNOWN = 0,

    P_CHARTYPE = 0x3f00,
    PC_ASCII = (WTF_TYPE_ASCII << 8),
    PC_CTRL = (WTF_TYPE_CTRL << 8),
    PC_WCHAR1 = (WTF_TYPE_WCHAR1 << 8),
    PC_WCHAR2 = (WTF_TYPE_WCHAR2 << 8),
    PC_KANJI = (WTF_TYPE_WIDE << 8),
    PC_KANJI1 = (PC_WCHAR1 | PC_KANJI),
    PC_KANJI2 = (PC_WCHAR2 | PC_KANJI),
    PC_UNKNOWN = (WTF_TYPE_UNKNOWN << 8),
    PC_UNDEF = (WTF_TYPE_UNDEF << 8),
    PC_SYMBOL = 0x8000,

    /* Effect ( standout/underline ) */
    P_EFFECT = 0x40ff,
    PE_NORMAL = 0x00,
    PE_MARK = 0x01,
    PE_UNDER = 0x02,
    PE_STAND = 0x04,
    PE_BOLD = 0x08,
    PE_ANCHOR = 0x10,
    PE_EMPH = 0x08,
    PE_IMAGE = 0x20,
    PE_FORM = 0x40,
    PE_ACTIVE = 0x80,
    PE_VISITED = 0x4000,

    /* Extra effect */
    PE_EX_ITALIC = 0x01,
    PE_EX_INSERT = 0x02,
    PE_EX_STRIKE = 0x04,

    PE_EX_ITALIC_E = PE_UNDER,
    PE_EX_INSERT_E = PE_UNDER,
    PE_EX_STRIKE_E = PE_STAND,
};

inline Lineprop get_mctype(int c)
{
    return (Lineprop)(wtf_type((uint8_t *)&c) << 8);
}

inline Lineprop CharType(int c)
{
    return (Lineprop)((c)&P_CHARTYPE);
}

inline Lineprop CharEffect(int c)
{
    return (Lineprop)((c) & (P_EFFECT | PC_SYMBOL));
}

inline void SetCharType(Lineprop &v, int c)
{
    ((v) = (Lineprop)(((v) & ~P_CHARTYPE) | (c)));
}

struct PropertiedCharacter
{
    const char *head;
    Lineprop prop;

    int ColumnLen() const;
};

struct PropertiedString
{
    char *lineBuf = nullptr;
    Lineprop *propBuf = nullptr;
    int len = 0;
    Linecolor *colorBuf = nullptr;

    PropertiedCharacter Get(int index) const
    {
        return {lineBuf + index, propBuf[index]};
    }
};

struct Line : gc_cleanup
{
    PropertiedString buffer;
    char *lineBuf()
    {
        return buffer.lineBuf;
    }
    Lineprop *propBuf()
    {
        return buffer.propBuf;
    }
    Linecolor *colorBuf()
    {
        return buffer.colorBuf;
    }
    int len() const
    {
        return buffer.len;
    }

    bool m_destroy = false;

    Line()
    {
    }

    Line(const PropertiedString str)
    {
        buffer = str;

        m_width = -1;
        bpos = 0;
        bwidth = 0;
    }

    ~Line()
    {
        m_destroy = true;
    }

    // separate start bytes
    int bpos = 0;
    // separate start column
    int bwidth = 0;

private:
    // sepate column size
    int m_width = -1;

public:
    int width()const
    {
        return m_width;
    }
    int bend() const
    {
        return bwidth + m_width;
    }

    long linenumber = 0;      /* on buffer */
    long real_linenumber = 0; /* on file */
    unsigned short usrflags = 0;

public:
    void CalcWidth(bool force = false);
    int COLPOS(int c);
};
using LinePtr = Line *;

/* Flags for calcPosition() */
enum CalcPositionMode
{
    CP_AUTO = 0,
    CP_FORCE = 1,
};
int calcPosition(const PropertiedString &str, int pos, int bpos, CalcPositionMode mode);
int columnPos(LinePtr line, int column);
int columnLen(LinePtr line, int column);
