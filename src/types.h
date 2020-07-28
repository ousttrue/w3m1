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
#include "line.h"


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


struct HmarkerList
{
    BufferPoint *marks;
    int nmark;
    int markmax;
};

struct FormSelectOptionItem
{
    Str value;
    Str label;
    int checked;
    FormSelectOptionItem *next;
};

struct FormItemList
{
    int type;
    Str name;
    Str value, init_value;
    int checked, init_checked;
    int accept;
    int size;
    int rows;
    int maxlength;
    int readonly;
    FormSelectOptionItem *select_option;
    Str label, init_label;
    int selected, init_selected;
    struct FormList *parent;
    FormItemList *next;
};

struct FormList
{
    FormItemList *item;
    FormItemList *lastitem;
    int method;
    Str action;
    char *target;
    char *name;
    wc_ces charset;
    int enctype;
    FormList *next;
    int nitems;
    char *body;
    char *boundary;
    unsigned long length;
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

#define RB_STACK_SIZE 10
#define TAG_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define FONTSTAT_SIZE 7

struct Breakpoint
{
    int pos;
    int len;
    int tlen;
    long flag;
    Anchor anchor;
    Str img_alt;
    char fontstat[FONTSTAT_SIZE];
    short nobr_level;
    Lineprop prev_ctype;
    char init_flag;
    short top_margin;
    short bottom_margin;
};

struct cmdtable
{
    char *cmdname;
    int cmd;
};

struct readbuffer
{
    Str line;
    Lineprop cprop;
    short pos;
    Str prevchar;
    long flag;
    long flag_stack[RB_STACK_SIZE];
    int flag_sp;
    int status;
    unsigned char end_tag;
    short table_level;
    short nobr_level;
    Anchor anchor;
    Str img_alt;
    char fontstat[FONTSTAT_SIZE];
    char fontstat_stack[FONT_STACK_SIZE][FONTSTAT_SIZE];
    int fontstat_sp;
    Lineprop prev_ctype;
    Breakpoint bp;
    struct cmdtable *tag_stack[TAG_STACK_SIZE];
    int tag_sp;
    short top_margin;
    short bottom_margin;
};

struct environment
{
    unsigned char env;
    int type;
    int count;
    char indent;
};

struct html_feed_environ
{
    struct readbuffer *obuf;
    TextLineList *buf;
    FILE *f;
    Str tagbuf;
    int limit;
    int maxlimit;
    struct environment *envs;
    int nenv;
    int envc;
    int envc_real;
    char *title;
    int blank_lines;
};

struct HtmlContext
{
    ParsedURL *BaseUrl = nullptr;
    Str Title = nullptr;
    wc_ces MetaCharset;
    int HSeq = 0;
};
