#pragma once

enum GraphicCharTypes
{
    GRAPHIC_CHAR_ASCII = 2,
    GRAPHIC_CHAR_DEC = 1,
    GRAPHIC_CHAR_CHARSET = 0,
};

const char **get_symbol(CharacterEncodingScheme charset, int *width);
char **set_symbol(int width);
extern void push_symbol(Str str, char symbol, int width, int n);
extern void update_utf8_symbol(void);
