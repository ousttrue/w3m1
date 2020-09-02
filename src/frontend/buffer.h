#pragma once
#include "stream/url.h"
#include "frontend/anchor.h"
#include "html/maparea.h"
#include "html/form.h"
#include "html/image.h"
#include "viewport.h"
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

/* Search Result */
enum SearchResultTypes
{
    SR_NONE = 0,
    SR_FOUND = 0x1,
    SR_NOTFOUND = 0x2,
    SR_WRAPPED = 0x4,
};

using DocumentPtr = std::shared_ptr<class Document>;
using LineList = std::vector<LinePtr>;
class Document
{
public:
    CharacterEncodingScheme document_charset = WC_CES_NONE;

    LineList m_lines;
    LineList::iterator _find(LinePtr l)
    {
        return std::find(m_lines.begin(), m_lines.end(), l);
    }
    LineList::const_iterator _find(LinePtr l) const
    {
        return std::find(m_lines.begin(), m_lines.end(), l);
    }

public:
    int LineCount() const
    {
        return m_lines.size();
    }
    LinePtr GetLine(int i) const
    {
        if (i < 0 || i >= m_lines.size())
        {
            return nullptr;
        }
        return m_lines[i];
    }
    void Clear()
    {
        m_lines.clear();
    }
    LinePtr AddLine()
    {
        auto line = std::make_shared<Line>();
        m_lines.push_back(line);
        return line;
    }
    LinePtr FirstLine() const
    {
        if (m_lines.empty())
        {
            return nullptr;
        }
        return m_lines.front();
    }
    LinePtr LastLine() const
    {
        if (m_lines.empty())
        {
            return nullptr;
        }
        return m_lines.back();
    }
    void EraseTo(int currentLine)
    {
        auto s = m_lines.begin();
        auto e = m_lines.begin();
        for (int i = 0; i < LineCount() && e != m_lines.end(); ++i, ++e)
        {
        }
        m_lines.erase(s, e);
    }
    LinePtr NextLine(LinePtr line) const
    {
        auto it = _find(line);
        if (it == m_lines.end())
        {
            return nullptr;
        }
        ++it;
        if (it == m_lines.end())
        {
            return nullptr;
        }
        return *it;
    }
    void InsertNext(LinePtr pos, LinePtr line)
    {
        auto it = _find(pos);
        assert(it != m_lines.end());
        ++it;
        m_lines.insert(it, line);
    }
    LinePtr PrevLine(LinePtr line) const
    {
        auto it = _find(line);
        if (it == m_lines.end())
        {
            return nullptr;
        }
        if (it == m_lines.begin())
        {
            return nullptr;
        }
        --it;
        return *it;
    }
    void InsertPrev(LinePtr line, LinePtr prev)
    {
        auto it = _find(line);
        assert(it != m_lines.end());
        m_lines.insert(it, prev);
    }

    AnchorList href;
    AnchorList name;
    AnchorList img;
    AnchorList formitem;
    std::vector<Link> linklist;
    std::vector<BufferPoint> hmarklist;
    void putHmarker(int line, int pos, int seq)
    {
        if ((seq + 1) >= hmarklist.size())
        {
            hmarklist.resize(seq + 1);
        }
        hmarklist[seq].line = line;
        hmarklist[seq].pos = pos;
        hmarklist[seq].invalid = 0;
    }
    std::vector<BufferPoint> imarklist;
    std::vector<FormPtr> formlist;
    std::vector<MapListPtr> maplist;
    AlarmEvent *event = nullptr;
};

using BufferPtr = std::shared_ptr<struct Buffer>;
using LinePtr = std::shared_ptr<struct Line>;
struct Buffer : std::enable_shared_from_this<Buffer>
{
    std::string filename;
    std::string buffername;

    bool need_reshape = false;

    DocumentPtr m_document;

    //
    // vertical
    //
    // top line
    int m_topLine = 0;
    // cursor line
    int m_currentLine = 0;
    int CursorY() const
    {
        return m_currentLine - m_topLine;
    }

    //
    // horizontal
    //
    // left column
    int leftCol = 0;
    // curosr column
    int CurrentCol() const
    {
        return CurrentLine()->buffer.BytePositionToColumn(bytePosition);
    }

    int bytePosition = 0;

    int CursorX() const
    {
        return CurrentCol() - leftCol;
    }

    std::tuple<int, int> GlobalXY() const
    {
        return {
            rect.rootX + CursorX(),
            rect.rootY + CursorY(),
        };
    }

public:
    static std::shared_ptr<Buffer> Create(const URL &url);

public:
    void AddNewLineFixedWidth(const PropertiedString &lineBuffer, int real_linenumber, int width);
    void AddNewLine(const PropertiedString &lineBuffer, int real_linenumber = -1);
    void ClearLines()
    {
        m_currentLine = 0;
        m_topLine = 0;
        m_document->Clear();
    }
    LinePtr TopLine() const
    {
        return m_document->GetLine(m_topLine);
    }
    LinePtr CurrentLine() const
    {
        return m_document->GetLine(m_currentLine);
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
    // void SetTopLine(LinePtr line)
    // {
    //     topLine = line;
    // }
    // void SetCurrentLine(LinePtr line)
    // {
    //     currentLine = line;
    // }
    // int TOP_LINENUMBER() const
    // {
    //     return (TopLine() ? TopLine()->linenumber : 1);
    // }
    // int CUR_LINENUMBER() const
    // {
    //     return (currentLine ? currentLine->linenumber : 1);
    // }

    void _goLine(std::string_view l, int prec_num);
    void GotoLine(int n, bool topline = false);
    void Goto(const BufferPoint &po, bool topline = false)
    {
        GotoLine(po.line, topline);
        bytePosition = po.pos;
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
        m_document->EraseTo(m_currentLine);
        m_topLine = 0;
        m_currentLine = 0;
    }
    bool MoveLeftWord(int n);
    bool MoveRightWord(int n);
    void resetPos(int i);
    void undoPos(int prec_num);
    void redoPos();

    // mimetype: text/plain
    std::string type;
    std::string real_type;

public:
    BufferProps bufferprop = BP_NORMAL;

    Viewport rect;

    void CursorHome()
    {
        bytePosition = 0;
        m_currentLine = 0;
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
        this->m_topLine = srcbuf->m_topLine;
        this->m_currentLine = srcbuf->m_currentLine;
        this->bytePosition = srcbuf->bytePosition;
        this->leftCol = srcbuf->leftCol;
    }
    void restorePosition(const BufferPtr orig)
    {
        this->m_topLine = orig->m_topLine;
        this->m_currentLine = orig->m_currentLine;
        this->bytePosition = orig->bytePosition;
        this->leftCol = orig->leftCol;
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

    int prevhseq = -1;

    URL url = {};
    std::string baseTarget;
    int real_scheme = 0;
    std::string sourcefile;
    char check_url = 0;
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
        return {line->linenumber, bytePosition};
    }

    void TmpClear();
    int WriteBufferCache();
    int ReadBufferCache();
    BufferPtr Copy();
    void CopyFrom(BufferPtr src);
    void shiftAnchorPosition(AnchorList &al, const BufferPoint &bp, int shift);
    void SavePosition();
    void DumpSource();
    int DrawLineRegion(LinePtr l, int i, int bpos, int epos);
};

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
