/* $Id: buffer.c,v 1.30 2010/07/18 14:10:09 htrb Exp $ */

#include "fm.h"
#include "frontend/buffer.h"
#include "public.h"
#include "indep.h"
#include "file.h"
#include "html/image.h"
#include "frontend/display.h"
#include "html/html.h"
#include "ctrlcode.h"
#include "transport/local.h"
#include "html/anchor.h"
#include "transport/url.h"
#include "transport/loader.h"
#include "mime/mimetypes.h"
#include <assert.h>

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
    buffername = "";
    bufferprop = BP_NORMAL;
    clone = New(int);
    *clone = 1;
    trbyte = 0;
    ssl_certificate = NULL;
    auto_detect = WcOption.auto_detect;
}

Buffer::~Buffer()
{
    deleteImage(this);
    clearBuffer(this);
    for (int i = 0; i < MAX_LB; i++)
    {
        auto b = linkBuffer[i];
        if (b == NULL)
            continue;
        b->linkBuffer[REV_LB[i]] = NULL;
    }
    if (savecache)
        unlink(savecache);
    if (--(*clone))
        return;
    if (pagerSource)
        ISclose(pagerSource);
    if (sourcefile &&
        (real_type.empty() || real_type.starts_with("image/")))
    {
        if (real_scheme != SCM_LOCAL || bufferprop & BP_FRAME)
            unlink(sourcefile);
    }
    if (header_source)
        unlink(header_source);
    if (mailcap_source)
        unlink(mailcap_source);
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

    Str tmp;
    FILE *cache = NULL;
    Line *l;
    int colorflag;

    if (this->savecache)
        return -1;

    if (this->firstLine == NULL)
        goto _error1;

    tmp = tmpfname(TMPF_CACHE, NULL);
    this->savecache = tmp->ptr;
    cache = fopen(this->savecache, "w");
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
    unlink(this->savecache);
_error1:
    this->savecache = NULL;
    return -1;
}

int Buffer::ReadBufferCache()
{
    // TODO:
    return -1;

    Line *basel = NULL;
    long clnum, tlnum;
    int colorflag;

    if (this->savecache == NULL)
        return -1;

    auto cache = fopen(this->savecache, "r");
    if (cache == NULL || fread1(clnum, cache) || fread1(tlnum, cache))
    {
        this->savecache = NULL;
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
    unlink(this->savecache);
    this->savecache = NULL;
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
    nextBuffer = NULL;
    for (int i = 0; i < MAX_LB; i++)
    {
        linkBuffer[i] = NULL;
    }
}

ParsedURL *Buffer::BaseURL()
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
    if (seq + 1 > hmarklist.size())
        hmarklist.resize(seq + 1);
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

void cmd_loadBuffer(BufferPtr buf, BufferProps prop, LinkBufferTypes linkid)
{
    if (buf == NULL)
    {
        disp_err_message("Can't load string", FALSE);
    }
    else
    {
        buf->bufferprop |= (BP_INTERNAL | prop);
        if (!(buf->bufferprop & BP_NO_URL))
            buf->currentURL = GetCurrentTab()->GetCurrentBuffer()->currentURL;
        if (linkid != LB_NOLINK)
        {
            buf->linkBuffer[linkid] = GetCurrentTab()->GetCurrentBuffer();
            GetCurrentTab()->GetCurrentBuffer()->linkBuffer[linkid] = buf;
        }
        GetCurrentTab()->PushBufferCurrentPrev(buf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

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

/* 
 * clearBuffer: clear buffer content
 */
void clearBuffer(BufferPtr buf)
{
    buf->firstLine = buf->topLine = buf->currentLine = buf->lastLine = NULL;
    buf->allLine = 0;
}

/* 
 * gotoLine: go to line number
 */
void gotoLine(BufferPtr buf, int n)
{
    char msg[32];
    Line *l = buf->firstLine;

    if (l == NULL)
        return;
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE))
    {
        if (buf->lastLine->linenumber < n)
            getNextPage(buf, n - buf->lastLine->linenumber);
        while ((buf->lastLine->linenumber < n) &&
               (getNextPage(buf, 1) != NULL))
            ;
    }
    if (l->linenumber > n)
    {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%d", l->linenumber);
        set_delayed_message(msg);
        buf->topLine = buf->currentLine = l;
        return;
    }
    if (buf->lastLine->linenumber < n)
    {
        l = buf->lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%d", buf->lastLine->linenumber);
        set_delayed_message(msg);
        buf->currentLine = l;
        buf->topLine = lineSkip(buf, buf->currentLine, -(buf->LINES - 1),
                                FALSE);
        return;
    }
    for (; l != NULL; l = l->next)
    {
        if (l->linenumber >= n)
        {
            buf->currentLine = l;
            if (n < buf->topLine->linenumber ||
                buf->topLine->linenumber + buf->LINES <= n)
                buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, FALSE);
            break;
        }
    }
}

/* 
 * gotoRealLine: go to real line number
 */
void gotoRealLine(BufferPtr buf, int n)
{
    char msg[32];
    Line *l = buf->firstLine;

    if (l == NULL)
        return;
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE))
    {
        if (buf->lastLine->real_linenumber < n)
            getNextPage(buf, n - buf->lastLine->real_linenumber);
        while ((buf->lastLine->real_linenumber < n) &&
               (getNextPage(buf, 1) != NULL))
            ;
    }
    if (l->real_linenumber > n)
    {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%d", l->real_linenumber);
        set_delayed_message(msg);
        buf->topLine = buf->currentLine = l;
        return;
    }
    if (buf->lastLine->real_linenumber < n)
    {
        l = buf->lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%d", buf->lastLine->real_linenumber);
        set_delayed_message(msg);
        buf->currentLine = l;
        buf->topLine = lineSkip(buf, buf->currentLine, -(buf->LINES - 1),
                                FALSE);
        return;
    }
    for (; l != NULL; l = l->next)
    {
        if (l->real_linenumber >= n)
        {
            buf->currentLine = l;
            if (n < buf->topLine->real_linenumber ||
                buf->topLine->real_linenumber + buf->LINES <= n)
                buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, FALSE);
            break;
        }
    }
}

