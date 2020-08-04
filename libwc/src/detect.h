#pragma once
#include "ces.h"
#include <stdint.h>
extern uint8_t WC_DETECT_MAP[];

CharacterEncodingScheme wc_auto_detect(char *is, size_t len, CharacterEncodingScheme hint);
void wc_create_detect_map(CharacterEncodingScheme ces, bool esc);
