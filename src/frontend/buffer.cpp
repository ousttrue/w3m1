#include <sstream>
#include <assert.h>
#include "fm.h"
#include "frontend/buffer.h"
#include "frontend/display.h"
#include "frontend/line.h"
#include "frontend/line.h"
#include "urimethod.h"
#include "public.h"
#include "indep.h"
#include "gc_helper.h"
#include "file.h"
#include "html/image.h"
#include "html/html.h"
#include "ctrlcode.h"
#include "transport/local.h"
#include "html/anchor.h"
#include "transport/url.h"
#include "transport/loader.h"
#include "mime/mimetypes.h"
#include "charset.h"
#include "html/parsetagx.h"

static int REV_LB[MAX_LB] = {
    LB_N_FRAME,
    LB_FRAME,
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};

template <typename T>
bool fwrite1(const T &d, FILE *f)
{
    return (fwrite(&d, sizeof(d), 1, f) == 0);
}

template <typename T>
bool fread1(T &d, FILE *f)
{
    return (fread(&d, sizeof(d), 1, f) == 0);
}

Buffer::Buffer()
{
    COLS = COLS;
    LINES = (LINES - 1);
    currentURL.scheme = SCM_UNKNOWN;
    baseURL = {};
    baseTarget = {};
    bufferprop = BP_NORMAL;
    clone = New(int);
    *clone = 1;
    trbyte = 0;
    auto_detect = WcOption.auto_detect;
}

Buffer::~Buffer()
{
    deleteImage(this);
    ClearLines();
    for (int i = 0; i < MAX_LB; i++)
    {
        auto b = linkBuffer[i];
        if (b == NULL)
            continue;
        b->linkBuffer[REV_LB[i]] = NULL;
    }
    if (savecache.size())
        unlink(savecache.c_str());
    if (--(*clone))
        return;
    if (pagerSource)
        ISclose(pagerSource);
    if (sourcefile.size() &&
        (real_type.empty() || real_type.starts_with("image/")))
    {
        if (real_scheme != SCM_LOCAL || bufferprop & BP_FRAME)
            unlink(sourcefile.c_str());
    }
    if (header_source.size())
        unlink(header_source.c_str());
    if (mailcap_source.size())
        unlink(mailcap_source.c_str());
    while (frameset)
    {
        deleteFrameSet(frameset);
        frameset = popFrameTree(&(frameQ));
    }
}

void Buffer::TmpClear()
{
    if (this->pagerSource == NULL && this->WriteBufferCache() == 0)
    {
        this->firstLine = NULL;
        this->topLine = NULL;
        this->currentLine = NULL;
        this->lastLine = NULL;
    }
}

int Buffer::WriteBufferCache()
{
    // TODO
    return -1;

    if (savecache.size())
    {
        // already created
        return -1;
    }
    Str tmp;
    FILE *cache = NULL;
    Line *l;
    int colorflag;

    if (this->LineCount() == 0)
        goto _error1;

    tmp = tmpfname(TMPF_CACHE, NULL);
    this->savecache = tmp->ptr;
    cache = fopen(this->savecache.c_str(), "w");
    if (!cache)
        goto _error1;

    if (fwrite1(this->currentLine->linenumber, cache) ||
        fwrite1(this->topLine->linenumber, cache))
        goto _error;

    for (l = this->firstLine; l; l = l->next)
    {
        if (fwrite1(l->real_linenumber, cache) ||
            fwrite1(l->usrflags, cache) ||
            fwrite1(l->width, cache) ||
            fwrite1(l->len, cache) ||
            fwrite1(l->size, cache) ||
            fwrite1(l->bpos, cache) || fwrite1(l->bwidth, cache))
            goto _error;
        if (l->bpos == 0)
        {
            if (fwrite(l->lineBuf, 1, l->size, cache) < l->size ||
                fwrite(l->propBuf, sizeof(Lineprop), l->size, cache) < l->size)
                goto _error;
        }
#ifdef USE_ANSI_COLOR
        colorflag = l->colorBuf ? 1 : 0;
        if (fwrite1(colorflag, cache))
            goto _error;
        if (colorflag)
        {
            if (l->bpos == 0)
            {
                if (fwrite(l->colorBuf, sizeof(Linecolor), l->size, cache) <
                    l->size)
                    goto _error;
            }
        }
#endif
    }

    fclose(cache);
    return 0;
_error:
    fclose(cache);
    unlink(this->savecache.c_str());
_error1:
    this->savecache.clear();
    return -1;
}

