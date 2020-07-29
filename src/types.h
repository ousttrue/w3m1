#pragma once
#include <gc_cpp.h>
#include <string>
#include <list>

#include "wc.h"
#include "wtf.h"
#include "wc_types.h"
#include "ucs.h"
#include "Str.h"
#include "textlist.h"
#include "transport/istream.h"
#include "transport/url.h"
#include "transport/urlfile.h"
#include "bufferpoint.h"
#include "html/anchor.h"
#include "frontend/line.h"


struct MapList
{
    Str name;
    GeneralList *area;
    MapList *next;
};

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
struct LinkList
{
    char *url;
    char *title; /* Next, Contents, ... */
    char *ctype; /* Content-Type */
    char type;   /* Rel, Rev */
    LinkList *next;
};


struct ImageCache
{
    char *url;
    ParsedURL *current;
    char *file;
    char *touch;
    pid_t pid;
    char loaded;
    int index;
    short width;
    short height;
};

struct Image
{
    char *url;
    const char *ext;
    short width;
    short height;
    short xoffset;
    short yoffset;
    short y;
    short rows;
    char *map;
    char ismap;
    int touch;
    ImageCache *cache;
};

struct FormSelectOptionItem
{
    Str value;
    Str label;
    int checked;
    FormSelectOptionItem *next;
};


using Command = void (*)();

struct AlarmEvent
{
    int sec;
    short status;
    Command cmd;
    void *data;
};

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
