#pragma once
#include <stdint.h>
#include <stddef.h>

struct wc_map
{
    uint16_t code;
    uint16_t code2;
};

struct wc_map3
{
    uint16_t code;
    uint16_t code2;
    uint16_t code3;
};

wc_map  *wc_map_search(uint16_t code, wc_map *map, size_t n);
wc_map3 *wc_map3_search(uint16_t c1, uint16_t c2, wc_map3 *map, size_t n);
wc_map  *wc_map_range_search(uint16_t code, wc_map *map, size_t n);
wc_map  *wc_map2_range_search(uint16_t code, wc_map *map, size_t n);
wc_map3 *wc_map3_range_search(uint16_t code, wc_map3 *map, size_t n);
