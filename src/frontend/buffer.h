#pragma once
#include "transport/url.h"
#include "html/anchor.h"
#include "line.h"
#include "link.h"

#include <stdint.h>
#include <assert.h>
#include <vector>

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
using LinePtr = struct Line *;
using LineList = std::vector<LinePtr>;
struct Buffer : gc_cleanup
{
    friend struct Tab;

    std::string filename;
    std::string buffername;

    char need_reshape;

private:
    // scroll
    Line *topLine;
    // cursor ?
    Line *currentLine;

    // private:
    // list
    LineList lines;
    LineList::iterator find(LinePtr l)
    {
        return std::find(lines.begin(), lines.end(), l);
    }
    LineList::const_iterator find(LinePtr l)const
    {
        return std::find(lines.begin(), lines.end(), l);
    }

public:
    int LineCount() const
    {
        return lines.size();
    }
    void AddLine(char *line, Lineprop *prop, Linecolor *color, int pos, int nlines);
    void ClearLines()
    {
        currentLine = topLine = NULL;
        lines.clear();
    }
    Line* FirstLine()const
    {
        if (lines.empty())
        {
            return nullptr;
        }
        return lines.front();
    }
    Line* LastLine()const {
        if (lines.empty())
        {
            return nullptr;
        }
        return lines.back();
    }
    Line* TopLine()const {
        return topLine;
    }
    Line* CurrentLine()const {
        return currentLine;
    }
    void SetFirstLine(Line *line)
    {
        assert(false);
        // firstLine = line;
    }
    void SetLastLine(Line *line)
    {
        assert(false);
    }
    void SetTopLine(Line* line)
    {
        topLine = line;
    }
    void SetCurrentLine(Line *line)
    {
        currentLine = line;
    }
    int TOP_LINENUMBER()const
    {
        return (TopLine() ? TopLine()->linenumber : 1);
    }
    int CUR_LINENUMBER()const
    {
        return (currentLine ? currentLine->linenumber : 1);
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
        auto it = find(currentLine);
        if (it == lines.end())
        {
            assert(false);
            return;
        }
        ++it;
        lines.erase(it, lines.end());
        // lastLine = currentLine;
        topLine = FirstLine();
        currentLine = FirstLine();
    }
    void EachLine(const std::function<void(Line *)> &func)
    {
        for (auto &l: lines)
        {
            func(l);
        }
    }
    Line* NextLine(Line* line)const
    {
        auto it = find(line);
        if (it==lines.end())
        {
            return nullptr;
        }
        ++it;
        if (it==lines.end())
        {
            return nullptr;
        }
        return *it;
    }
    void SetNextLine(Line* line, Line* next)
    {
        auto it = find(line);
        assert(it!=lines.end());
        ++it;
        lines.insert(it, next);
    }
    Line* PrevLine(Line* line)const
    {
        auto it = find(line);
        if (it==lines.end())
        {
            return nullptr;
        }
        if (it==lines.begin())
        {
            return nullptr;
        }
        --it;
        return *it;
    }
    void SetPrevLine(Line* line, Line* prev)
    {
        auto it = find(line);
        assert(it!=lines.end());
        lines.insert(it, prev);
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
    void COPY_BUFPOSITION_FROM(const BufferPtr srcbuf)
    {
        this->SetTopLine((srcbuf)->TopLine());
        this->currentLine = (srcbuf)->currentLine;
        this->pos = (srcbuf)->pos;
        this->cursorX = (srcbuf)->cursorX;
        this->cursorY = (srcbuf)->cursorY;
        this->visualpos = (srcbuf)->visualpos;
        this->currentColumn = (srcbuf)->currentColumn;
    }
    void restorePosition(const BufferPtr orig)
    {
        this->LineSkip(this->FirstLine(), orig->TOP_LINENUMBER() - 1, false);
        this->GotoLine(orig->CUR_LINENUMBER());
        this->pos = orig->pos;
        if (this->currentLine && orig->currentLine)
            this->pos += orig->currentLine->bpos - this->currentLine->bpos;
        this->currentColumn = orig->currentColumn;
        this->ArrangeCursor();
    }
    void COPY_BUFROOT_FROM(const BufferPtr srcbuf)
    {
        this->rootX = (srcbuf)->rootX;
        this->rootY = (srcbuf)->rootY;
        this->COLS = (srcbuf)->COLS;
        this->LINES = (srcbuf)->LINES;
    }

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
    URL currentURL ={};
    URL baseURL ={};
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
