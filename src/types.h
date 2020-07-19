#pragma once
#include "Str.h"
#include "textlist.h"
#include "istream.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "wc.h"
#include "wtf.h"
#include "wc_types.h"
#include "ucs.h"
#ifdef __cplusplus
}
#endif

#define MAX_LB 5

typedef struct _ParsedURL
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
} ParsedURL;

typedef struct {
    unsigned char scheme;
    char is_cgi;
    char encoding;
    InputStream stream;
    char *ext;
    int compression;
    int content_encoding;
    char *guess_type;
    char *ssl_certificate;
    char *url;
    time_t modtime;
} URLFile;

typedef struct {
    char *referer;
    int flag;
} URLOption;

struct cookie {
    ParsedURL url;
    Str name;
    Str value;
    time_t expires;
    Str path;
    Str domain;
    Str comment;
    Str commentURL;
    struct portlist *portl;
    char version;
    char flag;
    struct cookie *next;
};

typedef struct _MapList
{
    Str name;
    GeneralList *area;
    struct _MapList *next;
} MapList;

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
typedef struct _LinkList
{
    char *url;
    char *title; /* Next, Contents, ... */
    char *ctype; /* Content-Type */
    char type;   /* Rel, Rev */
    struct _LinkList *next;
} LinkList;

typedef unsigned short Lineprop;
typedef unsigned char Linecolor;

typedef struct _Line
{
    char *lineBuf;
    Lineprop *propBuf;
    Linecolor *colorBuf;
    struct _Line *next;
    struct _Line *prev;
    int len;
    int width;
    long linenumber;      /* on buffer */
    long real_linenumber; /* on file */
    unsigned short usrflags;
    int size;
    int bpos;
    int bwidth;
} Line;

typedef struct
{
    int line;
    int pos;
    int invalid;
} BufferPoint;

typedef struct _imageCache
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
} ImageCache;

typedef struct _image
{
    char *url;
    char *ext;
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
} Image;

typedef struct _anchor
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
} Anchor;

typedef struct _anchorList
{
    Anchor *anchors;
    int nanchor;
    int anchormax;
    int acache;
} AnchorList;

typedef struct
{
    BufferPoint *marks;
    int nmark;
    int markmax;
    int prevhseq;
} HmarkerList;

typedef struct form_list
{
    struct form_item_list *item;
    struct form_item_list *lastitem;
    int method;
    Str action;
    char *target;
    char *name;
    wc_ces charset;
    int enctype;
    struct form_list *next;
    int nitems;
    char *body;
    char *boundary;
    unsigned long length;
} FormList;

typedef struct form_select_option_item {
    Str value;
    Str label;
    int checked;
    struct form_select_option_item *next;
} FormSelectOptionItem;

typedef struct form_item_list {
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
    struct form_list *parent;
    struct form_item_list *next;
} FormItemList;


typedef struct _Buffer
{
    char *filename;
    char *buffername;
    Line *firstLine;
    Line *topLine;
    Line *currentLine;
    Line *lastLine;
    struct _Buffer *nextBuffer;
    struct _Buffer *linkBuffer[MAX_LB];
    short width;
    short height;
    char *type;
    char *real_type;
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
    InputStream pagerSource;
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
    wc_uint8 auto_detect;
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
    struct _BufferPos *undo;
    struct _AlarmEvent *event;
} Buffer;

typedef struct _MapArea
{
    char *url;
    char *target;
    char *alt;
    char shape;
    short *coords;
    int ncoords;
    short center_x;
    short center_y;
} MapArea;
