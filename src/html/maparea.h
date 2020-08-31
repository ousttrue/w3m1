#pragma once
#include <memory>
#include <vector>
#include <memory>
#include "bufferpoint.h"

enum ShapeTypes
{
    SHAPE_UNKNOWN = 0,
    SHAPE_DEFAULT = 1,
    SHAPE_RECT = 2,
    SHAPE_CIRCLE = 3,
    SHAPE_POLY = 4,
};

struct MapArea
{
    std::string url;
    std::string target;
    std::string alt;
    ShapeTypes shape;
    std::vector<short> coords;
    short center_x;
    short center_y;
};
using MapAreaPtr = std::shared_ptr<MapArea>;

struct MapList
{
    std::string name;
    std::vector<MapAreaPtr> area;
};
using MapListPtr = std::shared_ptr<MapList>;

using BufferPtr = std::shared_ptr<struct Buffer>;
using AnchorPtr = std::shared_ptr<struct Anchor>;
AnchorPtr retrieveCurrentMap(const std::shared_ptr<class Document> &doc, const BufferPoint &point);
MapAreaPtr follow_map_menu(BufferPtr buf, const char *name, const AnchorPtr &a_img, int x, int y);
MapAreaPtr retrieveCurrentMapArea(const BufferPtr &buf);
MapAreaPtr newMapArea(const char *url, const char *target, const char *alt, const char *shape, const char *coords);
MapListPtr searchMapList(BufferPtr buf, const char *name);
BufferPtr follow_map_panel(BufferPtr buf, char *name);
