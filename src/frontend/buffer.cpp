#include <sstream>
#include <assert.h>
#include <unistd.h>

#include "history.h"
#include "regex.h"
#include "frontend/buffer.h"
#include "frontend/display.h"
#include "frontend/line.h"
#include "frontend/lineinput.h"
#include "frontend/screen.h"
#include "urimethod.h"
#include "public.h"
#include "indep.h"
#include "file.h"
#include "html/image.h"
#include "html/html.h"
#include "ctrlcode.h"
#include "stream/local_cgi.h"
#include "html/anchor.h"
#include "stream/url.h"
#include "loader.h"
#include "mime/mimetypes.h"
#include "charset.h"
#include "html/html.h"
#include "html/form.h"


#include "stream/input_stream.h"
#include <iostream>

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
    currentURL.scheme = SCM_UNKNOWN;
    baseURL = {};
    baseTarget = {};
    bufferprop = BP_NORMAL;
    trbyte = 0;
    auto_detect = WcOption.auto_detect;
}

Buffer::~Buffer()
{
    ImageManager::Instance().deleteImage(this);
    ClearLines();
    if (savecache.size())
        unlink(savecache.c_str());
    if (pagerSource)
        pagerSource = nullptr;
    if (sourcefile.size() &&
        (real_type.empty() || real_type.starts_with("image/")))
    {
        if (real_scheme != SCM_LOCAL)
            unlink(sourcefile.c_str());
    }
    if (header_source.size())
        unlink(header_source.c_str());
    if (mailcap_source.size())
        unlink(mailcap_source.c_str());
}

