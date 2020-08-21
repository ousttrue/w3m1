#pragma once
#include <gc_cpp.h>
#include "frontend/buffer.h"

struct GeneralList;
struct Anchor;
struct MapArea : gc_cleanup
{
    const char *url;
    const char *target;
    const char *alt;
    char shape;
    short *coords;
    int ncoords;
    short center_x;
    short center_y;
};

struct MapList : gc_cleanup
{
    Str name;
    std::vector<MapArea *> area;
};

const Anchor *retrieveCurrentMap(const BufferPtr &buf);
MapArea *follow_map_menu(BufferPtr buf, const char *name, const Anchor *a_img, int x, int y);
MapArea *retrieveCurrentMapArea(const BufferPtr &buf);
MapArea *newMapArea(const char *url, const char *target, const char *alt, const char *shape, const char *coords);
MapList *searchMapList(BufferPtr buf, const char *name);
#ifndef MENU_MAP
BufferPtr follow_map_panel(BufferPtr buf, char *name);
#endif