int Buffer::ReadBufferCache()
{
    // TODO:
    return -1;

    Line *basel = NULL;
    long clnum, tlnum;
    int colorflag;

    if (this->savecache.empty())
        return -1;

    auto cache = fopen(this->savecache.c_str(), "r");
    if (cache == NULL || fread1(clnum, cache) || fread1(tlnum, cache))
    {
        this->savecache.clear();
        return -1;
    }

    Line *prevl = nullptr;
    Line *l = nullptr;
    for (int lnum = 0; !feof(cache); ++lnum, prevl = l)
    {
        l = New(Line);
        l->prev = prevl;
        if (prevl)
            prevl->next = l;
        else
            this->firstLine = l;
        l->linenumber = lnum;
        if (lnum == clnum)
            this->currentLine = l;
        if (lnum == tlnum)
            this->topLine = l;
        if (fread1(l->real_linenumber, cache) ||
            fread1(l->usrflags, cache) ||
            fread1(l->width, cache) ||
            fread1(l->len, cache) ||
            fread1(l->size, cache) ||
            fread1(l->bpos, cache) || fread1(l->bwidth, cache))
            break;
        if (l->bpos == 0)
        {
            basel = l;
            l->lineBuf = NewAtom_N(char, l->size + 1);
            fread(l->lineBuf, 1, l->size, cache);
            l->lineBuf[l->size] = '\0';
            l->propBuf = NewAtom_N(Lineprop, l->size);
            fread(l->propBuf, sizeof(Lineprop), l->size, cache);
        }
        else if (basel)
        {
            l->lineBuf = basel->lineBuf + l->bpos;
            l->propBuf = basel->propBuf + l->bpos;
        }
        else
            break;
#ifdef USE_ANSI_COLOR
        if (fread1(colorflag, cache))
            break;
        if (colorflag)
        {
            if (l->bpos == 0)
            {
                l->colorBuf = NewAtom_N(Linecolor, l->size);
                fread(l->colorBuf, sizeof(Linecolor), l->size, cache);
            }
            else
                l->colorBuf = basel->colorBuf + l->bpos;
        }
        else
        {
            l->colorBuf = NULL;
        }
#endif
    }
    this->lastLine = prevl;
    this->lastLine->next = NULL;
    fclose(cache);
    unlink(this->savecache.c_str());
    this->savecache.clear();
    return 0;
}

BufferPtr Buffer::Copy()
{
    auto copy = newBuffer(width);
    copy->CopyFrom(this);
    return copy;
}

void Buffer::CopyFrom(BufferPtr src)
{
    src->ReadBufferCache();
    ++(*src->clone);

    *this = *src;
}

void Buffer::ClearLink()
{
    for (int i = 0; i < MAX_LB; i++)
    {
        linkBuffer[i] = NULL;
    }
}

URL *Buffer::BaseURL()
{
    if (bufferprop & BP_NO_URL)
    {
        /* no URL is defined for the buffer */
        return NULL;
    }
    if (baseURL)
    {
        /* <BASE> tag is defined in the document */
        return &baseURL;
    }
    else
    {
        return &currentURL;
    }
}

void Buffer::putHmarker(int line, int pos, int seq)
{
    if ((seq + 1) >= hmarklist.size())
    {
        hmarklist.resize(seq + 1);
    }
    hmarklist[seq].line = line;
    hmarklist[seq].pos = pos;
    hmarklist[seq].invalid = 0;
}

