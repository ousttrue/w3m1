#pragma once
#include <wc.h>

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

struct Line
{
    char *lineBuf;
    Lineprop *propBuf;
    Linecolor *colorBuf;
private:
    friend struct Buffer;
    Line *next;
    Line *prev;
public:
    int len;
    int width;
    long linenumber;      /* on buffer */
    long real_linenumber; /* on file */
    unsigned short usrflags;
    int size;
    int bpos;
    int bwidth;

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
