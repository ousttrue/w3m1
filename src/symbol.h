#pragma once

const char **get_symbol(CharacterEncodingScheme charset, int *width);
char **set_symbol(int width);
extern void push_symbol(Str str, char symbol, int width, int n);
extern void update_utf8_symbol(void);
