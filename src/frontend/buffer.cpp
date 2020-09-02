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
#include "frontend/terminal.h"
#include "urimethod.h"
#include "public.h"
#include "indep.h"
#include "file.h"
#include "html/image.h"
#include "html/html.h"
#include "ctrlcode.h"
#include "stream/local_cgi.h"
#include "frontend/anchor.h"
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

int prev_nonnull_line(BufferPtr buf, LinePtr line)
{
    LinePtr l;

    for (l = line; l != NULL && l->buffer.len() == 0; l = buf->m_document->PrevLine(l))
        ;
    if (l == NULL || l->buffer.len() == 0)
        return -1;

    // GetCurrentBuffer()->SetCurrentLine(l);
    if (l != line)
        GetCurrentBuffer()->bytePosition = GetCurrentBuffer()->CurrentLine()->buffer.len();
    return 0;
}

int next_nonnull_line(BufferPtr buf, LinePtr line)
{
    LinePtr l;

    for (l = line; l != NULL && l->buffer.len() == 0; l = buf->m_document->NextLine(l))
        ;

    if (l == NULL || l->buffer.len() == 0)
        return -1;

    // GetCurrentBuffer()->SetCurrentLine(l);
    if (l != line)
        GetCurrentBuffer()->bytePosition = 0;
    return 0;
}

Buffer::Buffer()
    : m_document(new Document)
{
    url.scheme = SCM_UNKNOWN;
    baseTarget = {};
    bufferprop = BP_NORMAL;
    auto_detect = WcOption.auto_detect;
}

