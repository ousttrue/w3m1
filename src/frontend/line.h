#pragma once

using Lineprop = unsigned short;
using Linecolor = unsigned char;

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