void Buffer::shiftAnchorPosition(AnchorList &al, const BufferPoint &bp, int shift)
{
    if (al.size() == 0)
        return;

    auto s = al.size() / 2;
    auto e = al.size() - 1;
    for (auto b = 0; b <= e; s = (b + e + 1) / 2)
    {
        auto a = &al.anchors[s];
        auto cmp = a->CmpOnAnchor(bp);
        if (cmp == 0)
            break;
        else if (cmp > 0)
            b = s + 1;
        else if (s == 0)
            break;
        else
            e = s - 1;
    }
    for (; s < al.size(); s++)
    {
        auto a = &al.anchors[s];
        if (a->start.line > bp.line)
            break;
        if (a->start.pos > bp.pos)
        {
            a->start.pos += shift;
            if (hmarklist[a->hseq].line == bp.line)
                hmarklist[a->hseq].pos = a->start.pos;
        }
        if (a->end.pos >= bp.pos)
            a->end.pos += shift;
    }
}

const char *NullLine = "";
Lineprop NullProp[] = {0};

/* 
 * Buffer creation
 */
BufferPtr
newBuffer(int width)
{
    auto n = new Buffer;
    n->width = width;
    return n;
}

/* 
 * Create null buffer
 */
BufferPtr
nullBuffer(void)
{
    BufferPtr b;

    b = newBuffer(COLS);
    b->buffername = "*Null*";
    return b;
}