Buffer::~Buffer()
{
    ImageManager::Instance().deleteImage(this);
    ClearLines();
    if (savecache.size())
        unlink(savecache.c_str());
    // if (pagerSource)
    //     pagerSource = nullptr;
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
    if (/*this->pagerSource == NULL &&*/ this->WriteBufferCache() == 0)
    {
        m_topLine = 0;
        m_currentLine = 0;
        m_document->Clear();
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

    if (m_document->LineCount() == 0)
        goto _error1;

    tmp = tmpfname(TMPF_CACHE, NULL);
    this->savecache = tmp->ptr;
    cache = fopen(this->savecache.c_str(), "w");
    if (!cache)
        goto _error1;

    if (fwrite1(this->m_currentLine, cache) ||
        fwrite1(this->m_topLine, cache))
        goto _error;

    for (auto i = 0; i < m_document->LineCount(); ++i)
    {
        auto l = m_document->GetLine(i);
        if (fwrite1(l->real_linenumber, cache) ||
            // fwrite1(l->usrflags, cache) ||
            fwrite1(l->buffer.Columns(), cache) ||
            fwrite1(l->buffer.len(), cache)
            // || fwrite1(l->bpos, cache) || fwrite1(l->bwidth, cache)
        )
            goto _error;
        // if (l->bpos == 0)
        {
            if (fwrite(l->buffer.lineBuf(), 1, l->buffer.len(), cache) < l->buffer.len() ||
                fwrite(l->buffer.propBuf(), sizeof(Lineprop), l->buffer.len(), cache) < l->buffer.len())
                goto _error;
        }

        colorflag = l->buffer.colorBuf() ? 1 : 0;
        if (fwrite1(colorflag, cache))
            goto _error;
        if (colorflag)
        {
            // if (l->bpos == 0)
            {
                if (fwrite(l->buffer.colorBuf(), sizeof(Linecolor), l->buffer.len(), cache) <
                    l->buffer.len())
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
    //         fread1(l->buffer.len(), cache) ||
    //         fread1(l->buffer.len(), cache) ||
    //         fread1(l->bpos, cache) || fread1(l->bwidth, cache))
    //         break;
    //     if (l->bpos == 0)
    //     {
    //         basel = l;
    //         l->buffer.lineBuf() = NewAtom_N(char, l->buffer.len() + 1);
    //         fread(l->buffer.lineBuf(), 1, l->buffer.len(), cache);
    //         l->buffer.lineBuf()[l->buffer.len()] = '\0';
    //         l->buffer.propBuf() = NewAtom_N(Lineprop, l->buffer.len());
    //         fread(l->buffer.propBuf(), sizeof(Lineprop), l->buffer.len(), cache);
    //     }
    //     else if (basel)
    //     {
    //         l->buffer.lineBuf() = basel->lineBuf() + l->bpos;
    //         l->buffer.propBuf() = basel->propBuf() + l->bpos;
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
    //             l->buffer.colorBuf() = NewAtom_N(Linecolor, l->buffer.len());
    //             fread(l->buffer.colorBuf(), sizeof(Linecolor), l->buffer.len(), cache);
    //         }
    //         else
    //             l->buffer.colorBuf() = basel->colorBuf() + l->bpos;
    //     }
    //     else
    //     {
    //         l->buffer.colorBuf() = NULL;
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
    auto copy = Buffer::Create(url);
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

    // document
    m_document->document_charset = src->m_document->document_charset;
    m_document->m_lines = src->m_document->m_lines;
    m_document->href = src->m_document->href;
    m_document->name = src->m_document->name;
    m_document->img = src->m_document->img;
    m_document->formitem = src->m_document->formitem;
    m_document->linklist = src->m_document->linklist;
    m_document->hmarklist = src->m_document->hmarklist;
    m_document->imarklist = src->m_document->imarklist;
    m_document->formlist = src->m_document->formlist;
    m_document->maplist = src->m_document->maplist;
    m_document->event = src->m_document->event;

    // scroll
    m_topLine = src->m_topLine;
    // cursor ?
    m_currentLine = src->m_currentLine;

    // mimetype: text/plain
    type = src->type;
    real_type = src->real_type;

    bufferprop = src->bufferprop;
    leftCol = src->leftCol;
    bytePosition = src->bytePosition;
    rect = src->rect;

    prevhseq = src->prevhseq;

    url = src->url;
    baseTarget = src->baseTarget;
    real_scheme = src->real_scheme;
    sourcefile = src->sourcefile;
    check_url = src->check_url;
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
            if (m_document->hmarklist[a->hseq].line == bp.line)
                m_document->hmarklist[a->hseq].pos = a->start.pos;
        }
        if (a->end.pos >= bp.pos)
            a->end.pos += shift;
    }
}

std::shared_ptr<Buffer> Buffer::Create(const URL &url)
{
    auto n = std::make_shared<Buffer>();
    // n->width = Terminal::columns();
    n->url = url;
    return n;
}

LinePtr Buffer::CurrentLineSkip(LinePtr line, int offset, int last)
{
    int i, n;
    auto l = m_document->_find(line);

    // if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    // {
    //     n = line->linenumber + offset + this->rect.lines;
    //     if (m_document->LastLine()->linenumber < n)
    //         getNextPage(shared_from_this(), n - m_document->LastLine()->linenumber);
    //     while ((last || (m_document->LastLine()->linenumber < n)) &&
    //            (getNextPage(shared_from_this(), 1) != NULL))
    //         ;
    //     if (last)
    //         l = m_document->_find(m_document->LastLine());
    // }

    if (offset == 0)
        return *l;
    if (offset > 0)
        for (i = 0; i < offset && (*l) != m_document->m_lines.back(); i++, ++l)
        {
        }
    else
        for (i = 0; i < -offset && l != m_document->m_lines.begin(); i++, --l)
        {
        }
    return *l;
}

void Buffer::LineSkip(LinePtr line, int offset, int last)
{
    auto l = m_document->_find(CurrentLineSkip(line, offset, last));
    int i;
    if (!w3mApp::Instance().nextpage_topline)
        for (i = this->rect.lines - 1 - (m_document->LastLine()->linenumber - (*l)->linenumber);
             i > 0 && l != m_document->m_lines.begin();
             i--, --l)
        {
        };
    // topLine = *l;
}

/*
 * gotoLine: go to line number
 */
void Buffer::GotoLine(int n, bool topline)
{
    char msg[32];
    LinePtr l = m_document->FirstLine();
    if (l == NULL)
        return;
    // if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    // {
    //     if (m_document->LastLine()->linenumber < n)
    //         getNextPage(shared_from_this(), n - m_document->LastLine()->linenumber);
    //     while ((m_document->LastLine()->linenumber < n) &&
    //            (getNextPage(shared_from_this(), 1) != NULL))
    //         ;
    // }
    if (l->linenumber > n)
    {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->linenumber);
        set_delayed_message(msg);
        // this->topLine = this->currentLine = l;
        return;
    }
    if (m_document->LastLine()->linenumber < n)
    {
        l = m_document->LastLine();
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", m_document->LastLine()->linenumber);
        set_delayed_message(msg);
        // this->currentLine = l;
        // LineSkip(this->currentLine, -(this->rect.lines - 1), false);
        return;
    }
    auto it = m_document->_find(l);
    for (; it != m_document->m_lines.end(); ++it)
    {
        if ((*it)->linenumber >= n)
        {
            // this->currentLine = *it;
            // if (n < this->topLine->linenumber ||
            //     this->topLine->linenumber + this->rect.lines <= n)
            //     LineSkip(*it, -(this->rect.lines + 1) / 2, false);
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
    // auto lnum = currentLine->linenumber;
    // auto top = topLine;
    // this->LineSkip(top, n, false);
    // if (this->topLine == top)
    // {
    //     lnum += n;
    //     if (lnum < this->topLine->linenumber)
    //         lnum = this->topLine->linenumber;
    //     else if (lnum > m_document->LastLine()->linenumber)
    //         lnum = m_document->LastLine()->linenumber;
    // }
    // else
    // {
    //     auto tlnum = this->topLine->linenumber;
    //     auto llnum = this->topLine->linenumber + this->rect.lines - 1;
    //     int diff_n;
    //     if (w3mApp::Instance().nextpage_topline)
    //         diff_n = 0;
    //     else
    //         diff_n = n - (tlnum - top->linenumber);
    //     if (lnum < tlnum)
    //         lnum = tlnum + diff_n;
    //     if (lnum > llnum)
    //         lnum = llnum + diff_n;
    // }
    // this->GotoLine(lnum);
}

void Buffer::NScroll(int n)
{
    // if (m_document->LineCount() == 0)
    //     return;

    // this->Scroll(n);

    // this->ArrangeLine();
    // if (n > 0)
    // {
    //     // if (this->currentLine->bpos &&
    //     //     this->currentLine->bwidth >= this->currentColumn + this->visualpos)
    //     //     this->CursorDown(1);
    //     // else
    //     {
    //         while (m_document->NextLine(this->currentLine) && this->currentLine->buffer.Columns() < this->CurrentCol())
    //             this->CursorDown0(1);
    //     }
    // }
    // else
    // {
    //     if (this->currentLine->buffer.Columns() < this->CurrentCol())
    //         this->CursorUp(1);
    //     else
    //     {
    //         // while (m_document->PrevLine(this->currentLine) && this->currentLine->bwidth >= this->currentColumn + this->visualpos)
    //         //     this->CursorUp0(1);
    //     }
    // }
}

/*
 * gotoRealLine: go to real line number
 */
void Buffer::GotoRealLine(int n)
{
    char msg[32];
    LinePtr l = m_document->FirstLine();

    // if (l == NULL)
    //     return;
    // // if (this->pagerSource && !(this->bufferprop & BP_CLOSE))
    // // {
    // //     if (m_document->LastLine()->real_linenumber < n)
    // //         getNextPage(shared_from_this(), n - m_document->LastLine()->real_linenumber);
    // //     while ((m_document->LastLine()->real_linenumber < n) &&
    // //            (getNextPage(shared_from_this(), 1) != NULL))
    // //         ;
    // // }
    // if (l->real_linenumber > n)
    // {
    //     /* FIXME: gettextize? */
    //     sprintf(msg, "First line is #%ld", l->real_linenumber);
    //     set_delayed_message(msg);
    //     this->topLine = this->currentLine = l;
    //     return;
    // }
    // if (m_document->LastLine()->real_linenumber < n)
    // {
    //     l = m_document->LastLine();
    //     /* FIXME: gettextize? */
    //     sprintf(msg, "Last line is #%ld", m_document->LastLine()->real_linenumber);
    //     set_delayed_message(msg);
    //     this->currentLine = l;
    //     LineSkip(this->currentLine, -(this->rect.lines - 1),
    //              false);
    //     return;
    // }

    // auto it = m_document->_find(l);
    // for (; it != m_document->m_lines.end(); ++it)
    // {
    //     if ((*it)->real_linenumber >= n)
    //     {
    //         this->currentLine = *it;
    //         if (n < this->topLine->real_linenumber ||
    //             this->topLine->real_linenumber + this->rect.lines <= n)
    //             LineSkip(*it, -(this->rect.lines + 1) / 2, false);
    //         break;
    //     }
    // }
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
//     if (m_document->FirstLine() && sbuf->FirstLine())
//     {
//         LinePtr cur = sbuf->currentLine;
//         int n;

//         this->pos = sbuf->pos + cur->bpos;
//         while (cur->bpos && m_document->PrevLine(cur))
//             cur = m_document->PrevLine(cur);
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
        set_environ("W3M_URL", buf->url.ToStr()->ptr);
        set_environ("W3M_TYPE", buf->real_type.size() ? buf->real_type : "unknown");
        set_environ("W3M_CHARSET", wc_ces_to_charset(buf->m_document->document_charset));
    }
    l = buf->CurrentLine();
    if (l && (buf != prev_buf || l != prev_line || buf->bytePosition != prev_pos))
    {
        char *s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", (char *)(s ? s : ""));
        auto a = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
        if (a)
        {
            auto pu = URL::Parse(a->url, &buf->url);
            set_environ("W3M_CURRENT_LINK", pu.ToStr()->ptr);
        }
        else
            set_environ("W3M_CURRENT_LINK", "");
        a = buf->m_document->img.RetrieveAnchor(buf->CurrentPoint());
        if (a)
        {
            auto pu = URL::Parse(a->url, &buf->url);
            set_environ("W3M_CURRENT_IMG", pu.ToStr()->ptr);
        }
        else
            set_environ("W3M_CURRENT_IMG", "");
        a = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());
        if (a)
            set_environ("W3M_CURRENT_FORM", a->item->ToStr()->ptr);
        else
            set_environ("W3M_CURRENT_FORM", "");
        set_environ("W3M_CURRENT_LINE", Sprintf("%d",
                                                l->real_linenumber)
                                            ->ptr);
        set_environ("W3M_CURRENT_COLUMN", Sprintf("%d",
                                                  buf->leftCol + buf->CursorX() + 1)
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
    prev_pos = buf->bytePosition;
}

const char *NullLine = "";
Lineprop NullProp[] = {P_UNKNOWN};

void Buffer::SavePosition()
{
    if (m_document->LineCount() == 0)
        return;

    // auto b = undo.size() ? &undo.back() : nullptr;
    // if (b && b->top_linenumber == TOP_LINENUMBER() &&
    //     b->cur_linenumber == CUR_LINENUMBER() &&
    //     b->currentColumn == leftCol &&
    //     b->pos == bytePosition)
    // {
    //     return;
    // }

    // undo.push_back({});

    // b = &undo.back();
    // b->top_linenumber = TOP_LINENUMBER();
    // b->cur_linenumber = CUR_LINENUMBER();
    // b->currentColumn = this->leftCol;
    // b->pos = this->bytePosition;
}

void Buffer::CursorRight()
{
    if (m_document->LineCount() == 0)
        return;

    auto l = CurrentLine();
    auto p = l->buffer.propBuf();
    auto delta = 1;
    while (p[this->bytePosition + delta] & PC_WCHAR2)
        delta++;

    if (this->bytePosition + delta < l->buffer.len())
    {
        this->bytePosition += delta;
    }
}

void Buffer::CursorLeft()
{
    if (m_document->LineCount() == 0)
        return;

    auto l = CurrentLine();
    auto p = l->buffer.propBuf();
    auto delta = 1;
    while (this->bytePosition - delta > 0 && p[this->bytePosition - delta] & PC_WCHAR2)
        delta++;

    if (this->bytePosition >= delta){
        this->bytePosition -= delta;
    }
}

void Buffer::CursorXY(int x, int y)
{
    // to buffer local
    x -= rect.rootX;
    y -= rect.rootY;

    m_currentLine += y;

    if (CursorX() > x)
    {
        while (CursorX() > x)
            CursorLeft();
    }
    else if (CursorX() < x)
    {
        while (CursorX() < x)
        {
            int oldX = CursorX();

            CursorRight();

            if (oldX == CursorX())
                break;
        }
        if (CursorX() > x)
            CursorLeft();
    }
}

/*
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void Buffer::ArrangeCursor()
{
    // if (this->currentLine == NULL)
    //     return;

    // int col, col2, pos;
    // int delta = 1;
    // /* Arrange line */
    // if (this->currentLine->linenumber - this->topLine->linenumber >= this->rect.lines || this->currentLine->linenumber < this->topLine->linenumber)
    // {
    //     this->LineSkip(this->currentLine, 0, false);
    // }
    // /* Arrange column */
    // // while (this->pos < 0 && m_document->PrevLine(this->currentLine) && this->currentLine->bpos)
    // // {
    // //     pos = this->pos + m_document->PrevLine(this->currentLine)->len();
    // //     CursorUp0(1);
    // //     this->pos = pos;
    // // }
    // // while (this->pos >= this->currentLine->buffer.len() && m_document->NextLine(this->currentLine) &&
    // //        m_document->NextLine(this->currentLine)->bpos)
    // // {
    // //     pos = this->pos - this->currentLine->buffer.len();
    // //     CursorDown0(1);
    // //     this->pos = pos;
    // // }
    // if (this->currentLine->buffer.len() == 0 || this->bytePosition < 0)
    //     this->bytePosition = 0;
    // else if (this->bytePosition >= this->currentLine->buffer.len())
    //     this->bytePosition = this->currentLine->buffer.len() - 1;

    // while (this->bytePosition > 0 && this->currentLine->buffer.propBuf()[this->bytePosition] & PC_WCHAR2)
    //     this->bytePosition--;

    // col = this->currentLine->buffer.BytePositionToColumn(this->bytePosition);

    // while (this->bytePosition + delta < this->currentLine->buffer.len() &&
    //        this->currentLine->buffer.propBuf()[this->bytePosition + delta] & PC_WCHAR2)
    //     delta++;

    // col2 = this->currentLine->buffer.BytePositionToColumn(this->bytePosition + delta);
    // if (col < this->leftCol || col2 > this->rect.cols + this->leftCol)
    // {
    //     this->leftCol = 0;
    //     if (col2 > this->rect.cols)
    //         ColumnSkip(col);
    // }
}

void Buffer::ArrangeLine()
{
    if (m_document->LineCount() == 0)
    {
        return;
    }
    if (this->m_currentLine < 0)
    {
        this->m_currentLine = 0;
    }
    else if (this->m_currentLine >= m_document->LineCount())
    {
        m_currentLine = m_document->LineCount() - 1;
    }

    if (m_currentLine < m_topLine)
    {
        // scroll up
        m_topLine = m_currentLine;
    }
    else
    {
        auto delta = this->m_currentLine - this->m_topLine;
        if (delta >= rect.lines)
        {
            // scroll down
            this->m_topLine = m_currentLine - (rect.lines - 1);
        }
    }
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

bool Buffer::MoveLeftWord(int n)
{
    if (m_document->LineCount() == 0)
        return false;

    for (int i = 0; i < n; i++)
    {
        auto pline = this->CurrentLine();
        auto ppos = this->bytePosition;
        if (prev_nonnull_line(shared_from_this(), this->CurrentLine()) < 0)
            goto end;

        while (1)
        {
            auto l = this->CurrentLine();
            auto lb = l->buffer.lineBuf();
            while (this->bytePosition > 0)
            {
                int tmp = this->bytePosition;
                prevChar(&tmp, l);
                if (is_wordchar(getChar(&lb[tmp])))
                    break;
                this->bytePosition = tmp;
            }
            if (this->bytePosition > 0)
                break;
            if (prev_nonnull_line(shared_from_this(), this->m_document->PrevLine(this->CurrentLine())) < 0)
            {
                // this->SetCurrentLine(pline);
                this->bytePosition = ppos;
                goto end;
            }
            this->bytePosition = this->CurrentLine()->buffer.len();
        }
        {
            auto l = this->CurrentLine();
            auto lb = l->buffer.lineBuf();
            while (this->bytePosition > 0)
            {
                int tmp = this->bytePosition;
                prevChar(&tmp, l);
                if (!is_wordchar(getChar(&lb[tmp])))
                    break;
                this->bytePosition = tmp;
            }
        }
    }
end:
    this->ArrangeCursor();
    return true;
}

bool Buffer::MoveRightWord(int n)
{
    if (m_document->LineCount() == 0)
        return false;

    for (int i = 0; i < n; i++)
    {
        auto pline = this->CurrentLine();
        auto ppos = this->bytePosition;
        if (next_nonnull_line(shared_from_this(), this->CurrentLine()) < 0)
            goto end;

        auto l = this->CurrentLine();
        auto lb = l->buffer.lineBuf();
        while (this->bytePosition < l->buffer.len() &&
               is_wordchar(getChar(&lb[this->bytePosition])))
            nextChar(&this->bytePosition, l);

        while (1)
        {
            while (this->bytePosition < l->buffer.len() &&
                   !is_wordchar(getChar(&lb[this->bytePosition])))
                nextChar(&this->bytePosition, l);
            if (this->bytePosition < l->buffer.len())
                break;
            if (next_nonnull_line(shared_from_this(), this->m_document->NextLine(this->CurrentLine())) < 0)
            {
                // this->SetCurrentLine(pline);
                this->bytePosition = ppos;
                goto end;
            }
            this->bytePosition = 0;
            l = this->CurrentLine();
            lb = l->buffer.lineBuf();
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

    BufferPtr newBuf = std::make_shared<Buffer>();
    // newBuf->SetTopLine(top);
    // newBuf->SetCurrentLine(cur);
    newBuf->bytePosition = b.pos;
    newBuf->leftCol = b.currentColumn;
    restorePosition(newBuf);
}

void Buffer::undoPos(int prec_num)
{
    if (m_document->LineCount() == 0)
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
    if (m_document->LineCount() == 0)
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
        bytePosition += 1;
    auto result = srchcore(SearchString, routine[reverse], prec_num);
    if (result & SR_FOUND)
        CurrentLine()->buffer.clear_mark();

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
            CurrentLine()->buffer.clear_mark();
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
        s_pos = sbuf->bytePosition;
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
                this->bytePosition += 1;
            sbuf->COPY_BUFPOSITION_FROM(shared_from_this());
            if (srchcore(str, searchRoutine, prec_num) == SR_NOTFOUND && searchRoutine == forwardSearch)
            {
                this->bytePosition -= 1;
                sbuf->COPY_BUFPOSITION_FROM(shared_from_this());
            }
            this->ArrangeCursor();
            this->CurrentLine()->buffer.clear_mark();
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
        s_pos = this->bytePosition;
    }
    this->CurrentLine()->buffer.clear_mark();
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
            return;
        }
        disp = true;
    }

    auto p = this->bytePosition;
    if (func == forwardSearch)
        this->bytePosition += 1;
    auto result = srchcore(str, func, prec_num);
    if (result & SR_FOUND)
        this->CurrentLine()->buffer.clear_mark();
    else
        this->bytePosition = p;
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

    this->bytePosition = 0;
    if (((l[0] == '^') || (l[0] == '$')) && prec_num)
    {
        this->GotoRealLine(prec_num);
    }
    else if (l[0] == '^')
    {
        // this->SetCurrentLine(m_document->FirstLine());
        // this->SetTopLine(m_document->FirstLine());
    }
    else if (l[0] == '$')
    {
        this->LineSkip(m_document->LastLine(),
                       -(this->rect.lines + 1) / 2, true);
        // this->SetCurrentLine(m_document->LastLine());
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
    for (; pos < epos && pos < l->buffer.len(); pos++)
        l->buffer.propBuf()[pos] |= PE_MARK;
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
        this->m_document->document_charset != f_ces)
        str = wtf_conv_fit(str.data(), this->m_document->document_charset);
    return std::string(str);
}

SearchResultTypes forwardSearch(const BufferPtr &buf, std::string_view str)
{
    char *first, *last;
    int wrapped = false;

    const char *p;
    if ((p = regexCompile(str.data(), w3mApp::Instance().IgnoreCase)) != NULL)
    {
        message(p);
        return SR_NOTFOUND;
    }

    auto l = buf->CurrentLine();
    if (l == NULL)
    {
        return SR_NOTFOUND;
    }

    auto pos = buf->bytePosition;
    // if (l->bpos)
    // {
    //     pos += l->bpos;
    //     while (l->bpos && buf->m_document->PrevLine(l))
    //         l = buf->m_document->PrevLine(l);
    // }

    auto begin = l;
    while (pos < l->buffer.len() && l->buffer.propBuf()[pos] & PC_WCHAR2)
        pos++;

    if (pos < l->buffer.len() && regexMatch(&l->buffer.lineBuf()[pos], l->buffer.len() - pos, 0) == 1)
    {
        matchedPosition(&first, &last);
        pos = first - l->buffer.lineBuf();
        // while (pos >= l->buffer.len() && buf->m_document->NextLine(l) && buf->m_document->NextLine(l)->bpos)
        // {
        //     pos -= l->buffer.len();
        //     l = buf->m_document->NextLine(l);
        // }
        buf->bytePosition = pos;
        if (l != buf->CurrentLine())
            buf->GotoLine(l->linenumber);
        buf->ArrangeCursor();
        set_mark(l, pos, pos + last - first);
        return SR_FOUND;
    }
    for (l = buf->m_document->NextLine(l);; l = buf->m_document->NextLine(l))
    {
        if (l == NULL)
        {
            // if (buf->pagerSource)
            // {
            //     l = getNextPage(buf, 1);
            //     if (l == NULL)
            //     {
            //         if (w3mApp::Instance().WrapSearch && !wrapped)
            //         {
            //             l = buf->m_document->FirstLine();
            //             wrapped = true;
            //         }
            //         else
            //         {
            //             break;
            //         }
            //     }
            // }
            // else
            if (w3mApp::Instance().WrapSearch)
            {
                l = buf->m_document->FirstLine();
                wrapped = true;
            }
            else
            {
                break;
            }
        }
        // if (l->bpos)
        //     continue;
        if (regexMatch(l->buffer.lineBuf(), l->buffer.len(), 1) == 1)
        {
            matchedPosition(&first, &last);
            pos = first - l->buffer.lineBuf();
            // while (pos >= l->buffer.len() && buf->m_document->NextLine(l) && buf->m_document->NextLine(l)->bpos)
            // {
            //     pos -= l->buffer.len();
            //     l = buf->m_document->NextLine(l);
            // }
            buf->bytePosition = pos;
            // buf->SetCurrentLine(l);
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

    if ((p = regexCompile(str.data(), w3mApp::Instance().IgnoreCase)) != NULL)
    {
        message(p);
        return SR_NOTFOUND;
    }
    l = buf->CurrentLine();
    if (l == NULL)
    {
        return SR_NOTFOUND;
    }
    pos = buf->bytePosition;
    // if (l->bpos)
    // {
    //     pos += l->bpos;
    //     while (l->bpos && buf->m_document->PrevLine(l))
    //         l = buf->m_document->PrevLine(l);
    // }
    begin = l;
    if (pos > 0)
    {
        pos--;
#ifdef USE_M17N
        while (pos > 0 && l->buffer.propBuf()[pos] & PC_WCHAR2)
            pos--;
#endif
        p = &l->buffer.lineBuf()[pos];
        found = NULL;
        found_last = NULL;
        q = l->buffer.lineBuf();
        while (regexMatch(q, &l->buffer.lineBuf()[l->buffer.len()] - q, q == l->buffer.lineBuf()) == 1)
        {
            matchedPosition(&first, &last);
            if (first <= p)
            {
                found = first;
                found_last = last;
            }
            if (q - l->buffer.lineBuf() >= l->buffer.len())
                break;
            q++;
#ifdef USE_M17N
            while (q - l->buffer.lineBuf() < l->buffer.len() && l->buffer.propBuf()[q - l->buffer.lineBuf()] & PC_WCHAR2)
                q++;
#endif
            if (q > p)
                break;
        }
        if (found)
        {
            pos = found - l->buffer.lineBuf();
            // while (pos >= l->buffer.len() && buf->m_document->NextLine(l) && buf->m_document->NextLine(l)->bpos)
            // {
            //     pos -= l->buffer.len();
            //     l = buf->m_document->NextLine(l);
            // }
            buf->bytePosition = pos;
            if (l != buf->CurrentLine())
                buf->GotoLine(l->linenumber);
            buf->ArrangeCursor();
            set_mark(l, pos, pos + found_last - found);
            return SR_FOUND;
        }
    }
    for (l = buf->m_document->PrevLine(l);; l = buf->m_document->PrevLine(l))
    {
        if (l == NULL)
        {
            if (w3mApp::Instance().WrapSearch)
            {
                l = buf->m_document->LastLine();
                wrapped = true;
            }
            else
            {
                break;
            }
        }
        found = NULL;
        found_last = NULL;
        q = l->buffer.lineBuf();
        while (regexMatch(q, &l->buffer.lineBuf()[l->buffer.len()] - q, q == l->buffer.lineBuf()) == 1)
        {
            matchedPosition(&first, &last);
            found = first;
            found_last = last;
            if (q - l->buffer.lineBuf() >= l->buffer.len())
                break;
            q++;
#ifdef USE_M17N
            while (q - l->buffer.lineBuf() < l->buffer.len() && l->buffer.propBuf()[q - l->buffer.lineBuf()] & PC_WCHAR2)
                q++;
#endif
        }
        if (found)
        {
            pos = found - l->buffer.lineBuf();
            // while (pos >= l->buffer.len() && buf->m_document->NextLine(l) && buf->m_document->NextLine(l)->bpos)
            // {
            //     pos -= l->buffer.len();
            //     l = buf->m_document->NextLine(l);
            // }
            buf->bytePosition = pos;
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

int Buffer::ColumnSkip(int offset)
{
    int column = this->leftCol + offset;
    int nlines = this->rect.lines + 1;

    // int maxColumn = 0;
    // auto l = m_document->_find(topLine);
    // for (int i = 0; i < nlines && l != m_document->m_lines.end(); i++, ++l)
    // {
    //     if ((*l)->buffer.Columns() - 1 > maxColumn)
    //         maxColumn = (*l)->buffer.Columns() - 1;
    // }
    // maxColumn -= this->rect.cols - 1;
    // if (column < maxColumn)
    //     maxColumn = column;
    // if (maxColumn < 0)
    //     maxColumn = 0;

    // if (this->leftCol == maxColumn)
    //     return 0;
    // this->leftCol = maxColumn;
    return 1;
}
