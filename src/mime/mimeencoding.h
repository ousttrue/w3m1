#pragma once
#include "Str.h"


Str decodeB(char **ww);
Str decodeQ(char **ww);
Str decodeQP(char **ww);
Str decodeU(char **ww);
Str decodeWord(char **ow, CharacterEncodingScheme *charset);
Str decodeMIME(Str orgstr, CharacterEncodingScheme *charset);
Str encodeB(char *a);
