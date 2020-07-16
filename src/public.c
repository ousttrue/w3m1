#include "fm.h"
#include "public.h"
#include <setjmp.h>
#include <signal.h>

void escKeyProc(int c, int esc, unsigned char *map)
{
    if (CurrentKey >= 0 && CurrentKey & K_MULTI)
    {
        unsigned char **mmap;
        mmap = (unsigned char **)getKeyData(MULTI_KEY(CurrentKey));
        if (!mmap)
            return;
        switch (esc)
        {
        case K_ESCD:
            map = mmap[3];
            break;
        case K_ESCB:
            map = mmap[2];
            break;
        case K_ESC:
            map = mmap[1];
            break;
        default:
            map = mmap[0];
            break;
        }
        esc |= (CurrentKey & ~0xFFFF);
    }
    CurrentKey = esc | c;
    w3mFuncList[(int)map[c]].func();
}

static int s_prec_num = 0;
int prec_num()
{
    return s_prec_num;
}
void set_prec_num(int n)
{
    s_prec_num = n;
}

int PREC_NUM()
{
    return prec_num() ? prec_num() : 1;
}

int searchKeyNum(void)
{
    char *d;
    int n = 1;

    d = searchKeyData();
    if (d != NULL)
        n = atoi(d);
    return n * PREC_NUM();
}

void nscroll(int n, int mode)
{
    Buffer *buf = Currentbuf;
    Line *top = buf->topLine, *cur = buf->currentLine;
    int lnum, tlnum, llnum, diff_n;

    if (buf->firstLine == NULL)
        return;
    lnum = cur->linenumber;
    buf->topLine = lineSkip(buf, top, n, FALSE);
    if (buf->topLine == top)
    {
        lnum += n;
        if (lnum < buf->topLine->linenumber)
            lnum = buf->topLine->linenumber;
        else if (lnum > buf->lastLine->linenumber)
            lnum = buf->lastLine->linenumber;
    }
    else
    {
        tlnum = buf->topLine->linenumber;
        llnum = buf->topLine->linenumber + buf->LINES - 1;
        if (nextpage_topline)
            diff_n = 0;
        else
            diff_n = n - (tlnum - top->linenumber);
        if (lnum < tlnum)
            lnum = tlnum + diff_n;
        if (lnum > llnum)
            lnum = llnum + diff_n;
    }
    gotoLine(buf, lnum);
    arrangeLine(buf);
    if (n > 0)
    {
        if (buf->currentLine->bpos &&
            buf->currentLine->bwidth >= buf->currentColumn + buf->visualpos)
            cursorDown(buf, 1);
        else
        {
            while (buf->currentLine->next && buf->currentLine->next->bpos &&
                   buf->currentLine->bwidth + buf->currentLine->width <
                       buf->currentColumn + buf->visualpos)
                cursorDown0(buf, 1);
        }
    }
    else
    {
        if (buf->currentLine->bwidth + buf->currentLine->width <
            buf->currentColumn + buf->visualpos)
            cursorUp(buf, 1);
        else
        {
            while (buf->currentLine->prev && buf->currentLine->bpos &&
                   buf->currentLine->bwidth >=
                       buf->currentColumn + buf->visualpos)
                cursorUp0(buf, 1);
        }
    }
    displayBuffer(buf, mode);
}

static char *SearchString = NULL;
int (*searchRoutine)(Buffer *, char *);

void clear_mark(Line *l)
{
    int pos;
    if (!l)
        return;
    for (pos = 0; pos < l->size; pos++)
        l->propBuf[pos] &= ~PE_MARK;
}

void disp_srchresult(int result, char *prompt, char *str)
{
    if (str == NULL)
        str = "";
    if (result & SR_NOTFOUND)
        disp_message(Sprintf("Not found: %s", str)->ptr, TRUE);
    else if (result & SR_WRAPPED)
        disp_message(Sprintf("Search wrapped: %s", str)->ptr, TRUE);
    else if (show_srch_str)
        disp_message(Sprintf("%s%s", prompt, str)->ptr, TRUE);
}

