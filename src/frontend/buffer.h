#pragma once
#include "stream/url.h"
#include "html/anchor.h"
#include "termrect.h"
#include "line.h"
#include "link.h"

#include <stdint.h>
#include <assert.h>
#include <vector>
#include <memory>
#include <math.h>

struct Line;
class InputStream;
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

    // HTML source
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

using BufferPtr = std::shared_ptr<struct Buffer>;
using LinePtr = std::shared_ptr<struct Line>;
using LineList = std::vector<LinePtr>;
struct Buffer : std::enable_shared_from_this<Buffer>
{
    std::string filename;
    std::string buffername;

    bool need_reshape = false;
    LineList lines;

private:
    // scroll
    LinePtr topLine;
    // cursor ?
    LinePtr currentLine;

    // private:
    // list
    LineList::iterator find(LinePtr l)
    {
        return std::find(lines.begin(), lines.end(), l);
    }
    LineList::const_iterator find(LinePtr l) const
    {
        return std::find(lines.begin(), lines.end(), l);
    }

public:
    int LineCount() const
    {
        return lines.size();
    }

public:
    void AddNewLineFixedWidth(const PropertiedString &lineBuffer, int real_linenumber, int width);
    void AddNewLine(const PropertiedString &lineBuffer, int real_linenumber = -1);
    void ClearLines()
    {
        currentLine = topLine = NULL;
        lines.clear();
    }
    LinePtr FirstLine() const
    {
        if (lines.empty())
        {
            return nullptr;
        }
        return lines.front();
    }
    LinePtr LastLine() const
    {
        if (lines.empty())
        {
            return nullptr;
        }
        return lines.back();
    }
    LinePtr TopLine() const
    {
        return topLine;
    }
    LinePtr CurrentLine() const
    {
        return currentLine;
    }
    void SetFirstLine(LinePtr line)
    {
        assert(false);
        // firstLine = line;
    }
    void SetLastLine(LinePtr line)
    {
        assert(false);
    }
    void SetTopLine(LinePtr line)
    {
        topLine = line;
    }
    void SetCurrentLine(LinePtr line)
    {
        currentLine = line;
    }
    int TOP_LINENUMBER() const
    {
        return (TopLine() ? TopLine()->linenumber : 1);
    }
    int CUR_LINENUMBER() const
    {
        return (currentLine ? currentLine->linenumber : 1);
    }

    void GotoLine(int n, bool topline = false);
    void Goto(const BufferPoint &po, bool topline = false)
    {
        GotoLine(po.line, topline);
        pos = po.pos;
        ArrangeCursor();
    }
    void GotoRealLine(int n);
    // void Reshape();
    LinePtr CurrentLineSkip(LinePtr line, int offset, int last);
    void LineSkip(LinePtr line, int offset, int last);
    void Scroll(int n);
    void NScroll(int n);
    void CurrentAsLast()
    {
        if (currentLine)
        {
            auto it = find(currentLine);
            if (it == lines.end())
            {
                assert(false);
                return;
            }
            ++it;
            lines.erase(it, lines.end());
        }

        // lastLine = currentLine;
        topLine = FirstLine();
        currentLine = FirstLine();
    }
    void EachLine(const std::function<void(LinePtr)> &func)
    {
        for (auto &l : lines)
        {
            func(l);
        }
    }
    LinePtr NextLine(LinePtr line) const
    {
        auto it = find(line);
        if (it == lines.end())
        {
            return nullptr;
        }
        ++it;
        if (it == lines.end())
        {
            return nullptr;
        }
        return *it;
    }
    void SetNextLine(LinePtr line, LinePtr next)
    {
        auto it = find(line);
        assert(it != lines.end());
        ++it;
        lines.insert(it, next);
    }
    LinePtr PrevLine(LinePtr line) const
    {
        auto it = find(line);
        if (it == lines.end())
        {
            return nullptr;
        }
        if (it == lines.begin())
        {
            return nullptr;
        }
        --it;
        return *it;
    }
    void SetPrevLine(LinePtr line, LinePtr prev)
    {
        auto it = find(line);
        assert(it != lines.end());
        lines.insert(it, prev);
    }
    bool MoveLeftWord(int n);
    bool MoveRightWord(int n);

    std::array<BufferPtr, MAX_LB> linkBuffer;
    short width;
    short height;

    // mimetype: text/plain
    std::string type;
    std::string real_type;

public:
    BufferProps bufferprop = BP_NORMAL;
    int currentColumn = 0;
    int pos = 0;
    int visualpos = 0;

    TermRect rect;

    void CursorHome()
    {
        visualpos = 0;
        rect.resetCursor();
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
        this->rect.cursorX = (srcbuf)->rect.cursorX;
        this->rect.cursorY = (srcbuf)->rect.cursorY;
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

    std::shared_ptr<class InputStream> pagerSource;
    AnchorList href;
    AnchorList name;
    AnchorList img;
    AnchorList formitem;
    int prevhseq = -1;

    std::vector<Link> linklist;
    FormList *formlist = nullptr;
    MapList *maplist = nullptr;
    std::vector<BufferPoint> hmarklist;
    std::vector<BufferPoint> imarklist;
    URL currentURL = {};
    URL baseURL = {};
    std::string baseTarget;
    int real_scheme = 0;
    std::string sourcefile;
    struct frameset *frameset = nullptr;
    struct frameset_queue *frameQ = nullptr;
    int *clone = nullptr;
    size_t trbyte = 0;
    char check_url = 0;
    CharacterEncodingScheme document_charset = WC_CES_NONE;
    AutoDetectTypes auto_detect = WC_OPT_DETECT_OFF;
    // TextList *document_header = nullptr;
    FormItemList *form_submit = nullptr;
    // ReadBufferCache, WriteBufferCache
    std::string savecache;
    // editBf
    std::string edit;
    Mailcap *mailcap = nullptr;
    std::string mailcap_source;
    // backup stream read contents ?
    std::string header_source;
    char search_header = 0;
    // https
    std::string ssl_certificate;
    char image_flag = 0;
    char image_loaded = 0;
    Anchor *submit = nullptr;
    BufferPos *undo = nullptr;
    AlarmEvent *event = nullptr;

    Buffer();
    ~Buffer();
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    BufferPoint CurrentPoint() const
    {
        auto line = CurrentLine();
        if (!line)
        {
            return {-1, -1, true};
        }
        return {line->linenumber, pos};
    }

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

    void DrawLine(LinePtr l, int i);
    int DrawLineRegion(LinePtr l, int i, int bpos, int epos);
};

BufferPtr newBuffer(const URL &url);

void deleteImage(Buffer *buf);
void getAllImage(const BufferPtr &buf);

#define IMG_FLAG_START 0
#define IMG_FLAG_STOP 1
#define IMG_FLAG_NEXT 2
void loadImage(BufferPtr buf, int flag);

BufferPtr nullBuffer(void);

void set_buffer_environ(const BufferPtr &buf);

// anchor
void reAnchorWord(BufferPtr buf, LinePtr l, int spos, int epos);
const char *reAnchor(BufferPtr buf, const char *re);
const char *reAnchorNews(BufferPtr buf, const char *re);
char *reAnchorNewsheader(const BufferPtr &buf);
void addMultirowsForm(BufferPtr buf, AnchorList &al);
void addMultirowsImg(BufferPtr buf, AnchorList &al);
char *getAnchorText(BufferPtr buf, AnchorList &al, Anchor *a);
