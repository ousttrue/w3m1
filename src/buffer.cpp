/* $Id: buffer.c,v 1.30 2010/07/18 14:10:09 htrb Exp $ */

#include "fm.h"
#include "buffer.h"
#include "public.h"
#include "indep.h"
#include "file.h"
#include "image.h"
#include "display.h"
#include "terms.h"
#include "html.h"
#include "ctrlcode.h"
#include "local.h"

static int REV_LB[MAX_LB] = {
    LB_N_FRAME,
    LB_FRAME,
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};

Buffer::Buffer()
{
    COLS = COLS;
    LINES = (LINES - 1);
    currentURL.scheme = SCM_UNKNOWN;
    baseURL = NULL;
    baseTarget = NULL;
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
        (!real_type || strncasecmp(real_type, "image/", 6)))
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

BufferPtr Buffer::Copy()
{
    auto copy = newBuffer(width);
    copy->CopyFrom(this);
    return copy;
}

void Buffer::CopyFrom(BufferPtr src)
{
    readBufferCache(src);
    src->clone += 1;

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

char *NullLine = "";
Lineprop NullProp[] = {0};

void cmd_loadBuffer(BufferPtr buf, int prop, LinkBufferTypes linkid)
{
    if (buf == NULL)
    {
        disp_err_message("Can't load string", FALSE);
    }
    else
    {
        buf->bufferprop |= (BP_INTERNAL | prop);
        if (!(buf->bufferprop & BP_NO_URL))
            copyParsedURL(&buf->currentURL, &GetCurrentbuf()->currentURL);
        if (linkid != LB_NOLINK)
        {
            buf->linkBuffer[linkid] = GetCurrentbuf();
            GetCurrentbuf()->linkBuffer[linkid] = buf;
        }
        GetCurrentTab()->BufferPushBeforeCurrent(buf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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
 * namedBuffer: Select buffer which have specified name
 */
BufferPtr
namedBuffer(BufferPtr first, char *name)
{
    BufferPtr buf;

    if (first->buffername == name)
    {
        return first;
    }
    for (buf = first; buf->nextBuffer; buf = buf->nextBuffer)
    {
        if (buf->nextBuffer->buffername == name)
        {
            return buf->nextBuffer;
        }
    }
    return NULL;
}

/* 
 * deleteBuffer: delete buffer & return fistbuffer
 */
BufferPtr
deleteBuffer(BufferPtr first, BufferPtr delbuf)
{
    BufferPtr buf;
    BufferPtr b;

    if (first == delbuf && first->nextBuffer != NULL)
    {
        buf = first->nextBuffer;
        return buf;
    }
    if ((buf = prevBuffer(first, delbuf)) != NULL)
    {
        b = buf->nextBuffer;
        buf->nextBuffer = b->nextBuffer;
    }
    return first;
}

/* 
 * replaceBuffer: replace buffer
 */
BufferPtr
replaceBuffer(BufferPtr first, BufferPtr delbuf, BufferPtr newbuf)
{
    BufferPtr buf;

    if (delbuf == NULL)
    {
        newbuf->nextBuffer = first;
        return newbuf;
    }
    if (first == delbuf)
    {
        newbuf->nextBuffer = delbuf->nextBuffer;
        return newbuf;
    }
    if (delbuf && (buf = prevBuffer(first, delbuf)))
    {
        buf->nextBuffer = newbuf;
        newbuf->nextBuffer = delbuf->nextBuffer;
        return first;
    }
    newbuf->nextBuffer = first;
    return newbuf;
}

BufferPtr
nthBuffer(BufferPtr firstbuf, int n)
{
    int i;
    BufferPtr buf = firstbuf;

    if (n < 0)
        return firstbuf;
    for (i = 0; i < n; i++)
    {
        if (buf == NULL)
            return NULL;
        buf = buf->nextBuffer;
    }
    return buf;
}

static void
writeBufferName(BufferPtr buf, int n)
{
    Str msg;
    int all;

    all = buf->allLine;
    if (all == 0 && buf->lastLine != NULL)
        all = buf->lastLine->linenumber;
    move(n, 0);
    /* FIXME: gettextize? */
    msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
    if (buf->filename != NULL)
    {
        switch (buf->currentURL.scheme)
        {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            if (strcmp(buf->currentURL.file, "-"))
            {
                msg->Push(' ');
                msg->Push(conv_from_system(buf->currentURL.real_file));
            }
            break;
        case SCM_UNKNOWN:
        case SCM_MISSING:
            break;
        default:
            msg->Push(' ');
            msg->Push(parsedURL2Str(&buf->currentURL));
            break;
        }
    }
    addnstr_sup(msg->ptr, COLS - 1);
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
        sprintf(msg, "First line is #%ld", l->linenumber);
        set_delayed_message(msg);
        buf->topLine = buf->currentLine = l;
        return;
    }
    if (buf->lastLine->linenumber < n)
    {
        l = buf->lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", buf->lastLine->linenumber);
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
        sprintf(msg, "First line is #%ld", l->real_linenumber);
        set_delayed_message(msg);
        buf->topLine = buf->currentLine = l;
        return;
    }
    if (buf->lastLine->real_linenumber < n)
    {
        l = buf->lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", buf->lastLine->real_linenumber);
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

static BufferPtr
listBuffer(BufferPtr top, BufferPtr current)
{
    int i, c = 0;
    BufferPtr buf = top;

    move(0, 0);
#ifdef USE_COLOR
    if (useColor)
    {
        setfcolor(basic_color);
#ifdef USE_BG_COLOR
        setbcolor(bg_color);
#endif /* USE_BG_COLOR */
    }
#endif /* USE_COLOR */
    clrtobotx();
    for (i = 0; i < (LINES - 1); i++)
    {
        if (buf == current)
        {
            c = i;
            standout();
        }
        writeBufferName(buf, i);
        if (buf == current)
        {
            standend();
            clrtoeolx();
            move(i, 0);
            toggle_stand();
        }
        else
            clrtoeolx();
        if (buf->nextBuffer == NULL)
        {
            move(i + 1, 0);
            clrtobotx();
            break;
        }
        buf = buf->nextBuffer;
    }
    standout();
    /* FIXME: gettextize? */
    message("Buffer selection mode: SPC for select / D for delete buffer", 0,
            0);
    standend();
    /* 
     * move((LINES-1), COLS - 1); */
    move(c, 0);
    refresh();
    return buf->nextBuffer;
}

/* 
 * Select buffer visually
 */
BufferPtr
selectBuffer(BufferPtr firstbuf, BufferPtr currentbuf, char *selectchar)
{
    int i, cpoint,                     /* Current Buffer Number */
        spoint,                        /* Current Line on Screen */
        maxbuf, sclimit = (LINES - 1); /* Upper limit of line * number in 
					 * the * screen */
    BufferPtr buf;
    BufferPtr topbuf;
    char c;

    i = cpoint = 0;
    for (buf = firstbuf; buf != NULL; buf = buf->nextBuffer)
    {
        if (buf == currentbuf)
            cpoint = i;
        i++;
    }
    maxbuf = i;

    if (cpoint >= sclimit)
    {
        spoint = sclimit / 2;
        topbuf = nthBuffer(firstbuf, cpoint - spoint);
    }
    else
    {
        topbuf = firstbuf;
        spoint = cpoint;
    }
    listBuffer(topbuf, currentbuf);

    for (;;)
    {
        if ((c = getch()) == ESC_CODE)
        {
            if ((c = getch()) == '[' || c == 'O')
            {
                switch (c = getch())
                {
                case 'A':
                    c = 'k';
                    break;
                case 'B':
                    c = 'j';
                    break;
                case 'C':
                    c = ' ';
                    break;
                case 'D':
                    c = 'B';
                    break;
                }
            }
        }
#ifdef __EMX__
        else if (!c)
            switch (getch())
            {
            case K_UP:
                c = 'k';
                break;
            case K_DOWN:
                c = 'j';
                break;
            case K_RIGHT:
                c = ' ';
                break;
            case K_LEFT:
                c = 'B';
            }
#endif
        switch (c)
        {
        case CTRL_N:
        case 'j':
            if (spoint < sclimit - 1)
            {
                if (currentbuf->nextBuffer == NULL)
                    continue;
                writeBufferName(currentbuf, spoint);
                currentbuf = currentbuf->nextBuffer;
                cpoint++;
                spoint++;
                standout();
                writeBufferName(currentbuf, spoint);
                standend();
                move(spoint, 0);
                toggle_stand();
            }
            else if (cpoint < maxbuf - 1)
            {
                topbuf = currentbuf;
                currentbuf = currentbuf->nextBuffer;
                cpoint++;
                spoint = 1;
                listBuffer(topbuf, currentbuf);
            }
            break;
        case CTRL_P:
        case 'k':
            if (spoint > 0)
            {
                writeBufferName(currentbuf, spoint);
                currentbuf = nthBuffer(topbuf, --spoint);
                cpoint--;
                standout();
                writeBufferName(currentbuf, spoint);
                standend();
                move(spoint, 0);
                toggle_stand();
            }
            else if (cpoint > 0)
            {
                i = cpoint - sclimit;
                if (i < 0)
                    i = 0;
                cpoint--;
                spoint = cpoint - i;
                currentbuf = nthBuffer(firstbuf, cpoint);
                topbuf = nthBuffer(firstbuf, i);
                listBuffer(topbuf, currentbuf);
            }
            break;
        default:
            *selectchar = c;
            return currentbuf;
        }
        /* 
	 * move((LINES-1), COLS - 1);
	 */
        move(spoint, 0);
        refresh();
    }
}

/* 
 * Reshape HTML buffer
 */
void reshapeBuffer(BufferPtr buf)
{
    URLFile f;
    uint8_t old_auto_detect = WcOption.auto_detect;

    if (!buf->need_reshape)
        return;
    buf->need_reshape = FALSE;
    buf->width = INIT_BUFFER_WIDTH;
    if (buf->sourcefile == NULL)
        return;
    init_stream(&f, SCM_LOCAL, NULL);
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

    buf->href = NULL;
    buf->name = NULL;
    buf->img = NULL;
    buf->formitem = NULL;
    buf->formlist = NULL;
    buf->linklist = NULL;
    buf->maplist = NULL;
    if (buf->hmarklist)
        buf->hmarklist->nmark = 0;
    if (buf->imarklist)
        buf->imarklist->nmark = 0;

    if (buf->header_source)
    {
        if (buf->currentURL.scheme != SCM_LOCAL ||
            buf->mailcap_source || !strcmp(buf->currentURL.file, "-"))
        {
            URLFile h;
            init_stream(&h, SCM_LOCAL, NULL);
            examineFile(buf->header_source, &h);
            if (h.stream)
            {
                readHeader(&h, buf, TRUE, NULL);
                UFclose(&h);
            }
        }
        else if (buf->search_header) /* -m option */
            readHeader(&f, buf, TRUE, NULL);
    }

#ifdef USE_M17N
    WcOption.auto_detect = WC_OPT_DETECT_OFF;
    UseContentCharset = FALSE;
#endif
    if (is_html_type(buf->type))
        loadHTMLBuffer(&f, buf);
    else
        loadBuffer(&f, buf);
    UFclose(&f);
#ifdef USE_M17N
    WcOption.auto_detect = old_auto_detect;
    UseContentCharset = TRUE;
#endif

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
#ifdef USE_NNTP
    if (buf->check_url & CHK_NMID)
        chkNMIDBuffer(buf);
    if (buf->real_scheme == SCM_NNTP || buf->real_scheme == SCM_NEWS)
        reAnchorNewsheader(buf);
#endif
    formResetBuffer(buf, sbuf->formitem);
}

BufferPtr
prevBuffer(BufferPtr first, BufferPtr buf)
{
    BufferPtr b;

    for (b = first; b != NULL && b->nextBuffer != buf; b = b->nextBuffer)
        ;
    return b;
}

#define fwrite1(d, f) (fwrite(&d, sizeof(d), 1, f) == 0)
#define fread1(d, f) (fread(&d, sizeof(d), 1, f) == 0)

int writeBufferCache(BufferPtr buf)
{
    Str tmp;
    FILE *cache = NULL;
    Line *l;
#ifdef USE_ANSI_COLOR
    int colorflag;
#endif

    if (buf->savecache)
        return -1;

    if (buf->firstLine == NULL)
        goto _error1;

    tmp = tmpfname(TMPF_CACHE, NULL);
    buf->savecache = tmp->ptr;
    cache = fopen(buf->savecache, "w");
    if (!cache)
        goto _error1;

    if (fwrite1(buf->currentLine->linenumber, cache) ||
        fwrite1(buf->topLine->linenumber, cache))
        goto _error;

    for (l = buf->firstLine; l; l = l->next)
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
    unlink(buf->savecache);
_error1:
    buf->savecache = NULL;
    return -1;
}

int readBufferCache(BufferPtr buf)
{
    FILE *cache;
    Line *l = NULL, *prevl = NULL, *basel = NULL;
    long lnum = 0, clnum, tlnum;
#ifdef USE_ANSI_COLOR
    int colorflag;
#endif

    if (buf->savecache == NULL)
        return -1;

    cache = fopen(buf->savecache, "r");
    if (cache == NULL || fread1(clnum, cache) || fread1(tlnum, cache))
    {
        buf->savecache = NULL;
        return -1;
    }

    while (!feof(cache))
    {
        lnum++;
        prevl = l;
        l = New(Line);
        l->prev = prevl;
        if (prevl)
            prevl->next = l;
        else
            buf->firstLine = l;
        l->linenumber = lnum;
        if (lnum == clnum)
            buf->currentLine = l;
        if (lnum == tlnum)
            buf->topLine = l;
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
    buf->lastLine = prevl;
    buf->lastLine->next = NULL;
    fclose(cache);
    unlink(buf->savecache);
    buf->savecache = NULL;
    return 0;
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
        set_environ("W3M_URL", parsedURL2Str(&buf->currentURL)->ptr);
        set_environ("W3M_TYPE", (char *)(buf->real_type ? buf->real_type : "unknown"));
#ifdef USE_M17N
        set_environ("W3M_CHARSET", wc_ces_to_charset(buf->document_charset));
#endif
    }
    l = buf->currentLine;
    if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos))
    {
        Anchor *a;
        ParsedURL pu;
        char *s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", (char *)(s ? s : ""));
        a = retrieveCurrentAnchor(buf);
        if (a)
        {
            parseURL2(a->url, &pu, baseURL(buf));
            set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
        }
        else
            set_environ("W3M_CURRENT_LINK", "");
        a = retrieveCurrentImg(buf);
        if (a)
        {
            parseURL2(a->url, &pu, baseURL(buf));
            set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
        }
        else
            set_environ("W3M_CURRENT_IMG", "");
        a = retrieveCurrentForm(buf);
        if (a)
            set_environ("W3M_CURRENT_FORM", form2str((FormItemList *)a->url));
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
