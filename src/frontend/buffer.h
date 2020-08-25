#pragma once
#include "stream/url.h"
#include "html/anchor.h"
#include "html/maparea.h"
#include "html/form.h"
#include "html/image.h"
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
struct MapList;
struct AlarmEvent;
struct TextList;

enum BufferProps : int16_t
{
    BP_NORMAL = 0x0,
    BP_PIPE = 0x1,
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
};

enum DisplayMode
{
    B_NONE,
    B_NORMAL,
    B_FORCE_REDRAW,
    B_REDRAW,
    B_SCROLL,
    B_REDRAW_IMAGE,
};

/* Search Result */
enum SearchResultTypes
{
    SR_NONE = 0,
    SR_FOUND = 0x1,
    SR_NOTFOUND = 0x2,
    SR_WRAPPED = 0x4,
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
    DisplayMode m_redraw = B_REDRAW;

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
    DisplayMode Update()
    {
        auto redraw = m_redraw;
        m_redraw = B_NONE;
        return redraw;
    }
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

    void _goLine(std::string_view l, int prec_num);
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
    void resetPos(int i);
    void undoPos(int prec_num);
    void redoPos();

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

private:
    using SearchFunc = SearchResultTypes (*)(const std::shared_ptr<struct Buffer> &buf, std::string_view str);
    SearchFunc searchRoutine = nullptr;
    std::string SearchString;    
    SearchResultTypes srchcore(std::string_view str, SearchFunc func, int prec_num);
    int dispincsrch(int ch, Str buf, Lineprop *prop, int prec_num);

public:
    void isrch(SearchFunc func, const char *prompt, int prec_num);
    void srch(SearchFunc func, const char *prompt, std::string_view, int prec_num);
    std::string conv_search_string(std::string_view str, CharacterEncodingScheme f_ces);
    void srch_nxtprv(bool reverse, int prec_num);

    std::shared_ptr<class InputStream> pagerSource;
    AnchorList href;
    AnchorList name;
    AnchorList img;
    AnchorList formitem;
    int prevhseq = -1;

    std::vector<Link> linklist;
    std::vector<FormPtr> formlist;
    std::vector<MapListPtr> maplist;
    std::vector<BufferPoint> hmarklist;
    std::vector<BufferPoint> imarklist;
    URL currentURL = {};
    URL baseURL = {};
    std::string baseTarget;
    int real_scheme = 0;
    std::string sourcefile;

    size_t trbyte = 0;
    char check_url = 0;
    CharacterEncodingScheme document_charset = WC_CES_NONE;
    AutoDetectTypes auto_detect = WC_OPT_DETECT_OFF;
    // TextList *document_header = nullptr;
    FormItemPtr form_submit;
    // ReadBufferCache, WriteBufferCache
    std::string savecache;
    // editBf
    std::string edit;
    std::string mailcap_source;
    // backup stream read contents ?
    std::string header_source;
    char search_header = 0;
    // https
    std::string ssl_certificate;
    ImageFlags image_flag = IMG_FLAG_NONE;
    char image_loaded = 0;
    AnchorPtr submit = nullptr;
    std::vector<BufferPos> undo;
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
    URL *BaseURL();
    void putHmarker(int line, int pos, int seq);
    void shiftAnchorPosition(AnchorList &al, const BufferPoint &bp, int shift);
    void SavePosition();
    void DumpSource();

    void DrawLine(LinePtr l, int i);
    int DrawLineRegion(LinePtr l, int i, int bpos, int epos);
};

BufferPtr newBuffer(const URL &url);

BufferPtr nullBuffer(void);

void set_buffer_environ(const BufferPtr &buf);

// anchor
void reAnchorWord(BufferPtr buf, LinePtr l, int spos, int epos);
const char *reAnchor(BufferPtr buf, const char *re);
const char *reAnchorNews(BufferPtr buf, const char *re);
char *reAnchorNewsheader(const BufferPtr &buf);
void addMultirowsForm(BufferPtr buf, AnchorList &al);
void addMultirowsImg(BufferPtr buf, AnchorList &al);
char *getAnchorText(BufferPtr buf, AnchorList &al, AnchorPtr a);

// search
SearchResultTypes forwardSearch(const std::shared_ptr<struct Buffer> &buf, std::string_view str);
SearchResultTypes backwardSearch(const std::shared_ptr<struct Buffer> &buf, std::string_view str);
