#pragma once
#include "Str.h"
#include "wc_types.h"

Str decodeB(char **ww);
Str decodeQ(char **ww);
Str decodeQP(char **ww);
Str decodeU(char **ww);
Str decodeWord(char **ow, wc_ces *charset);
Str decodeMIME(Str orgstr, wc_ces *charset);
Str encodeB(char *a);
