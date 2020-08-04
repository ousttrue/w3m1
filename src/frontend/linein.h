#pragma once

#include "frontend/line.h"
#include "w3m.h"

/* Flags for inputLine() */
#define IN_STRING 0x10
#define IN_FILENAME 0x20
#define IN_PASSWORD 0x40
#define IN_COMMAND 0x80
#define IN_URL 0x100
#define IN_CHAR 0x200

struct Hist;
char *inputLineHistSearch(const char *prompt, const char *def_str, int flag,
                          Hist *hist, int (*incfunc)(int ch, Str buf, Lineprop *prop));

inline char *inputLineHist(const char *prompt, const char *def_str, int flag, Hist *hist)
{
    return inputLineHistSearch(prompt, def_str, flag, hist, NULL);
}

inline char *inputStrHist(const char *prompt, char *def_str, Hist *hist)
{
    return inputLineHist(prompt, def_str, IN_STRING, hist);
}

inline char *inputFilenameHist(const char *p, char *d, Hist *h)
{
    return inputLineHist(p, d, IN_FILENAME, h);
}

inline char *inputLine(const char *prompt, const char *def_str, int flag)
{
    return inputLineHist(prompt, def_str, flag, NULL);
}

#define inputStr(p, d) inputLine(p, d, IN_STRING)
#define inputFilename(p, d) inputLine(p, d, IN_FILENAME)
#define inputChar(p) inputLine(p, "", IN_CHAR)