Line *Buffer::CurrentLineSkip(Line *line, int offset, int last)
{
    int i, n;
    Line *l = line;

    if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    {
        n = line->linenumber + offset + this->LINES;
        if (this->lastLine->linenumber < n)
            getNextPage(this, n - this->lastLine->linenumber);
        while ((last || (this->lastLine->linenumber < n)) &&
               (getNextPage(this, 1) != NULL))
            ;
        if (last)
            l = this->lastLine;
    }

    if (offset == 0)
        return l;
    if (offset > 0)
        for (i = 0; i < offset && l->next != NULL; i++, l = l->next)
            ;
    else
        for (i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
            ;
    return l;
}

void Buffer::LineSkip(Line *line, int offset, int last)
{
    auto l = CurrentLineSkip(line, offset, last);

    int i;
    if (!nextpage_topline)
        for (i = this->LINES - 1 - (this->lastLine->linenumber - l->linenumber);
             i > 0 && l->prev != NULL; i--, l = l->prev)
            ;
    topLine = l;
}

/* 
 * gotoLine: go to line number
 */
void Buffer::GotoLine(int n)
{
    char msg[32];
    Line *l = this->firstLine;
    if (l == NULL)
        return;
    if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    {
        if (this->lastLine->linenumber < n)
            getNextPage(this, n - this->lastLine->linenumber);
        while ((this->lastLine->linenumber < n) &&
               (getNextPage(this, 1) != NULL))
            ;
    }
    if (l->linenumber > n)
    {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%d", l->linenumber);
        set_delayed_message(msg);
        this->topLine = this->currentLine = l;
        return;
    }
    if (this->lastLine->linenumber < n)
    {
        l = this->lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%d", this->lastLine->linenumber);
        set_delayed_message(msg);
        this->currentLine = l;
        LineSkip(this->currentLine, -(this->LINES - 1), FALSE);
        return;
    }
    for (; l != NULL; l = l->next)
    {
        if (l->linenumber >= n)
        {
            this->currentLine = l;
            if (n < this->topLine->linenumber ||
                this->topLine->linenumber + this->LINES <= n)
                LineSkip(l, -(this->LINES + 1) / 2, FALSE);
            break;
        }
    }
}

void Buffer::Scroll(int n)
{
    auto lnum = currentLine->linenumber;
    auto top = topLine;
    this->LineSkip(top, n, FALSE);
    if (this->topLine == top)
    {
        lnum += n;
        if (lnum < this->topLine->linenumber)
            lnum = this->topLine->linenumber;
        else if (lnum > this->lastLine->linenumber)
            lnum = this->lastLine->linenumber;
    }
    else
    {
        auto tlnum = this->topLine->linenumber;
        auto llnum = this->topLine->linenumber + this->LINES - 1;
        int diff_n;
        if (nextpage_topline)
            diff_n = 0;
        else
            diff_n = n - (tlnum - top->linenumber);
        if (lnum < tlnum)
            lnum = tlnum + diff_n;
        if (lnum > llnum)
            lnum = llnum + diff_n;
    }
    this->GotoLine(lnum);
}

/* 
 * gotoRealLine: go to real line number
 */
void Buffer::GotoRealLine(int n)
{
    char msg[32];
    Line *l = this->firstLine;

    if (l == NULL)
        return;
    if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    {
        if (this->lastLine->real_linenumber < n)
            getNextPage(this, n - this->lastLine->real_linenumber);
        while ((this->lastLine->real_linenumber < n) &&
               (getNextPage(this, 1) != NULL))
            ;
    }
    if (l->real_linenumber > n)
    {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%d", l->real_linenumber);
        set_delayed_message(msg);
        this->topLine = this->currentLine = l;
        return;
    }
    if (this->lastLine->real_linenumber < n)
    {
        l = this->lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%d", this->lastLine->real_linenumber);
        set_delayed_message(msg);
        this->currentLine = l;
        LineSkip(this->currentLine, -(this->LINES - 1),
                 FALSE);
        return;
    }
    for (; l != NULL; l = l->next)
    {
        if (l->real_linenumber >= n)
        {
            this->currentLine = l;
            if (n < this->topLine->real_linenumber ||
                this->topLine->real_linenumber + this->LINES <= n)
                LineSkip(l, -(this->LINES + 1) / 2, FALSE);
            break;
        }
    }
}

/* 
 * Reshape HTML buffer
 */
void Buffer::Reshape()
{
    uint8_t old_auto_detect = WcOption.auto_detect;

    if (!need_reshape)
        return;
    need_reshape = FALSE;

    this->width = INIT_BUFFER_WIDTH;
    if (this->sourcefile.empty())
        return;
    URLFile f(SCM_LOCAL, NULL);
    f.examineFile(this->mailcap_source.size() ? this->mailcap_source.c_str() : this->sourcefile.c_str());
    if (f.stream == NULL)
        return;

    auto sbuf = this->Copy();
    this->ClearLines();
    while (this->frameset)
    {
        deleteFrameSet(this->frameset);
        this->frameset = popFrameTree(&(this->frameQ));
    }

    this->href.clear();
    this->name.clear();
    this->img.clear();
    this->formitem.clear();
    this->formlist = NULL;
    this->linklist.clear();
    this->maplist = NULL;
    this->hmarklist.clear();
    this->imarklist.clear();

    if (this->header_source.size())
    {
        if (this->currentURL.scheme != SCM_LOCAL ||
            this->mailcap_source.size() || this->currentURL.file == "-")
        {
            URLFile h(SCM_LOCAL, NULL);
            h.examineFile(this->header_source);
            if (h.stream)
            {
                readHeader(&h, this, TRUE, NULL);
                h.Close();
            }
        }
        else if (this->search_header) /* -m option */
            readHeader(&f, this, TRUE, NULL);
    }

    WcOption.auto_detect = WC_OPT_DETECT_OFF;
    UseContentCharset = FALSE;

    if (is_html_type(this->type))
        loadHTMLBuffer(&f, this);
    else
        loadBuffer(&f, this);
    f.Close();

    WcOption.auto_detect = (AutoDetectTypes)old_auto_detect;
    UseContentCharset = TRUE;

    this->height = (LINES - 1) + 1;
    if (this->firstLine && sbuf->firstLine)
    {
        Line *cur = sbuf->currentLine;
        int n;

        this->pos = sbuf->pos + cur->bpos;
        while (cur->bpos && cur->prev)
            cur = cur->prev;
        if (cur->real_linenumber > 0)
            this->GotoRealLine(cur->real_linenumber);
        else
            this->GotoLine(cur->linenumber);
        n = (this->currentLine->linenumber - this->topLine->linenumber) - (cur->linenumber - sbuf->topLine->linenumber);
        if (n)
        {
            LineSkip(this->topLine, n, FALSE);
            if (cur->real_linenumber > 0)
                this->GotoRealLine(cur->real_linenumber);
            else
                this->GotoLine(cur->linenumber);
        }
        this->pos -= this->currentLine->bpos;
        if (FoldLine && !is_html_type(this->type))
            this->currentColumn = 0;
        else
            this->currentColumn = sbuf->currentColumn;
        ArrangeCursor();
    }
    if (this->check_url & CHK_URL)
        chkURLBuffer(this);

    if (this->check_url & CHK_NMID)
        chkNMIDBuffer(this);
    if (this->real_scheme == SCM_NNTP || this->real_scheme == SCM_NEWS)
        reAnchorNewsheader(this);

    formResetBuffer(this, sbuf->formitem);
}

void set_buffer_environ(BufferPtr buf)
{
    static BufferPtr prev_buf = NULL;
    static Line *prev_line = NULL;
    static int prev_pos = -1;
    Line *l;

    if (buf == NULL)
        return;
    if (buf != prev_buf)
    {
        set_environ("W3M_SOURCEFILE", buf->sourcefile);
        set_environ("W3M_FILENAME", buf->filename);
        set_environ("W3M_TITLE", buf->buffername.c_str());
        set_environ("W3M_URL", buf->currentURL.ToStr()->ptr);
        set_environ("W3M_TYPE", buf->real_type.size() ? buf->real_type : "unknown");
        set_environ("W3M_CHARSET", wc_ces_to_charset(buf->document_charset));
    }
    l = buf->currentLine;
    if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos))
    {
        URL pu;
        char *s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", (char *)(s ? s : ""));
        auto a = retrieveCurrentAnchor(buf);
        if (a)
        {
            pu.Parse2(a->url, buf->BaseURL());
            set_environ("W3M_CURRENT_LINK", pu.ToStr()->ptr);
        }
        else
            set_environ("W3M_CURRENT_LINK", "");
        a = retrieveCurrentImg(buf);
        if (a)
        {
            pu.Parse2(a->url, buf->BaseURL());
            set_environ("W3M_CURRENT_IMG", pu.ToStr()->ptr);
        }
        else
            set_environ("W3M_CURRENT_IMG", "");
        a = retrieveCurrentForm(buf);
        if (a)
            set_environ("W3M_CURRENT_FORM", form2str(a->item));
        else
            set_environ("W3M_CURRENT_FORM", "");
        set_environ("W3M_CURRENT_LINE", Sprintf("%d",
                                                l->real_linenumber)
                                            ->ptr);
        set_environ("W3M_CURRENT_COLUMN", Sprintf("%d",
                                                  buf->currentColumn +
                                                      buf->cursorX + 1)
                                              ->ptr);
    }
    else if (!l)
    {
        set_environ("W3M_CURRENT_WORD", "");
        set_environ("W3M_CURRENT_LINK", "");
        set_environ("W3M_CURRENT_IMG", "");
        set_environ("W3M_CURRENT_FORM", "");
        set_environ("W3M_CURRENT_LINE", "0");
        set_environ("W3M_CURRENT_COLUMN", "0");
    }
    prev_buf = buf;
    prev_line = l;
    prev_pos = buf->pos;
}

