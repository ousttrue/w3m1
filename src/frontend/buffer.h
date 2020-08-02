#pragma once
#include "transport/url.h"
#include "html/anchor.h"
#include "line.h"
#include "link.h"
#include "option.h"
#include <stdint.h>
#include <assert.h>

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

    char need_reshape;

    // private:
    // scroll
    Line *topLine;
    // cursor ?
    Line *currentLine;

public:
    // private:
    // list
    Line *firstLine;
    Line *lastLine;

private:
    int allLine = 0;

public:
    int LineCount() const
    {
        if (!firstLine)
        {
            return 0;
        }
        // linenumber is 1 origin ?
        // assert(allLine == (lastLine ? lastLine->linenumber : 0));
        return allLine;
    }
    void AddLine(char *line, Lineprop *prop, Linecolor *color, int pos, int nlines);
    void ClearLines()
    {
        firstLine = topLine = currentLine = lastLine = NULL;
        allLine = 0;
    }
    void GotoLine(int n);
    void GotoRealLine(int n);
    void Reshape();
    Line *CurrentLineSkip(Line *line, int offset, int last);
    void LineSkip(Line *line, int offset, int last);
    void Scroll(int n);
    void NScroll(int n);
    void CurrentAsLast()
    {
        lastLine = currentLine;
        topLine = firstLine;
        currentLine = firstLine;
    }
    void EachLine(const std::function<void(Line *)> &func)
    {
        for (auto l = firstLine; l; l = l->next)
        {
            func(l);
        }
    }

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
    void CursorHome()
    {
        visualpos = 0;
        cursorX = 0;
        cursorY = 0;
    }
    void CursorXY(int x, int y);
    void CursorUp0(int n);
    void CursorUp(int n);
    void CursorDown0(int n);
    void CursorDown(int n);
    void CursorUpDown(int n);
    void CursorRight(int n);
    void CursorLeft(int n);
    void ArrangeLine();
    void ArrangeCursor();
    int ColumnSkip(int offset);

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
    void SavePosition();
    void DumpSource();
};

#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

BufferPtr newBuffer(int width);

void deleteImage(BufferPtr buf);
void getAllImage(BufferPtr buf);

#define IMG_FLAG_START 0
#define IMG_FLAG_STOP 1
#define IMG_FLAG_NEXT 2
void loadImage(BufferPtr buf, int flag);

BufferPtr nullBuffer(void);

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
