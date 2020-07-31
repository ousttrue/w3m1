#pragma once
#include "ces.h"
CharacterEncodingScheme wc_auto_detect(char *is, size_t len, CharacterEncodingScheme hint);
void wc_create_detect_map(CharacterEncodingScheme ces, bool esc);