void Buffer::AddLine(char *line, Lineprop *prop, Linecolor *color, int pos, int nlines)
{
    Line *l;
    l = New(Line);
    l->next = NULL;
    l->lineBuf = line;
    l->propBuf = prop;
#ifdef USE_ANSI_COLOR
    l->colorBuf = color;
#endif
    l->len = pos;
    l->width = -1;
    l->size = pos;
    l->bpos = 0;
    l->bwidth = 0;
    l->prev = this->currentLine;
    if (this->currentLine)
    {
        l->next = this->currentLine->next;
        this->currentLine->next = l;
    }
    else
        l->next = NULL;
    if (this->lastLine == NULL || this->lastLine == this->currentLine)
        this->lastLine = l;
    this->currentLine = l;
    if (this->LineCount() == 0)
        this->firstLine = l;
    l->linenumber = ++this->allLine;
    if (nlines < 0)
    {
        /*     l->real_linenumber = l->linenumber;     */
        l->real_linenumber = 0;
    }
    else
    {
        l->real_linenumber = nlines;
    }
    l = NULL;
}

void Buffer::SavePosition()
{
    if (this->LineCount() == 0)
        return;

    BufferPos *b = this->undo;
    if (b && b->top_linenumber == TOP_LINENUMBER(this) &&
        b->cur_linenumber == CUR_LINENUMBER(this) &&
        b->currentColumn == this->currentColumn && b->pos == this->pos)
        return;
    b = New(BufferPos);
    b->top_linenumber = TOP_LINENUMBER(this);
    b->cur_linenumber = CUR_LINENUMBER(this);
    b->currentColumn = this->currentColumn;
    b->pos = this->pos;
    b->bpos = this->currentLine ? this->currentLine->bpos : 0;
    b->next = NULL;
    b->prev = this->undo;
    if (this->undo)
        this->undo->next = b;
    this->undo = b;
}

