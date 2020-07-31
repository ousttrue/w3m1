#pragma once
#include "Str.h"
#include "ces.h"

void wc_char_conv_init(CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
Str wc_char_conv(char c);
