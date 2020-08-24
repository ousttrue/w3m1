#pragma once

#include "frontend/line.h"

/* Flags for inputLine() */
enum LineInputFlags
{
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

struct Hist;
using IncFunc = int (*)(int ch, Str buf, Lineprop *prop, int prec_num);
char *inputLineHistSearch(const char *prompt, const char *def_str, LineInputFlags flag, Hist *hist, IncFunc incfunc, int prec_num);

inline char *inputLineHist(const char *prompt, const char *def_str, LineInputFlags flag, Hist *hist, int prec_num = 1)
{
    return inputLineHistSearch(prompt, def_str, flag, hist, NULL, prec_num);
}

inline char *inputStrHist(const char *prompt, const char *def_str, Hist *hist, int prec_num = 1)
{
    return inputLineHist(prompt, def_str, IN_STRING, hist, prec_num);
}

inline char *inputFilenameHist(const char *p, const char *d, Hist *h, int prec_num = 1)
{
    return inputLineHist(p, d, IN_FILENAME, h, prec_num);
}

inline char *inputLine(const char *prompt, const char *def_str, LineInputFlags flag, int prec_num = 1)
{
    return inputLineHist(prompt, def_str, flag, NULL, prec_num);
}

inline char *inputStr(const char *p, const char *d, int prec_num = 1)
{
    return inputLine(p, d, IN_STRING, prec_num);
}

inline char *inputFilename(const char *p, const char *d, int prec_num = 1)
{
    return inputLine(p, d, IN_FILENAME, prec_num);
}

inline char *inputChar(const char *p, int prec_num = 1)
{
    return inputLine(p, "", IN_CHAR, prec_num);
}
