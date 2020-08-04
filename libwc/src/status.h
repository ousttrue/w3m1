#pragma once
#include <stdint.h>
#include "ces_info.h"
#include "search.h"

typedef wc_wchar_t (*WcConvFunc)(CodedCharacterSet, uint16_t);

struct wc_table
{
    CodedCharacterSet ccs;
    size_t n;
    wc_map *map;
    WcConvFunc conv;
};

struct wc_status
{
    const wc_ces_info *ces_info;
    uint8_t gr;
    uint8_t gl;
    uint8_t ss;
    CodedCharacterSet g0_ccs;
    CodedCharacterSet g1_ccs;
    CodedCharacterSet design[4];

    wc_table **tlist;
    wc_table **tlistw;

    int state;

    Str tag;
    int ntag;
    uint32_t base;
    int shift;
};

void wc_input_init(CharacterEncodingScheme ces, wc_status *st);
void wc_output_init(CharacterEncodingScheme ces, wc_status *st);
