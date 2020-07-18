#pragma once
#include "anchor.h"
#include "line.h"
#include "bufferpoint.h"

typedef struct
{
    BufferPoint *marks;
    int nmark;
    int markmax;
    int prevhseq;
} HmarkerList;

typedef struct _MapList
{
    Str name;
    GeneralList *area;
    struct _MapList *next;
} MapList;

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
#ifdef USE_M17N
    wc_ces document_charset;
    wc_uint8 auto_detect;
#endif
    TextList *document_header;
    FormItemList *form_submit;
    char *savecache;
    char *edit;
    struct mailcap *mailcap;
    char *mailcap_source;
    char *header_source;
    char search_header;
#ifdef USE_SSL
    char *ssl_certificate;
#endif
    char image_flag;
    char image_loaded;
    char need_reshape;
    Anchor *submit;
    struct _BufferPos *undo;
#ifdef USE_ALARM
    struct _AlarmEvent *event;
#endif
} Buffer;

void deleteImage(Buffer *buf);
void getAllImage(Buffer *buf);
void loadImage(Buffer *buf, int flag);
Anchor *registerForm(Buffer *buf, FormList *flist, struct parsed_tag *tag, int line, int pos);
