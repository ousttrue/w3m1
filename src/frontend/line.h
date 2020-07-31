#pragma once
#include "buffer.h"
#include "wtf.h"

#define LINELEN 256          /* Initial line length */

using Lineprop = unsigned short;
using Linecolor = unsigned char;

/* 
 * Line Property
 */
#define P_CHARTYPE 0x3f00
#define PC_ASCII (WTF_TYPE_ASCII << 8)
#define PC_CTRL (WTF_TYPE_CTRL << 8)
#define PC_WCHAR1 (WTF_TYPE_WCHAR1 << 8)
#define PC_WCHAR2 (WTF_TYPE_WCHAR2 << 8)
#define PC_KANJI (WTF_TYPE_WIDE << 8)
#define PC_KANJI1 (PC_WCHAR1 | PC_KANJI)
#define PC_KANJI2 (PC_WCHAR2 | PC_KANJI)
#define PC_UNKNOWN (WTF_TYPE_UNKNOWN << 8)
#define PC_UNDEF (WTF_TYPE_UNDEF << 8)
#define PC_SYMBOL 0x8000

struct Line
{
    char *lineBuf;
    Lineprop *propBuf;
    Linecolor *colorBuf;
    Line *next;
    Line *prev;
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
#define CP_AUTO		0
#define CP_FORCE	1
int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos, int mode);
int columnPos(Line *line, int column);
int columnLen(Line *line, int column);
