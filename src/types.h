#pragma once
#include "Str.h"
#include "textlist.h"
#include "istream.h"
#include "wc.h"
#include "wtf.h"
#include "wc_types.h"
#include "ucs.h"
#include "bufferpoint.h"
#include "anchor.h"

#include <gc_cpp.h>
#include <string>
#include <list>
#include "url.h"
#include "urlfile.h"


struct portlist
{
    unsigned short port;
    portlist *next;
};

struct cookie
{
    ParsedURL url;
    Str name;
    Str value;
    time_t expires;
    Str path;
    Str domain;
    Str comment;
    Str commentURL;
    portlist *portl;
    char version;
    char flag;
    cookie *next;
};

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

using Lineprop = unsigned short;
using Linecolor = unsigned char;

struct Line
{
    char *lineBuf;
    Lineprop *propBuf;
    Linecolor *colorBuf;
    Line *next;
    Line *prev;
    int len;
    int width;
    int linenumber;      /* on buffer */
    int real_linenumber; /* on file */
    unsigned short usrflags;
    int size;
    int bpos;
    int bwidth;

    void CalcWidth();
    int COLPOS(int c);
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

struct BufferPos
{
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    BufferPos *next;
    BufferPos *prev;
};

enum LinkBufferTypes
{
    LB_NOLINK = -1,
    LB_FRAME = 0, /* rFrame() */
    LB_N_FRAME = 1,
    LB_INFO = 2, /* pginfo() */
    LB_N_INFO = 3,
    LB_SOURCE = 4, /* vwSrc() */
    LB_N_SOURCE = LB_SOURCE
};
const int MAX_LB = 5;

enum BufferProps
{
    BP_NORMAL = 0x0,
    BP_PIPE = 0x1,
    BP_FRAME = 0x2,
    BP_INTERNAL = 0x8,
    BP_NO_URL = 0x10,
    BP_REDIRECTED = 0x20,
    BP_CLOSE = 0x40,
};

using BufferPtr = struct Buffer *;
struct Buffer : gc_cleanup
{
    char *filename;
    std::string buffername;
    Line *firstLine;
    Line *topLine;
    Line *currentLine;
    Line *lastLine;
    BufferPtr nextBuffer;
    BufferPtr linkBuffer[MAX_LB];
    short width;
    short height;
    char *type;
    const char *real_type;
    int allLine;
    short bufferprop;
    int currentColumn;
    short cursorX;
    short cursorY;
    int pos;
    int visualpos;
    short rootX;
    short rootY;
    short COLS;
    short LINES;
    InputStream *pagerSource;
    AnchorList href;
    AnchorList name;
    AnchorList img;
    AnchorList formitem;
    int prevhseq = -1;

    LinkList *linklist;
    FormList *formlist;
    MapList *maplist;
    HmarkerList *hmarklist;
    HmarkerList *imarklist;
    ParsedURL currentURL;
    ParsedURL *baseURL;
    char *baseTarget;
    int real_scheme;
    char *sourcefile;
    struct frameset *frameset;
    struct frameset_queue *frameQ;
    int *clone;
    size_t trbyte;
    char check_url;
    wc_ces document_charset;
    uint8_t auto_detect;
    TextList *document_header;
    FormItemList *form_submit;
    char *savecache;
    char *edit;
    struct mailcap *mailcap;
    char *mailcap_source;
    char *header_source;
    char search_header;
    char *ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    Anchor *submit;
    BufferPos *undo;
    AlarmEvent *event;

    Buffer();
    ~Buffer();
    BufferPtr Copy();
    void CopyFrom(BufferPtr src);
    void ClearLink();
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
