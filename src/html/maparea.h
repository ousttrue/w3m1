#pragma once

#include "frontend/buffer.h"

struct GeneralList;
struct Anchor;
struct MapArea
{
    char *url;
    char *target;
    char *alt;
    char shape;
    short *coords;
    int ncoords;
    short center_x;
    short center_y;
};

struct MapList
{
    Str name;
    GeneralList *area;
    MapList *next;
};

const Anchor *retrieveCurrentMap(BufferPtr buf);
MapArea *follow_map_menu(BufferPtr buf, char *name, const Anchor *a_img, int x, int y);
MapArea *retrieveCurrentMapArea(BufferPtr buf);
MapArea *newMapArea(char *url, char *target, char *alt, char *shape, char *coords);
MapList *searchMapList(BufferPtr buf, char *name);
#ifndef MENU_MAP
BufferPtr follow_map_panel(BufferPtr buf, char *name);
#endif
