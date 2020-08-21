#pragma once

#include "frontend/line.h"

/* Flags for inputLine() */
enum LineInputFlags {
    IN_STRING= 0x10,
    IN_FILENAME= 0x20,
    IN_PASSWORD= 0x40,
    IN_COMMAND= 0x80,
    IN_URL= 0x100,
    IN_CHAR= 0x200,
};

struct Hist;
using IncFunc = int (*)(int ch, Str buf, Lineprop *prop);
char *inputLineHistSearch(const char *prompt, const char *def_str, LineInputFlags flag, Hist *hist, IncFunc incfunc);

inline char *inputLineHist(const char *prompt, const char *def_str, LineInputFlags flag, Hist *hist)
{
    return inputLineHistSearch(prompt, def_str, flag, hist, NULL);
}

inline char *inputStrHist(const char *prompt, const char *def_str, Hist *hist)
{
    return inputLineHist(prompt, def_str, IN_STRING, hist);
}

inline char *inputFilenameHist(const char *p, const char *d, Hist *h)
{
    return inputLineHist(p, d, IN_FILENAME, h);
}

inline char *inputLine(const char *prompt, const char *def_str, LineInputFlags flag)
{
    return inputLineHist(prompt, def_str, flag, NULL);
}

inline char* inputStr(const char *p, const char *d)
{
    return inputLine(p, d, IN_STRING);
}

inline char* inputFilename(const char *p, const char *d)
{
    return inputLine(p, d, IN_FILENAME);
}

inline char* inputChar(const char *p)
{
    return inputLine(p, "", IN_CHAR);
}
