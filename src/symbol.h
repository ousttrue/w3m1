#pragma once
#include <wc.h>

extern const char *graph_symbol[];
extern const char *graph2_symbol[];
#define N_GRAPH_SYMBOL 32

#define SYMBOL_BASE 0x20

enum GraphicCharTypes
{
    GRAPHIC_CHAR_ASCII = 2,
    GRAPHIC_CHAR_DEC = 1,
    GRAPHIC_CHAR_CHARSET = 0,
};

const char **get_symbol(CharacterEncodingScheme charset, int *width);

std::string_view  get_width_symbol(int width, char symbol);

void push_symbol(Str str, char symbol, int width, int n);