void Buffer::CursorUp0(int n)
{
    if (this->cursorY > 0)
        CursorUpDown(-1);
    else
    {
        this->LineSkip(this->topLine, -n, FALSE);
        if (this->currentLine->prev != NULL)
            this->currentLine = this->currentLine->prev;
        ArrangeLine();
    }
}

void Buffer::CursorUp(int n)
{
    Line *l = this->currentLine;
    if (this->LineCount() == 0)
        return;
    while (this->currentLine->prev && this->currentLine->bpos)
        CursorUp0(n);
    if (this->currentLine == this->firstLine)
    {
        this->GotoLine(l->linenumber);
        ArrangeLine();
        return;
    }
    CursorUp0(n);
    while (this->currentLine->prev && this->currentLine->bpos &&
           this->currentLine->bwidth >= this->currentColumn + this->visualpos)
        CursorUp0(n);
}

void Buffer::CursorDown0(int n)
{
    if (this->cursorY < this->LINES - 1)
        CursorUpDown(1);
    else
    {
        this->LineSkip(this->topLine, n, FALSE);
        if (this->currentLine->next != NULL)
            this->currentLine = this->currentLine->next;
        ArrangeLine();
    }
}

void Buffer::CursorDown(int n)
{
    Line *l = this->currentLine;
    if (this->LineCount() == 0)
        return;
    while (this->currentLine->next && this->currentLine->next->bpos)
        CursorDown0(n);
    if (this->currentLine == this->lastLine)
    {
        this->GotoLine(l->linenumber);
        ArrangeLine();
        return;
    }
    CursorDown0(n);
    while (this->currentLine->next && this->currentLine->next->bpos &&
           this->currentLine->bwidth + this->currentLine->width <
               this->currentColumn + this->visualpos)
        CursorDown0(n);
}

void Buffer::CursorUpDown(int n)
{
    Line *cl = this->currentLine;

    if (this->LineCount() == 0)
        return;
    if ((this->currentLine = this->CurrentLineSkip(cl, n, FALSE)) == cl)
        return;
    ArrangeLine();
}

void Buffer::CursorRight(int n)
{
    int i, delta = 1, cpos, vpos2;
    Line *l = this->currentLine;
    Lineprop *p;

    if (this->LineCount() == 0)
        return;
    if (this->pos == l->len && !(l->next && l->next->bpos))
        return;
    i = this->pos;
    p = l->propBuf;

    while (i + delta < l->len && p[i + delta] & PC_WCHAR2)
        delta++;

    if (i + delta < l->len)
    {
        this->pos = i + delta;
    }
    else if (l->len == 0)
    {
        this->pos = 0;
    }
    else if (l->next && l->next->bpos)
    {
        CursorDown0(1);
        this->pos = 0;
        ArrangeCursor();
        return;
    }
    else
    {
        this->pos = l->len - 1;
        while (this->pos && p[this->pos] & PC_WCHAR2)
            this->pos--;
    }
    cpos = l->COLPOS(this->pos);
    this->visualpos = l->bwidth + cpos - this->currentColumn;
    delta = 1;

    while (this->pos + delta < l->len && p[this->pos + delta] & PC_WCHAR2)
        delta++;

    vpos2 = l->COLPOS(this->pos + delta) - this->currentColumn - 1;
    if (vpos2 >= this->COLS && n)
    {
        ColumnSkip(n + (vpos2 - this->COLS) - (vpos2 - this->COLS) % n);
        this->visualpos = l->bwidth + cpos - this->currentColumn;
    }
    this->cursorX = this->visualpos - l->bwidth;
}

