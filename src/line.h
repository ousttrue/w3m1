#pragma once

typedef unsigned short Lineprop;
typedef unsigned char Linecolor;

typedef struct _Line
{
    char *lineBuf;
    Lineprop *propBuf;
    Linecolor *colorBuf;
    struct _Line *next;
    struct _Line *prev;
    int len;
    int width;
    long linenumber;      /* on buffer */
    long real_linenumber; /* on file */
    unsigned short usrflags;
    int size;
    int bpos;
    int bwidth;
} Line;
