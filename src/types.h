#pragma once
#include "Str.h"
#include "textlist.h"
#include "istream.h"

#include "wc.h"
#include "wtf.h"
#include "wc_types.h"
#include "ucs.h"

#define MAX_LB 5

struct ParsedURL
{
    int scheme;
    char *user;
    char *pass;
    char *host;
    int port;
    char *file;
    char *real_file;
    char *query;
    char *label;
    int is_nocache;
};

struct URLFile
{
    unsigned char scheme;
    char is_cgi;
    char encoding;
    InputStream *stream;
    const char *ext;
    int compression;
    int content_encoding;
    const char *guess_type;
    char *ssl_certificate;
    char *url;
    time_t modtime;
};

struct URLOption
{
    char *referer;
    int flag;
};

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
    long linenumber;      /* on buffer */
    long real_linenumber; /* on file */
    unsigned short usrflags;
    int size;
    int bpos;
    int bwidth;

    void CalcWidth();
    int COLPOS(int c);
};

struct BufferPoint
{
    int line;
    int pos;
    int invalid;
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

struct Anchor
{
    char *url;
    char *target;
    char *referer;
    char *title;
    unsigned char accesskey;
    BufferPoint start;
    BufferPoint end;
    int hseq;
    char slave;
    short y;
    short rows;
    Image *image;
};

struct AnchorList
{
    Anchor *anchors;
    int nanchor;
    int anchormax;
    int acache;
};

struct HmarkerList
{
    BufferPoint *marks;
    int nmark;
    int markmax;
    int prevhseq;
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

using BufferPtr = struct Buffer *;
struct Buffer
{
    char *filename;
    char *buffername;
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
    AnchorList *href;
    AnchorList *name;
    AnchorList *img;
    AnchorList *formitem;
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
