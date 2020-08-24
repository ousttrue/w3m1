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
char *inputLineHistSearch(std::string_view prompt, std::string_view def_str, LineInputFlags flag, Hist *hist, IncFunc incfunc, int prec_num);

inline char *inputLineHist(std::string_view prompt, std::string_view def_str, LineInputFlags flag, Hist *hist, int prec_num = 1)
{
    return inputLineHistSearch(prompt, def_str, flag, hist, NULL, prec_num);
}

inline char *inputStrHist(std::string_view prompt, std::string_view def_str, Hist *hist, int prec_num = 1)
{
    return inputLineHist(prompt, def_str, IN_STRING, hist, prec_num);
}

inline char *inputFilenameHist(std::string_view p, std::string_view d, Hist *h, int prec_num = 1)
{
    return inputLineHist(p, d, IN_FILENAME, h, prec_num);
}

inline char *inputLine(std::string_view prompt, std::string_view def_str, LineInputFlags flag, int prec_num = 1)
{
    return inputLineHist(prompt, def_str, flag, NULL, prec_num);
}

inline char *inputStr(std::string_view p, std::string_view d, int prec_num = 1)
{
    return inputLine(p, d, IN_STRING, prec_num);
}

inline char *inputFilename(std::string_view p, std::string_view d, int prec_num = 1)
{
    return inputLine(p, d, IN_FILENAME, prec_num);
}

inline char *inputChar(std::string_view p, int prec_num = 1)
{
    return inputLine(p, "", IN_CHAR, prec_num);
}