void Buffer::CursorLeft(int n)
{
    int i, delta = 1, cpos;
    Line *l = this->currentLine;
    Lineprop *p;

    if (this->LineCount() == 0)
        return;
    i = this->pos;
    p = l->propBuf;

    while (i - delta > 0 && p[i - delta] & PC_WCHAR2)
        delta++;

    if (i >= delta)
        this->pos = i - delta;
    else if (l->prev && l->bpos)
    {
        CursorUp0(-1);
        this->pos = this->currentLine->len - 1;
        ArrangeCursor();
        return;
    }
    else
        this->pos = 0;
    cpos = l->COLPOS(this->pos);
    this->visualpos = l->bwidth + cpos - this->currentColumn;
    if (this->visualpos - l->bwidth < 0 && n)
    {
        ColumnSkip(-n + this->visualpos - l->bwidth - (this->visualpos - l->bwidth) % n);
        this->visualpos = l->bwidth + cpos - this->currentColumn;
    }
    this->cursorX = this->visualpos - l->bwidth;
}

void Buffer::CursorXY(int x, int y)
{
    CursorUpDown(y - cursorY);

    if (this->cursorX > x)
    {
        while (this->cursorX > x)
            CursorLeft(this->COLS / 2);
    }
    else if (this->cursorX < x)
    {
        while (this->cursorX < x)
        {
            int oldX = this->cursorX;

            CursorRight(this->COLS / 2);

            if (oldX == this->cursorX)
                break;
        }
        if (this->cursorX > x)
            CursorLeft(this->COLS / 2);
    }
}

/* 
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void Buffer::ArrangeCursor()
{
    int col, col2, pos;
    int delta = 1;
    if (this->currentLine == NULL)
        return;
    /* Arrange line */
    if (this->currentLine->linenumber - this->topLine->linenumber >= this->LINES || this->currentLine->linenumber < this->topLine->linenumber)
    {
        this->LineSkip(this->currentLine, 0, FALSE);
    }
    /* Arrange column */
    while (this->pos < 0 && this->currentLine->prev && this->currentLine->bpos)
    {
        pos = this->pos + this->currentLine->prev->len;
        CursorUp0(1);
        this->pos = pos;
    }
    while (this->pos >= this->currentLine->len && this->currentLine->next &&
           this->currentLine->next->bpos)
    {
        pos = this->pos - this->currentLine->len;
        CursorDown0(1);
        this->pos = pos;
    }
    if (this->currentLine->len == 0 || this->pos < 0)
        this->pos = 0;
    else if (this->pos >= this->currentLine->len)
        this->pos = this->currentLine->len - 1;

    while (this->pos > 0 && this->currentLine->propBuf[this->pos] & PC_WCHAR2)
        this->pos--;

    col = this->currentLine->COLPOS(this->pos);

    while (this->pos + delta < this->currentLine->len &&
           this->currentLine->propBuf[this->pos + delta] & PC_WCHAR2)
        delta++;

    col2 = this->currentLine->COLPOS(this->pos + delta);
    if (col < this->currentColumn || col2 > this->COLS + this->currentColumn)
    {
        this->currentColumn = 0;
        if (col2 > this->COLS)
            ColumnSkip(col);
    }
    /* Arrange cursor */
    this->cursorY = this->currentLine->linenumber - this->topLine->linenumber;
    this->visualpos = this->currentLine->bwidth +
                     this->currentLine->COLPOS(this->pos) - this->currentColumn;
    this->cursorX = this->visualpos - this->currentLine->bwidth;
    
#ifdef DISPLAY_DEBUG
    fprintf(stderr,
            "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
            this->currentColumn, this->cursorX, this->visualpos, this->pos,
            this->currentLine->len);
#endif
}

void Buffer::ArrangeLine()
{
    if (this->LineCount() == 0)
        return;

    this->cursorY = this->currentLine->linenumber - this->topLine->linenumber;
    auto i = columnPos(this->currentLine, this->currentColumn + this->visualpos - this->currentLine->bwidth);
    auto cpos = this->currentLine->COLPOS(i) - this->currentColumn;
    if (cpos >= 0)
    {
        this->cursorX = cpos;
        this->pos = i;
    }
    else if (this->currentLine->len > i)
    {
        this->cursorX = 0;
        this->pos = i + 1;
    }
    else
    {
        this->cursorX = 0;
        this->pos = 0;
    }

#ifdef DISPLAY_DEBUG
    fprintf(stderr,
            "arrangeLine: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
            this->currentColumn, this->cursorX, this->visualpos, this->pos,
            this->currentLine->len);
#endif
}
