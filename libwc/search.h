#pragma once
wc_map  *wc_map_search(wc_uint16 code, wc_map *map, size_t n);
wc_map3 *wc_map3_search(wc_uint16 c1, wc_uint16 c2, wc_map3 *map, size_t n);
wc_map  *wc_map_range_search(wc_uint16 code, wc_map *map, size_t n);
wc_map  *wc_map2_range_search(wc_uint16 code, wc_map *map, size_t n);
wc_map3 *wc_map3_range_search(wc_uint16 code, wc_map3 *map, size_t n);