void Buffer::TmpClear()
{
    if (this->pagerSource == NULL && this->WriteBufferCache() == 0)
    {
        topLine = NULL;
        currentLine = NULL;
        lines.clear();
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

    for (auto &l : lines)
    {
        if (fwrite1(l->real_linenumber, cache) ||
            fwrite1(l->usrflags, cache) ||
            fwrite1(l->width(), cache) ||
            fwrite1(l->len(), cache) ||
            fwrite1(l->bpos, cache) || fwrite1(l->bwidth, cache))
            goto _error;
        if (l->bpos == 0)
        {
            if (fwrite(l->lineBuf(), 1, l->len(), cache) < l->len() ||
                fwrite(l->propBuf(), sizeof(Lineprop), l->len(), cache) < l->len())
                goto _error;
        }

        colorflag = l->colorBuf() ? 1 : 0;
        if (fwrite1(colorflag, cache))
            goto _error;
        if (colorflag)
        {
            if (l->bpos == 0)
            {
                if (fwrite(l->colorBuf(), sizeof(Linecolor), l->len(), cache) <
                    l->len())
                    goto _error;
            }
        }
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

    // LinePtr basel = NULL;
    // long clnum, tlnum;
    // int colorflag;

    // if (this->savecache.empty())
    //     return -1;

    // auto cache = fopen(this->savecache.c_str(), "r");
    // if (cache == NULL || fread1(clnum, cache) || fread1(tlnum, cache))
    // {
    //     this->savecache.clear();
    //     return -1;
    // }

    // LinePtr prevl = nullptr;
    // LinePtr l = nullptr;
    // for (int lnum = 0; !feof(cache); ++lnum, prevl = l)
    // {
    //     l = New(Line);
    //     l->prev = prevl;
    //     if (prevl)
    //         prevl->next = l;
    //     else
    //         this->SetFirstLine(l);
    //     l->linenumber = lnum;
    //     if (lnum == clnum)
    //         this->currentLine = l;
    //     if (lnum == tlnum)
    //         this->topLine = l;
    //     if (fread1(l->real_linenumber, cache) ||
    //         fread1(l->usrflags, cache) ||
    //         fread1(l->width, cache) ||
    //         fread1(l->len(), cache) ||
    //         fread1(l->len(), cache) ||
    //         fread1(l->bpos, cache) || fread1(l->bwidth, cache))
    //         break;
    //     if (l->bpos == 0)
    //     {
    //         basel = l;
    //         l->lineBuf() = NewAtom_N(char, l->len() + 1);
    //         fread(l->lineBuf(), 1, l->len(), cache);
    //         l->lineBuf()[l->len()] = '\0';
    //         l->propBuf() = NewAtom_N(Lineprop, l->len());
    //         fread(l->propBuf(), sizeof(Lineprop), l->len(), cache);
    //     }
    //     else if (basel)
    //     {
    //         l->lineBuf() = basel->lineBuf() + l->bpos;
    //         l->propBuf() = basel->propBuf() + l->bpos;
    //     }
    //     else
    //         break;
    //     #ifdef USE_ANSI_COLOR
    //     if (fread1(colorflag, cache))
    //         break;
    //     if (colorflag)
    //     {
    //         if (l->bpos == 0)
    //         {
    //             l->colorBuf() = NewAtom_N(Linecolor, l->len());
    //             fread(l->colorBuf(), sizeof(Linecolor), l->len(), cache);
    //         }
    //         else
    //             l->colorBuf() = basel->colorBuf() + l->bpos;
    //     }
    //     else
    //     {
    //         l->colorBuf() = NULL;
    //     }
    //     #endif
    // }
    // prevl->next = NULL;
    // this->SetLastLine(prevl);

    // fclose(cache);
    // unlink(this->savecache.c_str());
    // this->savecache.clear();
    // return 0;
}

BufferPtr Buffer::Copy()
{
    auto copy = newBuffer(currentURL);
    copy->CopyFrom(shared_from_this());
    return copy;
}

void Buffer::CopyFrom(BufferPtr src)
{
    src->ReadBufferCache();

    // *this = *src;
    filename = src->filename;
    buffername = src->buffername;

    need_reshape = src->need_reshape;
    lines = src->lines;

    // scroll
    topLine = src->topLine;
    // cursor ?
    currentLine = src->currentLine;

    width = src->width;
    height = src->height;

    // mimetype: text/plain
    type = src->type;
    real_type = src->real_type;

    bufferprop = src->bufferprop;
    currentColumn = src->currentColumn;
    pos = src->pos;
    visualpos = src->visualpos;
    rect = src->rect;
    pagerSource = src->pagerSource;
    href = src->href;
    name = src->name;
    img = src->img;
    formitem = src->formitem;
    prevhseq = src->prevhseq;

    linklist = src->linklist;
    formlist = src->formlist;
    maplist = src->maplist;
    hmarklist = src->hmarklist;
    imarklist = src->imarklist;
    currentURL = src->currentURL;
    baseURL = src->baseURL;
    baseTarget = src->baseTarget;
    real_scheme = src->real_scheme;
    sourcefile = src->sourcefile;

    trbyte = src->trbyte;
    check_url = src->check_url;
    document_charset = src->document_charset;
    auto_detect = src->auto_detect;
    form_submit = src->form_submit;
    savecache = src->savecache;
    edit = src->edit;
    mailcap_source = src->mailcap_source;
    header_source = src->header_source;
    search_header = src->search_header;
    ssl_certificate = src->ssl_certificate;
    image_flag = src->image_flag;
    image_loaded = src->image_loaded;
    submit = src->submit;
    undo = src->undo;
    event = src->event;
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

    return &currentURL;
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
        auto a = al.anchors[s];
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
        auto a = al.anchors[s];
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

/*
 * Buffer creation
 */
BufferPtr
newBuffer(const URL &url)
{
    auto n = std::make_shared<Buffer>();
    n->width = w3mApp::Instance().INIT_BUFFER_WIDTH();
    n->currentURL = url;
    return n;
}

/*
 * Create null buffer
 */
BufferPtr
nullBuffer(void)
{
    auto b = newBuffer({});
    b->buffername = "*Null*";
    return b;
}

LinePtr Buffer::CurrentLineSkip(LinePtr line, int offset, int last)
{
    int i, n;
    auto l = find(line);

    if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    {
        n = line->linenumber + offset + this->rect.lines;
        if (this->LastLine()->linenumber < n)
            getNextPage(shared_from_this(), n - this->LastLine()->linenumber);
        while ((last || (this->LastLine()->linenumber < n)) &&
               (getNextPage(shared_from_this(), 1) != NULL))
            ;
        if (last)
            l = find(this->LastLine());
    }

    if (offset == 0)
        return *l;
    if (offset > 0)
        for (i = 0; i < offset && (*l) != lines.back(); i++, ++l)
        {
        }
    else
        for (i = 0; i < -offset && l != lines.begin(); i++, --l)
        {
        }
    return *l;
}

void Buffer::LineSkip(LinePtr line, int offset, int last)
{
    auto l = find(CurrentLineSkip(line, offset, last));
    int i;
    if (!w3mApp::Instance().nextpage_topline)
        for (i = this->rect.lines - 1 - (this->LastLine()->linenumber - (*l)->linenumber);
             i > 0 && l != lines.begin();
             i--, --l)
        {
        };
    topLine = *l;
}

/*
 * gotoLine: go to line number
 */
void Buffer::GotoLine(int n, bool topline)
{
    char msg[32];
    LinePtr l = this->FirstLine();
    if (l == NULL)
        return;
    if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    {
        if (this->LastLine()->linenumber < n)
            getNextPage(shared_from_this(), n - this->LastLine()->linenumber);
        while ((this->LastLine()->linenumber < n) &&
               (getNextPage(shared_from_this(), 1) != NULL))
            ;
    }
    if (l->linenumber > n)
    {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->linenumber);
        set_delayed_message(msg);
        this->topLine = this->currentLine = l;
        return;
    }
    if (this->LastLine()->linenumber < n)
    {
        l = this->LastLine();
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", this->LastLine()->linenumber);
        set_delayed_message(msg);
        this->currentLine = l;
        LineSkip(this->currentLine, -(this->rect.lines - 1), false);
        return;
    }
    auto it = find(l);
    for (; it != lines.end(); ++it)
    {
        if ((*it)->linenumber >= n)
        {
            this->currentLine = *it;
            if (n < this->topLine->linenumber ||
                this->topLine->linenumber + this->rect.lines <= n)
                LineSkip(*it, -(this->rect.lines + 1) / 2, false);
            break;
        }
    }

    if (topline)
    {
        LineSkip(TopLine(), CurrentLine()->linenumber - TopLine()->linenumber, false);
    }
}

void Buffer::Scroll(int n)
{
    auto lnum = currentLine->linenumber;
    auto top = topLine;
    this->LineSkip(top, n, false);
    if (this->topLine == top)
    {
        lnum += n;
        if (lnum < this->topLine->linenumber)
            lnum = this->topLine->linenumber;
        else if (lnum > this->LastLine()->linenumber)
            lnum = this->LastLine()->linenumber;
    }
    else
    {
        auto tlnum = this->topLine->linenumber;
        auto llnum = this->topLine->linenumber + this->rect.lines - 1;
        int diff_n;
        if (w3mApp::Instance().nextpage_topline)
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

void Buffer::NScroll(int n)
{
    if (this->LineCount() == 0)
        return;

    this->Scroll(n);

    this->ArrangeLine();
    if (n > 0)
    {
        if (this->currentLine->bpos &&
            this->currentLine->bwidth >= this->currentColumn + this->visualpos)
            this->CursorDown(1);
        else
        {
            while (NextLine(this->currentLine) && NextLine(this->currentLine)->bpos &&
                   this->currentLine->bend() <
                       this->currentColumn + this->visualpos)
                this->CursorDown0(1);
        }
    }
    else
    {
        if (this->currentLine->bend() <
            this->currentColumn + this->visualpos)
            this->CursorUp(1);
        else
        {
            while (PrevLine(this->currentLine) && this->currentLine->bpos &&
                   this->currentLine->bwidth >=
                       this->currentColumn + this->visualpos)
                this->CursorUp0(1);
        }
    }
}

/*
 * gotoRealLine: go to real line number
 */
void Buffer::GotoRealLine(int n)
{
    char msg[32];
    LinePtr l = this->FirstLine();

    if (l == NULL)
        return;
    if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    {
        if (this->LastLine()->real_linenumber < n)
            getNextPage(shared_from_this(), n - this->LastLine()->real_linenumber);
        while ((this->LastLine()->real_linenumber < n) &&
               (getNextPage(shared_from_this(), 1) != NULL))
            ;
    }
    if (l->real_linenumber > n)
    {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->real_linenumber);
        set_delayed_message(msg);
        this->topLine = this->currentLine = l;
        return;
    }
    if (this->LastLine()->real_linenumber < n)
    {
        l = this->LastLine();
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", this->LastLine()->real_linenumber);
        set_delayed_message(msg);
        this->currentLine = l;
        LineSkip(this->currentLine, -(this->rect.lines - 1),
                 false);
        return;
    }

    auto it = find(l);
    for (; it != lines.end(); ++it)
    {
        if ((*it)->real_linenumber >= n)
        {
            this->currentLine = *it;
            if (n < this->topLine->real_linenumber ||
                this->topLine->real_linenumber + this->rect.lines <= n)
                LineSkip(*it, -(this->rect.lines + 1) / 2, false);
            break;
        }
    }
}

/*
 * Reshape HTML buffer
 */
// void Buffer::Reshape()
// {
//     uint8_t old_auto_detect = WcOption.auto_detect;

//     if (!need_reshape)
//         return;
//     need_reshape = false;

//     this->width = INIT_BUFFER_WIDTH();
//     if (this->sourcefile.empty())
//         return;
//     auto f = URLFile::OpenFile(this->mailcap_source.size() ? this->mailcap_source.c_str() : this->sourcefile.c_str());
//     if (f->stream == NULL)
//         return;

//     auto sbuf = this->Copy();
//     this->ClearLines();
//     while (this->frameset)
//     {
//         deleteFrameSet(this->frameset);
//         this->frameset = popFrameTree(&(this->frameQ));
//     }

//     this->href.clear();
//     this->name.clear();
//     this->img.clear();
//     this->formitem.clear();
//     this->formlist = NULL;
//     this->linklist.clear();
//     this->maplist = NULL;
//     this->hmarklist.clear();
//     this->imarklist.clear();

//     if (this->header_source.size())
//     {
//         if (this->currentURL.scheme != SCM_LOCAL ||
//             this->mailcap_source.size() || this->currentURL.path == "-")
//         {
//             auto h = URLFile::OpenFile(this->header_source);
//             if (h->stream)
//             {
//                 readHeader(h, shared_from_this(), true, NULL);
//             }
//         }
//         else if (this->search_header) /* -m option */
//             readHeader(f, shared_from_this(), true, NULL);
//     }

//     WcOption.auto_detect = WC_OPT_DETECT_OFF;
//     w3mApp::Instance().UseContentCharset = false;

//     if (is_html_type(this->type))
//         loadHTMLBuffer(f, shared_from_this());
//     else
//         loadBuffer(f, shared_from_this());

//     WcOption.auto_detect = (AutoDetectTypes)old_auto_detect;
//     w3mApp::Instance().UseContentCharset = true;

//     this->height = (rect.lines - 1) + 1;
//     if (this->FirstLine() && sbuf->FirstLine())
//     {
//         LinePtr cur = sbuf->currentLine;
//         int n;

//         this->pos = sbuf->pos + cur->bpos;
//         while (cur->bpos && PrevLine(cur))
//             cur = PrevLine(cur);
//         if (cur->real_linenumber > 0)
//             this->GotoRealLine(cur->real_linenumber);
//         else
//             this->GotoLine(cur->linenumber);
//         n = (this->currentLine->linenumber - this->topLine->linenumber) - (cur->linenumber - sbuf->topLine->linenumber);
//         if (n)
//         {
//             LineSkip(this->topLine, n, false);
//             if (cur->real_linenumber > 0)
//                 this->GotoRealLine(cur->real_linenumber);
//             else
//                 this->GotoLine(cur->linenumber);
//         }
//         this->pos -= this->currentLine->bpos;
//         if (w3mApp::Instance().FoldLine && !is_html_type(this->type))
//             this->currentColumn = 0;
//         else
//             this->currentColumn = sbuf->currentColumn;
//         ArrangeCursor();
//     }
//     if (this->check_url & CHK_URL)
//         chkURLBuffer(shared_from_this());

//     if (this->check_url & CHK_NMID)
//         chkNMIDBuffer(shared_from_this());
//     if (this->real_scheme == SCM_NNTP || this->real_scheme == SCM_NEWS)
//         reAnchorNewsheader(shared_from_this());

//     formResetBuffer(shared_from_this(), sbuf->formitem);
// }

void set_buffer_environ(const BufferPtr &buf)
{
    static BufferPtr prev_buf = NULL;
    static LinePtr prev_line = NULL;
    static int prev_pos = -1;
    LinePtr l;

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
    l = buf->CurrentLine();
    if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos))
    {
        char *s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", (char *)(s ? s : ""));
        auto a = buf->href.RetrieveAnchor(buf->CurrentPoint());
        if (a)
        {
            auto pu = URL::Parse(a->url, buf->BaseURL());
            set_environ("W3M_CURRENT_LINK", pu.ToStr()->ptr);
        }
        else
            set_environ("W3M_CURRENT_LINK", "");
        a = buf->img.RetrieveAnchor(buf->CurrentPoint());
        if (a)
        {
            auto pu = URL::Parse(a->url, buf->BaseURL());
            set_environ("W3M_CURRENT_IMG", pu.ToStr()->ptr);
        }
        else
            set_environ("W3M_CURRENT_IMG", "");
        a = buf->formitem.RetrieveAnchor(buf->CurrentPoint());
        if (a)
            set_environ("W3M_CURRENT_FORM", a->item->ToStr()->ptr);
        else
            set_environ("W3M_CURRENT_FORM", "");
        set_environ("W3M_CURRENT_LINE", Sprintf("%d",
                                                l->real_linenumber)
                                            ->ptr);
        set_environ("W3M_CURRENT_COLUMN", Sprintf("%d",
                                                  buf->currentColumn + buf->rect.cursorX + 1)
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

const char *NullLine = "";
Lineprop NullProp[] = {P_UNKNOWN};

void Buffer::AddNewLine(const PropertiedString &lineBuffer, int real_linenumber)
{
    auto l = std::make_shared<Line>(lineBuffer);
    if (lines.empty())
    {
        topLine = l;
    }
    currentLine = l;
    lines.push_back(l);

    // 1 origin linenumber
    l->linenumber = this->LineCount();
    if (real_linenumber >= 0)
    {
        l->real_linenumber = real_linenumber;
    }
    else
    {
        l->real_linenumber = l->linenumber;
    }
}

void Buffer::AddNewLineFixedWidth(const PropertiedString &lineBuffer, int real_linenumber, int width)
{
    // TODO:
    assert(false);
    // separate line
    // int bpos = 0;
    // int bwidth = 0;
    // while (1)
    // {
    //     auto l = CurrentLine();
    //     l->bpos = bpos;
    //     l->bwidth = bwidth;
    //     auto i = columnLen(l, width);
    //     if (i == 0)
    //     {
    //         i++;
    //         while (i < l->len() && p[i] & PC_WCHAR2)
    //             i++;
    //     }
    //     l->buffer.len = i;
    //     l->width = l->COLPOS(l->len());
    //     if (pos <= i)
    //         return;
    //     bpos += l->len();
    //     bwidth += l->width;
    //     s += i;
    //     p += i;
    //     if (c)
    //         c += i;
    //     pos -= i;
    //     AddLine(s, p, c, pos, nlines);
    // }
}

void Buffer::SavePosition()
{
    if (LineCount() == 0)
        return;

    auto b = undo.size() ? &undo.back() : nullptr;
    if (b && b->top_linenumber == TOP_LINENUMBER() &&
        b->cur_linenumber == CUR_LINENUMBER() &&
        b->currentColumn == currentColumn &&
        b->pos == pos)
    {
        return;
    }

    undo.push_back({});

    b = &undo.back();
    b->top_linenumber = TOP_LINENUMBER();
    b->cur_linenumber = CUR_LINENUMBER();
    b->currentColumn = this->currentColumn;
    b->pos = this->pos;
    b->bpos = this->currentLine ? this->currentLine->bpos : 0;
}

void Buffer::CursorUp0(int n)
{
    if (this->rect.cursorY > 0)
        CursorUpDown(-1);
    else
    {
        this->LineSkip(this->topLine, -n, false);
        if (PrevLine(this->currentLine) != NULL)
            this->currentLine = PrevLine(this->currentLine);
        ArrangeLine();
    }
}

void Buffer::CursorUp(int n)
{
    LinePtr l = this->currentLine;
    if (this->LineCount() == 0)
        return;
    while (PrevLine(this->currentLine) && this->currentLine->bpos)
        CursorUp0(n);
    if (this->currentLine == this->FirstLine())
    {
        this->GotoLine(l->linenumber);
        ArrangeLine();
        return;
    }
    CursorUp0(n);
    while (PrevLine(this->currentLine) && this->currentLine->bpos &&
           this->currentLine->bwidth >= this->currentColumn + this->visualpos)
        CursorUp0(n);
}

void Buffer::CursorDown0(int n)
{
    if (this->rect.cursorY < this->rect.lines - 1)
        CursorUpDown(1);
    else
    {
        this->LineSkip(this->topLine, n, false);
        if (NextLine(this->currentLine) != NULL)
            this->currentLine = NextLine(this->currentLine);
        ArrangeLine();
    }
}

void Buffer::CursorDown(int n)
{
    LinePtr l = this->currentLine;
    if (this->LineCount() == 0)
        return;
    while (NextLine(this->currentLine) && NextLine(this->currentLine)->bpos)
        CursorDown0(n);
    if (this->currentLine == this->LastLine())
    {
        this->GotoLine(l->linenumber);
        ArrangeLine();
        return;
    }
    CursorDown0(n);
    while (NextLine(this->currentLine) && NextLine(this->currentLine)->bpos &&
           this->currentLine->bend() <
               this->currentColumn + this->visualpos)
        CursorDown0(n);
}

void Buffer::CursorUpDown(int n)
{
    LinePtr cl = this->currentLine;

    if (this->LineCount() == 0)
        return;
    if ((this->currentLine = this->CurrentLineSkip(cl, n, false)) == cl)
        return;
    ArrangeLine();
}

void Buffer::CursorRight(int n)
{
    int i, delta = 1, cpos, vpos2;
    LinePtr l = this->currentLine;
    Lineprop *p;

    if (this->LineCount() == 0)
        return;
    if (this->pos == l->len() && !(NextLine(l) && NextLine(l)->bpos))
        return;
    i = this->pos;
    p = l->propBuf();

    while (i + delta < l->len() && p[i + delta] & PC_WCHAR2)
        delta++;

    if (i + delta < l->len())
    {
        this->pos = i + delta;
    }
    else if (l->len() == 0)
    {
        this->pos = 0;
    }
    else if (NextLine(l) && NextLine(l)->bpos)
    {
        CursorDown0(1);
        this->pos = 0;
        ArrangeCursor();
        return;
    }
    else
    {
        this->pos = l->len() - 1;
        while (this->pos && p[this->pos] & PC_WCHAR2)
            this->pos--;
    }
    cpos = l->COLPOS(this->pos);
    this->visualpos = l->bwidth + cpos - this->currentColumn;
    delta = 1;

    while (this->pos + delta < l->len() && p[this->pos + delta] & PC_WCHAR2)
        delta++;

    vpos2 = l->COLPOS(this->pos + delta) - this->currentColumn - 1;
    if (vpos2 >= this->rect.cols && n)
    {
        ColumnSkip(n + (vpos2 - this->rect.cols) - (vpos2 - this->rect.cols) % n);
        this->visualpos = l->bwidth + cpos - this->currentColumn;
    }
    this->rect.cursorX = this->visualpos - l->bwidth;

    m_redraw = B_NORMAL;
}

void Buffer::CursorLeft(int n)
{
    int i, delta = 1, cpos;
    LinePtr l = this->currentLine;
    Lineprop *p;

    if (this->LineCount() == 0)
        return;
    i = this->pos;
    p = l->propBuf();

    while (i - delta > 0 && p[i - delta] & PC_WCHAR2)
        delta++;

    if (i >= delta)
        this->pos = i - delta;
    else if (PrevLine(l) && l->bpos)
    {
        CursorUp0(-1);
        this->pos = this->currentLine->len() - 1;
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
    this->rect.cursorX = this->visualpos - l->bwidth;

    m_redraw = B_NORMAL;
}

void Buffer::CursorXY(int x, int y)
{
    // to buffer local
    x -= rect.rootX;
    y -= rect.rootY;

    CursorUpDown(y - rect.cursorY);

    if (this->rect.cursorX > x)
    {
        while (this->rect.cursorX > x)
            CursorLeft(this->rect.cols / 2);
    }
    else if (this->rect.cursorX < x)
    {
        while (this->rect.cursorX < x)
        {
            int oldX = this->rect.cursorX;

            CursorRight(this->rect.cols / 2);

            if (oldX == this->rect.cursorX)
                break;
        }
        if (this->rect.cursorX > x)
            CursorLeft(this->rect.cols / 2);
    }
}

/*
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void Buffer::ArrangeCursor()
{
    if (this->currentLine == NULL)
        return;

    int col, col2, pos;
    int delta = 1;
    m_redraw = B_NORMAL;
    /* Arrange line */
    if (this->currentLine->linenumber - this->topLine->linenumber >= this->rect.lines || this->currentLine->linenumber < this->topLine->linenumber)
    {
        this->LineSkip(this->currentLine, 0, false);
    }
    /* Arrange column */
    while (this->pos < 0 && PrevLine(this->currentLine) && this->currentLine->bpos)
    {
        pos = this->pos + PrevLine(this->currentLine)->len();
        CursorUp0(1);
        this->pos = pos;
    }
    while (this->pos >= this->currentLine->len() && NextLine(this->currentLine) &&
           NextLine(this->currentLine)->bpos)
    {
        pos = this->pos - this->currentLine->len();
        CursorDown0(1);
        this->pos = pos;
    }
    if (this->currentLine->len() == 0 || this->pos < 0)
        this->pos = 0;
    else if (this->pos >= this->currentLine->len())
        this->pos = this->currentLine->len() - 1;

    while (this->pos > 0 && this->currentLine->propBuf()[this->pos] & PC_WCHAR2)
        this->pos--;

    col = this->currentLine->COLPOS(this->pos);

    while (this->pos + delta < this->currentLine->len() &&
           this->currentLine->propBuf()[this->pos + delta] & PC_WCHAR2)
        delta++;

    col2 = this->currentLine->COLPOS(this->pos + delta);
    if (col < this->currentColumn || col2 > this->rect.cols + this->currentColumn)
    {
        this->currentColumn = 0;
        if (col2 > this->rect.cols)
            ColumnSkip(col);
    }
    /* Arrange cursor */
    this->rect.cursorY = this->currentLine->linenumber - this->topLine->linenumber;
    this->visualpos = this->currentLine->bwidth +
                      this->currentLine->COLPOS(this->pos) - this->currentColumn;
    this->rect.cursorX = this->visualpos - this->currentLine->bwidth;

#ifdef DISPLAY_DEBUG
    fprintf(stderr,
            "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
            this->currentColumn, this->cursorX, this->visualpos, this->pos,
            this->currentLine->len());
#endif
}

void Buffer::ArrangeLine()
{
    if (this->LineCount() == 0)
        return;

    m_redraw = B_FORCE_REDRAW;
    this->rect.cursorY = this->currentLine->linenumber - this->topLine->linenumber;
    auto i = columnPos(this->currentLine, this->currentColumn + this->visualpos - this->currentLine->bwidth);
    auto cpos = this->currentLine->COLPOS(i) - this->currentColumn;
    if (cpos >= 0)
    {
        this->rect.cursorX = cpos;
        this->pos = i;
    }
    else if (this->currentLine->len() > i)
    {
        this->rect.cursorX = 0;
        this->pos = i + 1;
    }
    else
    {
        this->rect.cursorX = 0;
        this->pos = 0;
    }

#ifdef DISPLAY_DEBUG
    fprintf(stderr,
            "arrangeLine: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
            this->currentColumn, this->cursorX, this->visualpos, this->pos,
            this->currentLine->len());
#endif
}

void Buffer::DumpSource()
{
    FILE *f;
    char c;
    if (sourcefile.empty())
        return;
    f = fopen(sourcefile.c_str(), "r");
    if (f == NULL)
        return;
    while (c = fgetc(f), !feof(f))
    {
        putchar(c);
    }
    fclose(f);
}

#include <math.h>

///
/// draw a line
///
void Buffer::DrawLine(LinePtr l, int line)
{
    if (l == NULL)
    {
        return;
    }

    ///
    /// show line number
    ///
    Screen::Instance().Move(line, 0);
    if (w3mApp::Instance().showLineNum)
    {
        rect.updateRootX(this->LastLine()->real_linenumber);

        char tmp[16];
        if (l->real_linenumber && !l->bpos)
            sprintf(tmp, "%*ld:", rect.rootX - 1, l->real_linenumber);
        else
            sprintf(tmp, "%*s ", rect.rootX - 1, "");
        Screen::Instance().Puts(tmp);
    }

    l->CalcWidth();
    if (l->len() == 0 || l->width() - 1 < currentColumn)
    {
        Screen::Instance().CtrlToEolWithBGColor();
        return;
    }

    ///
    /// draw line
    ///
    Screen::Instance().Move(line, rect.rootX);
    auto pos = columnPos(l, currentColumn);
    auto p = &(l->lineBuf()[pos]);
    auto pr = &(l->propBuf()[pos]);
    Linecolor *pc;
    if (w3mApp::Instance().useColor && l->colorBuf())
        pc = &(l->colorBuf()[pos]);
    else
        pc = NULL;

    auto rcol = l->COLPOS(pos);
    int delta = 1;
    int vpos = -1;
    for (int j = 0; rcol - currentColumn < this->rect.cols && pos + j < l->len(); j += delta)
    {
        if (w3mApp::Instance().useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED))
        {
            auto a = this->href.RetrieveAnchor({l->linenumber, pos + j});
            if (a)
            {
                auto url = URL::Parse(a->url, this->BaseURL());
                if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->c_str()))
                {
                    for (int k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }

        delta = wtf_len((uint8_t *)&p[j]);
        switch (delta)
        {
        case 3:
        {
            auto a = 0;
        }
        break;

        case 4:
        {
            auto b = 0;
        }
        break;
        }

        int ncol = l->COLPOS(pos + j + delta);
        if (ncol - currentColumn > this->rect.cols)
            break;

        if (pc)
            do_color(pc[j]);

        if (rcol < currentColumn)
        {
            for (rcol = currentColumn; rcol < ncol; rcol++)
                addChar(' ');
            continue;
        }
        if (p[j] == '\t')
        {
            for (; rcol < ncol; rcol++)
                addChar(' ');
        }
        else
        {
            addMChar(&p[j], pr[j], delta);
        }
        rcol = ncol;
    }

    clear_effect();

    if (rcol - currentColumn < this->rect.cols)
        Screen::Instance().CtrlToEolWithBGColor();
}

int Buffer::DrawLineRegion(LinePtr l, int i, int bpos, int epos)
{
    if (l == NULL)
        return 0;

    int pos = columnPos(l, currentColumn);
    auto p = &(l->lineBuf()[pos]);
    auto pr = &(l->propBuf()[pos]);
    Linecolor *pc;
    if (w3mApp::Instance().useColor && l->colorBuf())
        pc = &(l->colorBuf()[pos]);
    else
        pc = NULL;

    int rcol = l->COLPOS(pos);
    int bcol = bpos - pos;
    int ecol = epos - pos;
    int delta = 1;
    int vpos = -1;
    for (int j = 0; rcol - currentColumn < this->rect.cols && pos + j < l->len(); j += delta)
    {
        if (w3mApp::Instance().useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED))
        {
            auto a = this->href.RetrieveAnchor({l->linenumber, pos + j});
            if (a)
            {
                auto url = URL::Parse(a->url, this->BaseURL());
                if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->c_str()))
                {
                    for (auto k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }

        delta = wtf_len((uint8_t *)&p[j]);
        auto ncol = l->COLPOS(pos + j + delta);
        if (ncol - currentColumn > this->rect.cols)
            break;
        if (pc)
            do_color(pc[j]);
        if (j >= bcol && j < ecol)
        {
            if (rcol < currentColumn)
            {
                Screen::Instance().Move(i, rect.rootX);
                for (rcol = currentColumn; rcol < ncol; rcol++)
                    addChar(' ');
                continue;
            }
            Screen::Instance().Move(i, rcol - currentColumn + rect.rootX);
            if (p[j] == '\t')
            {
                for (; rcol < ncol; rcol++)
                    addChar(' ');
            }
            else

                addMChar(&p[j], pr[j], delta);
        }
        rcol = ncol;
    }

    clear_effect();

    return rcol - currentColumn;
}

bool Buffer::MoveLeftWord(int n)
{
    if (this->LineCount() == 0)
        return false;

    for (int i = 0; i < n; i++)
    {
        auto pline = this->CurrentLine();
        auto ppos = this->pos;
        if (prev_nonnull_line(shared_from_this(), this->CurrentLine()) < 0)
            goto end;

        while (1)
        {
            auto l = this->CurrentLine();
            auto lb = l->lineBuf();
            while (this->pos > 0)
            {
                int tmp = this->pos;
                prevChar(&tmp, l);
                if (is_wordchar(getChar(&lb[tmp])))
                    break;
                this->pos = tmp;
            }
            if (this->pos > 0)
                break;
            if (prev_nonnull_line(shared_from_this(), this->PrevLine(this->CurrentLine())) < 0)
            {
                this->SetCurrentLine(pline);
                this->pos = ppos;
                goto end;
            }
            this->pos = this->CurrentLine()->len();
        }
        {
            auto l = this->CurrentLine();
            auto lb = l->lineBuf();
            while (this->pos > 0)
            {
                int tmp = this->pos;
                prevChar(&tmp, l);
                if (!is_wordchar(getChar(&lb[tmp])))
                    break;
                this->pos = tmp;
            }
        }
    }
end:
    this->ArrangeCursor();
    return true;
}

bool Buffer::MoveRightWord(int n)
{
    if (this->LineCount() == 0)
        return false;

    for (int i = 0; i < n; i++)
    {
        auto pline = this->CurrentLine();
        auto ppos = this->pos;
        if (next_nonnull_line(shared_from_this(), this->CurrentLine()) < 0)
            goto end;

        auto l = this->CurrentLine();
        auto lb = l->lineBuf();
        while (this->pos < l->len() &&
               is_wordchar(getChar(&lb[this->pos])))
            nextChar(&this->pos, l);

        while (1)
        {
            while (this->pos < l->len() &&
                   !is_wordchar(getChar(&lb[this->pos])))
                nextChar(&this->pos, l);
            if (this->pos < l->len())
                break;
            if (next_nonnull_line(shared_from_this(), this->NextLine(this->CurrentLine())) < 0)
            {
                this->SetCurrentLine(pline);
                this->pos = ppos;
                goto end;
            }
            this->pos = 0;
            l = this->CurrentLine();
            lb = l->lineBuf();
        }
    }
end:
    this->ArrangeCursor();
    return true;
}

void Buffer::resetPos(int i)
{
    auto top = std::make_shared<Line>();

    auto &b = undo[i];
    top->linenumber = b.top_linenumber;

    auto cur = std::make_shared<Line>();
    cur->linenumber = b.cur_linenumber;
    cur->bpos = b.bpos;

    BufferPtr newBuf = std::make_shared<Buffer>();
    newBuf->SetTopLine(top);
    newBuf->SetCurrentLine(cur);
    newBuf->pos = b.pos;
    newBuf->currentColumn = b.currentColumn;
    restorePosition(newBuf);

    // TODO: erase
    m_redraw = B_FORCE_REDRAW;
}

void Buffer::undoPos(int prec_num)
{
    if (LineCount() == 0)
        return;
    if (undo.size() < 2)
    {
        return;
    }

    if (prec_num <= 0)
    {
        prec_num = 1;
    }

    auto b = undo.end();
    --b;
    int j = undo.size() - 1;
    for (int i = 0; i < prec_num && b != undo.begin(); i++, --b, --j)
        ;

    resetPos(j);
}

void Buffer::redoPos()
{
    if (LineCount() == 0)
        return;

    // TODO:
    // BufferPos *b = undo;
    // if (!b || !b->next)
    //     return;
    // for (int i = 0; i < (prec_num() ? prec_num() : 1) && b->next; i++, b = b->next)
    //     ;
    // resetPos(b);
}

void Buffer::srch_nxtprv(bool reverse, int prec_num)
{
    static SearchFunc routine[2] = {forwardSearch, backwardSearch};

    if (searchRoutine == NULL)
    {
        /* FIXME: gettextize? */
        disp_message("No previous regular expression", true);
        return;
    }

    if (searchRoutine == backwardSearch)
        reverse = !reverse;
    if (reverse)
        pos += 1;
    auto result = srchcore(SearchString, routine[reverse], prec_num);
    if (result & SR_FOUND)
        CurrentLine()->clear_mark();

    m_redraw = B_NORMAL;
    disp_srchresult(result, (char *)(reverse ? "Backward: " : "Forward: "),
                    SearchString);
}

/* search by regular expression */
SearchResultTypes Buffer::srchcore(std::string_view str, SearchFunc func, int prec_num)
{
    if (str != NULL && str != SearchString)
        SearchString = str;
    if (SearchString.empty())
        return SR_NOTFOUND;

    auto converted = conv_search_string(SearchString, w3mApp::Instance().DisplayCharset);

    SearchResultTypes result = SR_NOTFOUND;
    for (int i = 0; i < prec_num; i++)
    {
        result = func(shared_from_this(), converted);
        if (i < prec_num - 1 && result & SR_FOUND)
            CurrentLine()->clear_mark();
    }
    return result;
}

int Buffer::dispincsrch(int ch, Str src, Lineprop *prop, int prec_num)
{
    static BufferPtr sbuf = std::shared_ptr<Buffer>(new Buffer);
    static LinePtr currentLine;
    static int s_pos;
    int do_next_search = false;

    if (ch == 0)
    {
        sbuf->COPY_BUFPOSITION_FROM(shared_from_this()); /* search starting point */
        currentLine = sbuf->CurrentLine();
        s_pos = sbuf->pos;
        return -1;
    }

    auto str = src->ptr;
    switch (ch)
    {
    case 022: /* C-r */
        searchRoutine = backwardSearch;
        do_next_search = true;
        break;
    case 023: /* C-s */
        searchRoutine = forwardSearch;
        do_next_search = true;
        break;

    default:
        if (ch >= 0)
            return ch; /* use InputKeymap */
    }

    if (do_next_search)
    {
        if (*str)
        {
            if (searchRoutine == forwardSearch)
                this->pos += 1;
            sbuf->COPY_BUFPOSITION_FROM(shared_from_this());
            if (srchcore(str, searchRoutine, prec_num) == SR_NOTFOUND && searchRoutine == forwardSearch)
            {
                this->pos -= 1;
                sbuf->COPY_BUFPOSITION_FROM(shared_from_this());
            }
            this->ArrangeCursor();
            m_redraw = B_FORCE_REDRAW;
            this->CurrentLine()->clear_mark();
            return -1;
        }
        else
            return 020; /* _prev completion for C-s C-s */
    }
    else if (*str)
    {
        this->COPY_BUFPOSITION_FROM(sbuf);
        this->ArrangeCursor();
        srchcore(str, searchRoutine, prec_num);
        this->ArrangeCursor();
        currentLine = this->CurrentLine();
        s_pos = this->pos;
    }
    m_redraw = B_FORCE_REDRAW;
    this->CurrentLine()->clear_mark();
    return -1;
}

void Buffer::isrch(SearchFunc func, const char *prompt, int prec_num)
{
    BufferPtr sbuf = std::shared_ptr<Buffer>(new Buffer);
    sbuf->COPY_BUFPOSITION_FROM(shared_from_this());
    dispincsrch(0, NULL, NULL, prec_num); /* initialize incremental search state */

    searchRoutine = func;
    auto callback = std::bind(&Buffer::dispincsrch, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    auto str = inputLineHistSearch(prompt, "", IN_STRING, w3mApp::Instance().TextHist, callback, prec_num);
    if (str == NULL)
    {
        COPY_BUFPOSITION_FROM(sbuf);
    }

    m_redraw = B_FORCE_REDRAW;
}

void Buffer::srch(SearchFunc func, const char *prompt, std::string_view str, int prec_num)
{
    int disp = false;
    if (str.empty())
    {
        str = inputStrHist(prompt, "", w3mApp::Instance().TextHist, prec_num);
        if (str.empty())
            str = SearchString;
        if (str.empty())
        {
            m_redraw = B_NORMAL;
            return;
        }
        disp = true;
    }

    auto p = this->pos;
    if (func == forwardSearch)
        this->pos += 1;
    auto result = srchcore(str, func, prec_num);
    if (result & SR_FOUND)
        this->CurrentLine()->clear_mark();
    else
        this->pos = p;
    m_redraw = B_NORMAL;
    if (disp)
        disp_srchresult(result, prompt, str);
    searchRoutine = func;
}

/* Go to specified line */
void Buffer::_goLine(std::string_view l, int prec_num)
{
    if (l.empty() || this->CurrentLine() == NULL)
    {
        return;
    }

    this->pos = 0;
    if (((l[0] == '^') || (l[0] == '$')) && prec_num)
    {
        this->GotoRealLine(prec_num);
    }
    else if (l[0] == '^')
    {
        this->SetCurrentLine(this->FirstLine());
        this->SetTopLine(this->FirstLine());
    }
    else if (l[0] == '$')
    {
        this->LineSkip(this->LastLine(),
                       -(this->rect.lines + 1) / 2, true);
        this->SetCurrentLine(this->LastLine());
    }
    else
    {
        this->GotoRealLine(atoi(l.data()));
    }
    this->ArrangeCursor();
}

static void
set_mark(LinePtr l, int pos, int epos)
{
    for (; pos < epos && pos < l->len(); pos++)
        l->propBuf()[pos] |= PE_MARK;
}

#ifdef USE_MIGEMO
/* Migemo: romaji --> kana+kanji in regexp */
static FILE *migemor = NULL, *migemow = NULL;
static int migemo_running;
static int migemo_pid = 0;

void init_migemo()
{
    migemo_active = migemo_running = use_migemo;
    if (migemor != NULL)
        fclose(migemor);
    if (migemow != NULL)
        fclose(migemow);
    migemor = migemow = NULL;
    if (migemo_pid)
        kill(migemo_pid, SIGKILL);
    migemo_pid = 0;
}

static int
open_migemo(char *migemo_command)
{
    migemo_pid = open_pipe_rw(&migemor, &migemow);
    if (migemo_pid < 0)
        goto err0;
    if (migemo_pid == 0)
    {
        /* child */
        setup_child(false, 2, -1);
        myExec(migemo_command);
        /* XXX: ifdef __EMX__, use start /f ? */
    }
    return 1;
err0:
    migemo_pid = 0;
    migemo_active = migemo_running = 0;
    return 0;
}

static char *
migemostr(char *str)
{
    Str tmp = NULL;
    if (migemor == NULL || migemow == NULL)
        if (open_migemo(migemo_command) == 0)
            return str;
    fprintf(migemow, "%s\n", conv_to_system(str));
again:
    if (fflush(migemow) != 0)
    {
        switch (errno)
        {
        case EINTR:
            goto again;
        default:
            goto err;
        }
    }
    tmp = Str_conv_from_system(Strfgets(migemor));
    Strchop(tmp);
    if (tmp->length == 0)
        goto err;
    return conv_search_string(tmp->ptr, SystemCharset);
err:
    /* XXX: backend migemo is not working? */
    init_migemo();
    migemo_active = migemo_running = 0;
    return str;
}
#endif /* USE_MIGEMO */

/* normalize search string */
std::string Buffer::conv_search_string(std::string_view str, CharacterEncodingScheme f_ces)
{
    if (w3mApp::Instance().SearchConv && !WcOption.pre_conv &&
        this->document_charset != f_ces)
        str = wtf_conv_fit(str.data(), this->document_charset);
    return std::string(str);
}

SearchResultTypes forwardSearch(const BufferPtr &buf, std::string_view str)
{
    char *first, *last;
    int wrapped = false;

    const char *p;
    if ((p = regexCompile(str.data(), w3mApp::Instance().IgnoreCase)) != NULL)
    {
        message(p, 0, 0);
        return SR_NOTFOUND;
    }

    auto l = buf->CurrentLine();
    if (l == NULL)
    {
        return SR_NOTFOUND;
    }

    auto pos = buf->pos;
    if (l->bpos)
    {
        pos += l->bpos;
        while (l->bpos && buf->PrevLine(l))
            l = buf->PrevLine(l);
    }

    auto begin = l;
    while (pos < l->len() && l->propBuf()[pos] & PC_WCHAR2)
        pos++;

    if (pos < l->len() && regexMatch(&l->lineBuf()[pos], l->len() - pos, 0) == 1)
    {
        matchedPosition(&first, &last);
        pos = first - l->lineBuf();
        while (pos >= l->len() && buf->NextLine(l) && buf->NextLine(l)->bpos)
        {
            pos -= l->len();
            l = buf->NextLine(l);
        }
        buf->pos = pos;
        if (l != buf->CurrentLine())
            buf->GotoLine(l->linenumber);
        buf->ArrangeCursor();
        set_mark(l, pos, pos + last - first);
        return SR_FOUND;
    }
    for (l = buf->NextLine(l);; l = buf->NextLine(l))
    {
        if (l == NULL)
        {
            if (buf->pagerSource)
            {
                l = getNextPage(buf, 1);
                if (l == NULL)
                {
                    if (w3mApp::Instance().WrapSearch && !wrapped)
                    {
                        l = buf->FirstLine();
                        wrapped = true;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else if (w3mApp::Instance().WrapSearch)
            {
                l = buf->FirstLine();
                wrapped = true;
            }
            else
            {
                break;
            }
        }
        if (l->bpos)
            continue;
        if (regexMatch(l->lineBuf(), l->len(), 1) == 1)
        {
            matchedPosition(&first, &last);
            pos = first - l->lineBuf();
            while (pos >= l->len() && buf->NextLine(l) && buf->NextLine(l)->bpos)
            {
                pos -= l->len();
                l = buf->NextLine(l);
            }
            buf->pos = pos;
            buf->SetCurrentLine(l);
            buf->GotoLine(l->linenumber);
            buf->ArrangeCursor();
            set_mark(l, pos, pos + last - first);
            return SR_FOUND | (wrapped ? SR_WRAPPED : SR_NONE);
        }
        if (wrapped && l == begin) /* no match */
            break;
    }
    return SR_NOTFOUND;
}

SearchResultTypes backwardSearch(const BufferPtr &buf, std::string_view str)
{
    const char *p;
    char *q, *found, *found_last, *first, *last;
    LinePtr l;
    LinePtr begin;
    int wrapped = false;
    int pos;

#ifdef USE_MIGEMO
    if (migemo_active > 0)
    {
        if (((p = regexCompile(migemostr(str), IgnoreCase)) != NULL) && ((p = regexCompile(str, IgnoreCase)) != NULL))
        {
            message(p, 0, 0);
            return SR_NOTFOUND;
        }
    }
    else
#endif
        if ((p = regexCompile(str.data(), w3mApp::Instance().IgnoreCase)) != NULL)
    {
        message(p, 0, 0);
        return SR_NOTFOUND;
    }
    l = buf->CurrentLine();
    if (l == NULL)
    {
        return SR_NOTFOUND;
    }
    pos = buf->pos;
    if (l->bpos)
    {
        pos += l->bpos;
        while (l->bpos && buf->PrevLine(l))
            l = buf->PrevLine(l);
    }
    begin = l;
    if (pos > 0)
    {
        pos--;
#ifdef USE_M17N
        while (pos > 0 && l->propBuf()[pos] & PC_WCHAR2)
            pos--;
#endif
        p = &l->lineBuf()[pos];
        found = NULL;
        found_last = NULL;
        q = l->lineBuf();
        while (regexMatch(q, &l->lineBuf()[l->len()] - q, q == l->lineBuf()) == 1)
        {
            matchedPosition(&first, &last);
            if (first <= p)
            {
                found = first;
                found_last = last;
            }
            if (q - l->lineBuf() >= l->len())
                break;
            q++;
#ifdef USE_M17N
            while (q - l->lineBuf() < l->len() && l->propBuf()[q - l->lineBuf()] & PC_WCHAR2)
                q++;
#endif
            if (q > p)
                break;
        }
        if (found)
        {
            pos = found - l->lineBuf();
            while (pos >= l->len() && buf->NextLine(l) && buf->NextLine(l)->bpos)
            {
                pos -= l->len();
                l = buf->NextLine(l);
            }
            buf->pos = pos;
            if (l != buf->CurrentLine())
                buf->GotoLine(l->linenumber);
            buf->ArrangeCursor();
            set_mark(l, pos, pos + found_last - found);
            return SR_FOUND;
        }
    }
    for (l = buf->PrevLine(l);; l = buf->PrevLine(l))
    {
        if (l == NULL)
        {
            if (w3mApp::Instance().WrapSearch)
            {
                l = buf->LastLine();
                wrapped = true;
            }
            else
            {
                break;
            }
        }
        found = NULL;
        found_last = NULL;
        q = l->lineBuf();
        while (regexMatch(q, &l->lineBuf()[l->len()] - q, q == l->lineBuf()) == 1)
        {
            matchedPosition(&first, &last);
            found = first;
            found_last = last;
            if (q - l->lineBuf() >= l->len())
                break;
            q++;
#ifdef USE_M17N
            while (q - l->lineBuf() < l->len() && l->propBuf()[q - l->lineBuf()] & PC_WCHAR2)
                q++;
#endif
        }
        if (found)
        {
            pos = found - l->lineBuf();
            while (pos >= l->len() && buf->NextLine(l) && buf->NextLine(l)->bpos)
            {
                pos -= l->len();
                l = buf->NextLine(l);
            }
            buf->pos = pos;
            buf->GotoLine(l->linenumber);
            buf->ArrangeCursor();
            set_mark(l, pos, pos + found_last - found);
            return SR_FOUND | (wrapped ? SR_WRAPPED : SR_NONE);
        }
        if (wrapped && l == begin) /* no match */
            break;
    }
    return SR_NOTFOUND;
}
