#pragma once
#include "Str.h"
#include "frontend/line.h"

struct Hist;
char *inputLineHistSearch(const char* prompt, const char *def_str, int flag,
                          Hist *hist, int (*incfunc)(int ch, Str buf, Lineprop *prop));