/* 
 * Reshape HTML buffer
 */
void reshapeBuffer(BufferPtr buf)
{
    uint8_t old_auto_detect = WcOption.auto_detect;

    if (!buf->need_reshape)
        return;
    buf->need_reshape = FALSE;
    buf->width = INIT_BUFFER_WIDTH;
    if (buf->sourcefile == NULL)
        return;
    URLFile f(SCM_LOCAL, NULL);
    examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile,
                &f);
    if (f.stream == NULL)
        return;

    auto sbuf = buf->Copy();
    clearBuffer(buf);
    while (buf->frameset)
    {
        deleteFrameSet(buf->frameset);
        buf->frameset = popFrameTree(&(buf->frameQ));
    }

    buf->href.clear();
    buf->name.clear();
    buf->img.clear();
    buf->formitem.clear();
    buf->formlist = NULL;
    buf->linklist = NULL;
    buf->maplist = NULL;
    buf->hmarklist.clear();
    buf->imarklist.clear();

    if (buf->header_source)
    {
        if (buf->currentURL.scheme != SCM_LOCAL ||
            buf->mailcap_source || buf->currentURL.file == "-")
        {
            URLFile h(SCM_LOCAL, NULL);
            examineFile(buf->header_source, &h);
            if (h.stream)
            {
                readHeader(&h, buf, TRUE, NULL);
                h.Close();
            }
        }
        else if (buf->search_header) /* -m option */
            readHeader(&f, buf, TRUE, NULL);
    }

    WcOption.auto_detect = WC_OPT_DETECT_OFF;
    UseContentCharset = FALSE;

    if (is_html_type(buf->type))
        loadHTMLBuffer(&f, buf);
    else
        loadBuffer(&f, buf);
    f.Close();

    WcOption.auto_detect = old_auto_detect;
    UseContentCharset = TRUE;

    buf->height = (LINES - 1) + 1;
    if (buf->firstLine && sbuf->firstLine)
    {
        Line *cur = sbuf->currentLine;
        int n;

        buf->pos = sbuf->pos + cur->bpos;
        while (cur->bpos && cur->prev)
            cur = cur->prev;
        if (cur->real_linenumber > 0)
            gotoRealLine(buf, cur->real_linenumber);
        else
            gotoLine(buf, cur->linenumber);
        n = (buf->currentLine->linenumber - buf->topLine->linenumber) - (cur->linenumber - sbuf->topLine->linenumber);
        if (n)
        {
            buf->topLine = lineSkip(buf, buf->topLine, n, FALSE);
            if (cur->real_linenumber > 0)
                gotoRealLine(buf, cur->real_linenumber);
            else
                gotoLine(buf, cur->linenumber);
        }
        buf->pos -= buf->currentLine->bpos;
        if (FoldLine && !is_html_type(buf->type))
            buf->currentColumn = 0;
        else
            buf->currentColumn = sbuf->currentColumn;
        arrangeCursor(buf);
    }
    if (buf->check_url & CHK_URL)
        chkURLBuffer(buf);

    if (buf->check_url & CHK_NMID)
        chkNMIDBuffer(buf);
    if (buf->real_scheme == SCM_NNTP || buf->real_scheme == SCM_NEWS)
        reAnchorNewsheader(buf);

    formResetBuffer(buf, sbuf->formitem);
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
        ParsedURL pu;
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
