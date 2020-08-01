#pragma once
#include "transport/url.h"
#include "html/anchor.h"
#include "line.h"
#include "link.h"
#include "option.h"
#include <stdint.h>

struct Line;
union InputStream;
struct FormList;
struct FormItemList;
struct MapList;
struct AlarmEvent;
struct TextList;
struct Mailcap;

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

enum BufferProps : int16_t
{
    BP_NORMAL = 0x0,
    BP_PIPE = 0x1,
    BP_FRAME = 0x2,
    BP_INTERNAL = 0x8,
    BP_NO_URL = 0x10,
    BP_REDIRECTED = 0x20,
    BP_CLOSE = 0x40,
};
#include "enum_bit_operator.h"

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
struct Buffer : gc_cleanup
{
    friend struct Tab;

    std::string filename;
    std::string buffername;

    // list
    Line *firstLine;
    Line *lastLine;

    // scroll
    Line *topLine;
    // cursor ?
    Line *currentLine;

private:
    int allLine = 0;

public:
    int LineCount() const { return allLine; }
    void AddLine(char *line, Lineprop *prop, Linecolor *color, int pos, int nlines);
    void ClearLines()
    {
        firstLine = topLine = currentLine = lastLine = NULL;
        allLine = 0;
    }
    void GotoLine(int n);

    BufferPtr linkBuffer[MAX_LB];
    short width;
    short height;

    // mimetype: text/plain
    std::string type;
    std::string real_type;

public:
    BufferProps bufferprop = BP_NORMAL;
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

    std::vector<Link> linklist;
    FormList *formlist;
    MapList *maplist;
    std::vector<BufferPoint> hmarklist;
    std::vector<BufferPoint> imarklist;
    URL currentURL = {};
    URL baseURL = {};
    std::string baseTarget;
    int real_scheme;
    std::string sourcefile;
    struct frameset *frameset;
    struct frameset_queue *frameQ;
    int *clone;
    size_t trbyte;
    char check_url;
    CharacterEncodingScheme document_charset;
    AutoDetectTypes auto_detect;
    TextList *document_header;
    FormItemList *form_submit;
    // ReadBufferCache, WriteBufferCache
    std::string savecache;
    // editBf
    std::string edit;
    Mailcap *mailcap;
    std::string mailcap_source;
    // backup stream read contents ?
    std::string header_source;
    char search_header;
    // https
    std::string ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    Anchor *submit;
    BufferPos *undo;
    AlarmEvent *event;

    Buffer();
    ~Buffer();
    void TmpClear();
    int WriteBufferCache();
    int ReadBufferCache();
    BufferPtr Copy();
    void CopyFrom(BufferPtr src);
    void ClearLink();
    URL *BaseURL();
    void putHmarker(int line, int pos, int seq);
    void shiftAnchorPosition(AnchorList &al, const BufferPoint &bp, int shift);
};

#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

BufferPtr newBuffer(int width);

void deleteImage(BufferPtr buf);
void getAllImage(BufferPtr buf);
void loadImage(BufferPtr buf, int flag);

BufferPtr nullBuffer(void);
void gotoRealLine(BufferPtr buf, int n);
void reshapeBuffer(BufferPtr buf);

void set_buffer_environ(BufferPtr buf);

// anchor
const Anchor *retrieveCurrentAnchor(BufferPtr buf);
const Anchor *retrieveCurrentImg(BufferPtr buf);
const Anchor *retrieveCurrentForm(BufferPtr buf);
const Anchor *searchURLLabel(BufferPtr buf, char *url);
void reAnchorWord(BufferPtr buf, Line *l, int spos, int epos);
char *reAnchor(BufferPtr buf, char *re);
char *reAnchorNews(BufferPtr buf, char *re);
char *reAnchorNewsheader(BufferPtr buf);
void addMultirowsForm(BufferPtr buf, AnchorList &al);
void addMultirowsImg(BufferPtr buf, AnchorList &al);
char *getAnchorText(BufferPtr buf, AnchorList &al, Anchor *a);

TextList *make_domain_list(char *domain_list);
Line *lineSkip(BufferPtr buf, Line *line, int offset, int last);
Line *currentLineSkip(BufferPtr buf, Line *line, int offset, int last);
int columnSkip(BufferPtr buf, int offset);