void srch_nxtprv(int reverse)
{
    int result;
    /* *INDENT-OFF* */
    static int (*routine[2])(Buffer *, char *) = {
        forwardSearch, backwardSearch};
    /* *INDENT-ON* */

    if (searchRoutine == NULL)
    {
        /* FIXME: gettextize? */
        disp_message("No previous regular expression", TRUE);
        return;
    }
    if (reverse != 0)
        reverse = 1;
    if (searchRoutine == backwardSearch)
        reverse ^= 1;
    if (reverse == 0)
        Currentbuf->pos += 1;
    result = srchcore(SearchString, routine[reverse]);
    if (result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    displayBuffer(Currentbuf, B_NORMAL);
    disp_srchresult(result, (char *)(reverse ? "Backward: " : "Forward: "),
                    SearchString);
}

#ifndef __MINGW32_VERSION
JMP_BUF IntReturn;
#else
_JBTYPE IntReturn[_JBLEN];
#endif /* __MINGW32_VERSION */

MySignalHandler
    intTrap(SIGNAL_ARG)
{ /* Interrupt catcher */
    LONGJMP(IntReturn, 0);
    SIGNAL_RETURN;
}

static void
dump_extra(Buffer *buf)
{
    printf("W3m-current-url: %s\n", parsedURL2Str(&buf->currentURL)->ptr);
    if (buf->baseURL)
        printf("W3m-base-url: %s\n", parsedURL2Str(buf->baseURL)->ptr);
#ifdef USE_M17N
    printf("W3m-document-charset: %s\n",
           wc_ces_to_charset(buf->document_charset));
#endif
#ifdef USE_SSL
    if (buf->ssl_certificate)
    {
        Str tmp = Strnew();
        char *p;
        for (p = buf->ssl_certificate; *p; p++)
        {
            Strcat_char(tmp, *p);
            if (*p == '\n')
            {
                for (; *(p + 1) == '\n'; p++)
                    ;
                if (*(p + 1))
                    Strcat_char(tmp, '\t');
            }
        }
        if (Strlastchar(tmp) != '\n')
            Strcat_char(tmp, '\n');
        printf("W3m-ssl-certificate: %s", tmp->ptr);
    }
#endif
}

static void
dump_source(Buffer *buf)
{
    FILE *f;
    char c;
    if (buf->sourcefile == NULL)
        return;
    f = fopen(buf->sourcefile, "r");
    if (f == NULL)
        return;
    while (c = fgetc(f), !feof(f))
    {
        putchar(c);
    }
    fclose(f);
}

static void
dump_head(Buffer *buf)
{
    TextListItem *ti;

    if (buf->document_header == NULL)
    {
        if (w3m_dump & DUMP_EXTRA)
            printf("\n");
        return;
    }
    for (ti = buf->document_header->first; ti; ti = ti->next)
    {
#ifdef USE_M17N
        printf("%s",
               wc_conv_strict(ti->ptr, InnerCharset,
                              buf->document_charset)
                   ->ptr);
#else
        printf("%s", ti->ptr);
#endif
    }
    puts("");
}

void do_dump(Buffer *buf)
{
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

    prevtrap = mySignal(SIGINT, intTrap);
    if (SETJMP(IntReturn) != 0)
    {
        mySignal(SIGINT, prevtrap);
        return;
    }
    if (w3m_dump & DUMP_EXTRA)
        dump_extra(buf);
    if (w3m_dump & DUMP_HEAD)
        dump_head(buf);
    if (w3m_dump & DUMP_SOURCE)
        dump_source(buf);
    if (w3m_dump == DUMP_BUFFER)
    {
        int i;
        saveBuffer(buf, stdout, FALSE);
        if (displayLinkNumber && buf->href)
        {
            printf("\nReferences:\n\n");
            for (i = 0; i < buf->href->nanchor; i++)
            {
                ParsedURL pu;
                static Str s = NULL;
                if (buf->href->anchors[i].slave)
                    continue;
                parseURL2(buf->href->anchors[i].url, &pu, baseURL(buf));
                s = parsedURL2Str(&pu);
                if (DecodeURL)
                    s = Strnew_charp(url_unquote_conv(s->ptr, Currentbuf->document_charset));
                printf("[%d] %s\n", buf->href->anchors[i].hseq + 1, s->ptr);
            }
        }
    }
    mySignal(SIGINT, prevtrap);
}

/* search by regular expression */
int srchcore(char *volatile str, int (*func)(Buffer *, char *))
{
    volatile int i, result = SR_NOTFOUND;

    if (str != NULL && str != SearchString)
        SearchString = str;
    if (SearchString == NULL || *SearchString == '\0')
        return SR_NOTFOUND;

    str = conv_search_string(SearchString, DisplayCharset);
    auto prevtrap = mySignal(SIGINT, intTrap);
    crmode();
    if (SETJMP(IntReturn) == 0)
    {
        for (i = 0; i < PREC_NUM(); i++)
        {
            result = func(Currentbuf, str);
            if (i < PREC_NUM() - 1 && result & SR_FOUND)
                clear_mark(Currentbuf->currentLine);
        }
    }
    mySignal(SIGINT, prevtrap);
    term_raw();
    return result;
}

int dispincsrch(int ch, Str buf, Lineprop *prop)
{
    static Buffer sbuf;
    static Line *currentLine;
    static int pos;
    char *str;
    int do_next_search = FALSE;

    if (ch == 0 && buf == NULL)
    {
        SAVE_BUFPOSITION(&sbuf); /* search starting point */
        currentLine = sbuf.currentLine;
        pos = sbuf.pos;
        return -1;
    }

    str = buf->ptr;
    switch (ch)
    {
    case 022: /* C-r */
        searchRoutine = backwardSearch;
        do_next_search = TRUE;
        break;
    case 023: /* C-s */
        searchRoutine = forwardSearch;
        do_next_search = TRUE;
        break;

#ifdef USE_MIGEMO
    case 034:
        migemo_active = -migemo_active;
        goto done;
#endif

    default:
        if (ch >= 0)
            return ch; /* use InputKeymap */
    }

    if (do_next_search)
    {
        if (*str)
        {
            if (searchRoutine == forwardSearch)
                Currentbuf->pos += 1;
            SAVE_BUFPOSITION(&sbuf);
            if (srchcore(str, searchRoutine) == SR_NOTFOUND && searchRoutine == forwardSearch)
            {
                Currentbuf->pos -= 1;
                SAVE_BUFPOSITION(&sbuf);
            }
            arrangeCursor(Currentbuf);
            displayBuffer(Currentbuf, B_FORCE_REDRAW);
            clear_mark(Currentbuf->currentLine);
            return -1;
        }
        else
            return 020; /* _prev completion for C-s C-s */
    }
    else if (*str)
    {
        RESTORE_BUFPOSITION(&sbuf);
        arrangeCursor(Currentbuf);
        srchcore(str, searchRoutine);
        arrangeCursor(Currentbuf);
        currentLine = Currentbuf->currentLine;
        pos = Currentbuf->pos;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    clear_mark(Currentbuf->currentLine);
#ifdef USE_MIGEMO
done:
    while (*str++ != '\0')
    {
        if (migemo_active > 0)
            *prop++ |= PE_UNDER;
        else
            *prop++ &= ~PE_UNDER;
    }
#endif
    return -1;
}

void isrch(int (*func)(Buffer *, char *), char *prompt)
{
    char *str;
    Buffer sbuf;
    SAVE_BUFPOSITION(&sbuf);
    dispincsrch(0, NULL, NULL); /* initialize incremental search state */

    searchRoutine = func;
    str = inputLineHistSearch(prompt, NULL, IN_STRING, TextHist, dispincsrch);
    if (str == NULL)
    {
        RESTORE_BUFPOSITION(&sbuf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void srch(int (*func)(Buffer *, char *), char *prompt)
{
    char *str;
    int result;
    int disp = FALSE;
    int pos;

    str = searchKeyData();
    if (str == NULL || *str == '\0')
    {
        str = inputStrHist(prompt, NULL, TextHist);
        if (str != NULL && *str == '\0')
            str = SearchString;
        if (str == NULL)
        {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
        disp = TRUE;
    }
    pos = Currentbuf->pos;
    if (func == forwardSearch)
        Currentbuf->pos += 1;
    result = srchcore(str, func);
    if (result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    else
        Currentbuf->pos = pos;
    displayBuffer(Currentbuf, B_NORMAL);
    if (disp)
        disp_srchresult(result, prompt, str);
    searchRoutine = func;
}

void shiftvisualpos(Buffer *buf, int shift)
{
    Line *l = buf->currentLine;
    buf->visualpos -= shift;
    if (buf->visualpos - l->bwidth >= buf->COLS)
        buf->visualpos = l->bwidth + buf->COLS - 1;
    else if (buf->visualpos - l->bwidth < 0)
        buf->visualpos = l->bwidth;
    arrangeLine(buf);
    if (buf->visualpos - l->bwidth == -shift && buf->cursorX == 0)
        buf->visualpos = l->bwidth;
}

void pushBuffer(Buffer *buf)
{
    Buffer *b;

#ifdef USE_IMAGE
    deleteImage(Currentbuf);
#endif
    if (clear_buffer)
        tmpClearBuffer(Currentbuf);
    if (Firstbuf == Currentbuf)
    {
        buf->nextBuffer = Firstbuf;
        Firstbuf = Currentbuf = buf;
    }
    else if ((b = prevBuffer(Firstbuf, Currentbuf)) != NULL)
    {
        b->nextBuffer = buf;
        buf->nextBuffer = Currentbuf;
        Currentbuf = buf;
    }
#ifdef USE_BUFINFO
    saveBufferInfo();
#endif
}

void cmd_loadfile(char *fn)
{
    Buffer *buf;

    buf = loadGeneralFile(file_to_url(fn), NULL, NO_REFERER, 0, NULL);
    if (buf == NULL)
    {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
        disp_err_message(emsg, FALSE);
    }
    else if (buf != NO_BUFFER)
    {
        pushBuffer(buf);
        if (RenderFrame && Currentbuf->frameset != NULL)
            rFrame();
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void cmd_loadURL(char *url, ParsedURL *current, char *referer, FormList *request)
{
    Buffer *buf;

    if (handleMailto(url))
        return;
#if 0
    if (!strncasecmp(url, "news:", 5) && strchr(url, '@') == NULL) {
	/* news:newsgroup is not supported */
	/* FIXME: gettextize? */
	disp_err_message("news:newsgroup_name is not supported", TRUE);
	return;
    }
#endif /* USE_NNTP */

    refresh();
    buf = loadGeneralFile(url, current, referer, 0, request);
    if (buf == NULL)
    {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
        disp_err_message(emsg, FALSE);
    }
    else if (buf != NO_BUFFER)
    {
        pushBuffer(buf);
        if (RenderFrame && Currentbuf->frameset != NULL)
            rFrame();
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

int handleMailto(char *url)
{
    Str to;
    char *pos;

    if (strncasecmp(url, "mailto:", 7))
        return 0;
#ifdef USE_W3MMAILER
    if (!non_null(Mailer) || MailtoOptions == MAILTO_OPTIONS_USE_W3MMAILER)
        return 0;
#else
    if (!non_null(Mailer))
    {
        /* FIXME: gettextize? */
        disp_err_message("no mailer is specified", TRUE);
        return 1;
    }
#endif

    /* invoke external mailer */
    if (MailtoOptions == MAILTO_OPTIONS_USE_MAILTO_URL)
    {
        to = Strnew_charp(html_unquote(url));
    }
    else
    {
        to = Strnew_charp(url + 7);
        if ((pos = strchr(to->ptr, '?')) != NULL)
            Strtruncate(to, pos - to->ptr);
    }
    fmTerm();
    system(myExtCommand(Mailer, shell_quote(file_unquote(to->ptr)),
                        FALSE)
               ->ptr);
    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    pushHashHist(URLHist, url);
    return 1;
}

/* Move cursor left */
void _movL(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorLeft(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor downward */
void _movD(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorDown(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* move cursor upward */
void _movU(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorUp(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor right */
void _movR(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorRight(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

int prev_nonnull_line(Line *line)
{
    Line *l;

    for (l = line; l != NULL && l->len == 0; l = l->prev)
        ;
    if (l == NULL || l->len == 0)
        return -1;

    Currentbuf->currentLine = l;
    if (l != line)
        Currentbuf->pos = Currentbuf->currentLine->len;
    return 0;
}

int next_nonnull_line(Line *line)
{
    Line *l;

    for (l = line; l != NULL && l->len == 0; l = l->next)
        ;

    if (l == NULL || l->len == 0)
        return -1;

    Currentbuf->currentLine = l;
    if (l != line)
        Currentbuf->pos = 0;
    return 0;
}

char *
getCurWord(Buffer *buf, int *spos, int *epos)
{
    char *p;
    Line *l = buf->currentLine;
    int b, e;

    *spos = 0;
    *epos = 0;
    if (l == NULL)
        return NULL;
    p = l->lineBuf;
    e = buf->pos;
    while (e > 0 && !is_wordchar(getChar(&p[e])))
        prevChar(&e, l);
    if (!is_wordchar(getChar(&p[e])))
        return NULL;
    b = e;
    while (b > 0)
    {
        int tmp = b;
        prevChar(&tmp, l);
        if (!is_wordchar(getChar(&p[tmp])))
            break;
        b = tmp;
    }
    while (e < l->len && is_wordchar(getChar(&p[e])))
        nextChar(&e, l);
    *spos = b;
    *epos = e;
    return &p[b];
}

/* 
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900 
 */
void prevChar(int *s, Line *l)
{
    do
    {
        (*s)--;
    } while ((*s) > 0 && (l)->propBuf[*s] & PC_WCHAR2);
}

void nextChar(int *s, Line *l)
{
    do
    {
        (*s)++;
    } while ((*s) < (l)->len && (l)->propBuf[*s] & PC_WCHAR2);
}

wc_uint32 getChar(char *p)
{
    return wc_any_to_ucs(wtf_parse1((wc_uchar **)&p));
}

int is_wordchar(wc_uint32 c)
{
    return wc_is_ucs_alnum(c);
}
