#pragma once
#include <wc.h>
#include <gc_cpp.h>

#define LINELEN 256          /* Initial line length */

using Lineprop = unsigned short;
using Linecolor = unsigned char;

inline Lineprop get_mctype(int c)
{
    return (Lineprop)(wtf_type((uint8_t *)&c) << 8);
}

/*
 * Line Property
 */
enum LineProperties {
    P_CHARTYPE= 0x3f00,
    PC_ASCII= (WTF_TYPE_ASCII << 8),
    PC_CTRL= (WTF_TYPE_CTRL << 8),
    PC_WCHAR1= (WTF_TYPE_WCHAR1 << 8),
    PC_WCHAR2= (WTF_TYPE_WCHAR2 << 8),
    PC_KANJI= (WTF_TYPE_WIDE << 8),
    PC_KANJI1= (PC_WCHAR1 | PC_KANJI),
    PC_KANJI2= (PC_WCHAR2 | PC_KANJI),
    PC_UNKNOWN= (WTF_TYPE_UNKNOWN << 8),
    PC_UNDEF= (WTF_TYPE_UNDEF << 8),
    PC_SYMBOL= 0x8000,

    /* Effect ( standout/underline ) */
    P_EFFECT= 0x40ff,
    PE_NORMAL= 0x00,
    PE_MARK= 0x01,
    PE_UNDER= 0x02,
    PE_STAND= 0x04,
    PE_BOLD= 0x08,
    PE_ANCHOR= 0x10,
    PE_EMPH= 0x08,
    PE_IMAGE= 0x20,
    PE_FORM= 0x40,
    PE_ACTIVE= 0x80,
    PE_VISITED= 0x4000,

    /* Extra effect */
    PE_EX_ITALIC= 0x01,
    PE_EX_INSERT= 0x02,
    PE_EX_STRIKE= 0x04,

    PE_EX_ITALIC_E= PE_UNDER,
    PE_EX_INSERT_E= PE_UNDER,
    PE_EX_STRIKE_E= PE_STAND,
};

inline LineProperties CharType(int c)
{
    return  (LineProperties)((c)&P_CHARTYPE);
}

inline LineProperties CharEffect(int c)
{
    return (LineProperties)((c) & (P_EFFECT | PC_SYMBOL));
}

inline void SetCharType(LineProperties &v, int c) {
    ((v) = (LineProperties)(((v) & ~P_CHARTYPE) | (c)));
}

struct Line : gc_cleanup
{
    char *lineBuf = nullptr;
    Lineprop *propBuf= nullptr;
    Linecolor *colorBuf= nullptr;
    bool m_destroy = false;

    Line()
    {
    }

    Line(char *line, Lineprop *prop, Linecolor *color, int pos)
    {
        lineBuf = line;
        propBuf = prop;
        colorBuf = color;
        len = pos;
        width = -1;
        size = pos;
        bpos = 0;
        bwidth = 0;
    }

    ~Line()
    {
        m_destroy = true;
    }

public:
    int len = 0;
    int width = 0;
    long linenumber = 0;      /* on buffer */
    long real_linenumber =0; /* on file */
    unsigned short usrflags =0;
    int size =0;
    int bpos = 0;
    int bwidth =0;

    void CalcWidth();
    int COLPOS(int c);
};

/* Flags for calcPosition() */
enum CalcPositionMode
{
    CP_AUTO=		0,
    CP_FORCE=	1,
};
int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos, CalcPositionMode mode);
int columnPos(Line *line, int column);
int columnLen(Line *line, int column);
