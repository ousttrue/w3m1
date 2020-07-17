#include "fm.h"
#include "public.h"
#include <setjmp.h>
#include <signal.h>
#include "ucs.h"
#include "proto.h"

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

void _quitfm(int confirm)
{
    char *ans = "y";

    if (checkDownloadList())
        /* FIXME: gettextize? */
        ans = inputChar("Download process retains. "
                        "Do you want to exit w3m? (y/n)");
    else if (confirm)
        /* FIXME: gettextize? */
        ans = inputChar("Do you want to exit w3m? (y/n)");
    if (!(ans && TOLOWER(*ans) == 'y'))
    {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }

    term_title(""); /* XXX */
#ifdef USE_IMAGE
    if (activeImage)
        termImage();
#endif
    fmTerm();
#ifdef USE_COOKIE
    save_cookies();
#endif /* USE_COOKIE */
#ifdef USE_HISTORY
    if (UseHistory && SaveURLHist)
        saveHistory(URLHist, URLHistSize);
#endif /* USE_HISTORY */
    w3m_exit(0);
}

void delBuffer(Buffer *buf)
{
    if (buf == NULL)
        return;
    if (Currentbuf == buf)
        Currentbuf = buf->nextBuffer;
    Firstbuf = deleteBuffer(Firstbuf, buf);
    if (!Currentbuf)
        Currentbuf = Firstbuf;
}

/* Go to specified line */
void _goLine(char *l)
{
    if (l == NULL || *l == '\0' || Currentbuf->currentLine == NULL)
    {
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    }
    Currentbuf->pos = 0;
    if (((*l == '^') || (*l == '$')) && prec_num())
    {
        gotoRealLine(Currentbuf, prec_num());
    }
    else if (*l == '^')
    {
        Currentbuf->topLine = Currentbuf->currentLine = Currentbuf->firstLine;
    }
    else if (*l == '$')
    {
        Currentbuf->topLine =
            lineSkip(Currentbuf, Currentbuf->lastLine,
                     -(Currentbuf->LINES + 1) / 2, TRUE);
        Currentbuf->currentLine = Currentbuf->lastLine;
    }
    else
        gotoRealLine(Currentbuf, atoi(l));
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

int cur_real_linenumber(Buffer *buf)
{
    Line *l, *cur = buf->currentLine;
    int n;

    if (!cur)
        return 1;
    n = cur->real_linenumber ? cur->real_linenumber : 1;
    for (l = buf->firstLine; l && l != cur && l->real_linenumber == 0; l = l->next)
    { /* header */
        if (l->bpos == 0)
            n++;
    }
    return n;
}

char *inputLineHist(char *prompt, char *def_str, int flag, Hist *hist)
{
    return inputLineHistSearch(prompt, def_str, flag, hist, NULL);
}

char *inputStrHist(char *prompt, char *def_str, Hist *hist)
{
    return inputLineHist(prompt, def_str, IN_STRING, hist);
}

char *inputLine(char *prompt, char *def_str, int flag)
{
    return inputLineHist(prompt, def_str, flag, NULL);
}

static char *s_MarkString = NULL;

char *MarkString()
{
    return s_MarkString;
}

void SetMarkString(char *str)
{
    s_MarkString = str;
}

void _followForm(int submit)
{
    Line *l;
    Anchor *a, *a2;
    char *p;
    FormItemList *fi, *f2;
    Str tmp, tmp2;
    int multipart = 0, i;

    if (Currentbuf->firstLine == NULL)
        return;
    l = Currentbuf->currentLine;

    a = retrieveCurrentForm(Currentbuf);
    if (a == NULL)
        return;
    fi = (FormItemList *)a->url;
    switch (fi->type)
    {
    case FORM_INPUT_TEXT:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        /* FIXME: gettextize? */
        p = inputStrHist("TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept || fi->parent->nitems == 1)
            goto do_submit;
        break;
    case FORM_INPUT_FILE:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        /* FIXME: gettextize? */
        p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : NULL,
                              NULL);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept || fi->parent->nitems == 1)
            goto do_submit;
        break;
    case FORM_INPUT_PASSWORD:
        if (submit)
            goto do_submit;
        if (fi->readonly)
        {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        /* FIXME: gettextize? */
        p = inputLine("Password:", fi->value ? fi->value->ptr : NULL,
                      IN_PASSWORD);
        if (p == NULL)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept)
            goto do_submit;
        break;
    case FORM_TEXTAREA:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        input_textarea(fi);
        formUpdateBuffer(a, Currentbuf, fi);
        break;
    case FORM_INPUT_RADIO:
        if (submit)
            goto do_submit;
        if (fi->readonly)
        {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        formRecheckRadio(a, Currentbuf, fi);
        break;
    case FORM_INPUT_CHECKBOX:
        if (submit)
            goto do_submit;
        if (fi->readonly)
        {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        fi->checked = !fi->checked;
        formUpdateBuffer(a, Currentbuf, fi);
        break;
#ifdef MENU_SELECT
    case FORM_SELECT:
        if (submit)
            goto do_submit;
        if (!formChooseOptionByMenu(fi,
                                    Currentbuf->cursorX - Currentbuf->pos +
                                        a->start.pos + Currentbuf->rootX,
                                    Currentbuf->cursorY + Currentbuf->rootY))
            break;
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->parent->nitems == 1)
            goto do_submit;
        break;
#endif /* MENU_SELECT */
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    do_submit:
        tmp = Strnew();
        tmp2 = Strnew();
        multipart = (fi->parent->method == FORM_METHOD_POST &&
                     fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
        query_from_followform(&tmp, fi, multipart);

        tmp2 = Strdup(fi->parent->action);
        if (!Strcmp_charp(tmp2, "!CURRENT_URL!"))
        {
            /* It means "current URL" */
            tmp2 = parsedURL2Str(&Currentbuf->currentURL);
            if ((p = strchr(tmp2->ptr, '?')) != NULL)
                Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
        }

        if (fi->parent->method == FORM_METHOD_GET)
        {
            if ((p = strchr(tmp2->ptr, '?')) != NULL)
                Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
            Strcat_charp(tmp2, "?");
            Strcat(tmp2, tmp);
            loadLink(tmp2->ptr, a->target, NULL, NULL);
        }
        else if (fi->parent->method == FORM_METHOD_POST)
        {
            Buffer *buf;
            if (multipart)
            {
                struct stat st;
                stat(fi->parent->body, &st);
                fi->parent->length = st.st_size;
            }
            else
            {
                fi->parent->body = tmp->ptr;
                fi->parent->length = tmp->length;
            }
            buf = loadLink(tmp2->ptr, a->target, NULL, fi->parent);
            if (multipart)
            {
                unlink(fi->parent->body);
            }
            if (buf && !(buf->bufferprop & BP_REDIRECTED))
            { /* buf must be Currentbuf */
                /* BP_REDIRECTED means that the buffer is obtained through
		 * Location: header. In this case, buf->form_submit must not be set
		 * because the page is not loaded by POST method but GET method.
		 */
                buf->form_submit = save_submit_formlist(fi);
            }
        }
        else if ((fi->parent->method == FORM_METHOD_INTERNAL && (!Strcmp_charp(fi->parent->action, "map") || !Strcmp_charp(fi->parent->action, "none"))) || Currentbuf->bufferprop & BP_INTERNAL)
        { /* internal */
            do_internal(tmp2->ptr, tmp->ptr);
        }
        else
        {
            disp_err_message("Can't send form because of illegal method.",
                             FALSE);
        }
        break;
    case FORM_INPUT_RESET:
        for (i = 0; i < Currentbuf->formitem->nanchor; i++)
        {
            a2 = &Currentbuf->formitem->anchors[i];
            f2 = (FormItemList *)a2->url;
            if (f2->parent == fi->parent &&
                f2->name && f2->value &&
                f2->type != FORM_INPUT_SUBMIT &&
                f2->type != FORM_INPUT_HIDDEN &&
                f2->type != FORM_INPUT_RESET)
            {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
#ifdef MENU_SELECT
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
#endif /* MENU_SELECT */
                formUpdateBuffer(a2, Currentbuf, f2);
            }
        }
        break;
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void query_from_followform(Str *query, FormItemList *fi, int multipart)
{
    FormItemList *f2;
    FILE *body = NULL;

    if (multipart)
    {
        *query = tmpfname(TMPF_DFL, NULL);
        body = fopen((*query)->ptr, "w");
        if (body == NULL)
        {
            return;
        }
        fi->parent->body = (*query)->ptr;
        fi->parent->boundary =
            Sprintf("------------------------------%d%ld%ld%ld", CurrentPid,
                    fi->parent, fi->parent->body, fi->parent->boundary)
                ->ptr;
    }
    *query = Strnew();
    for (f2 = fi->parent->item; f2; f2 = f2->next)
    {
        if (f2->name == NULL)
            continue;
        /* <ISINDEX> is translated into single text form */
        if (f2->name->length == 0 &&
            (multipart || f2->type != FORM_INPUT_TEXT))
            continue;
        switch (f2->type)
        {
        case FORM_INPUT_RESET:
            /* do nothing */
            continue;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_IMAGE:
            if (f2 != fi || f2->value == NULL)
                continue;
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (!f2->checked)
                continue;
        }
        if (multipart)
        {
            if (f2->type == FORM_INPUT_IMAGE)
            {
                int x = 0, y = 0;
#ifdef USE_IMAGE
                getMapXY(Currentbuf, retrieveCurrentImg(Currentbuf), &x, &y);
#endif
                *query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
                Strcat_charp(*query, ".x");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                                Sprintf("%d", x)->ptr);
                *query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
                Strcat_charp(*query, ".y");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                                Sprintf("%d", y)->ptr);
            }
            else if (f2->name && f2->name->length > 0 && f2->value != NULL)
            {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value, fi, Currentbuf);
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                                         conv_form_encoding(f2->name, fi,
                                                            Currentbuf)
                                             ->ptr,
                                         (*query)->ptr,
                                         Str_conv_to_system(f2->value)->ptr);
                else
                    form_write_data(body, fi->parent->boundary,
                                    conv_form_encoding(f2->name, fi,
                                                       Currentbuf)
                                        ->ptr,
                                    (*query)->ptr);
            }
        }
        else
        {
            /* not multipart */
            if (f2->type == FORM_INPUT_IMAGE)
            {
                int x = 0, y = 0;
#ifdef USE_IMAGE
                getMapXY(Currentbuf, retrieveCurrentImg(Currentbuf), &x, &y);
#endif
                Strcat(*query,
                       Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                Strcat(*query, Sprintf(".x=%d&", x));
                Strcat(*query,
                       Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                Strcat(*query, Sprintf(".y=%d", y));
            }
            else
            {
                /* not IMAGE */
                if (f2->name && f2->name->length > 0)
                {
                    Strcat(*query,
                           Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                    Strcat_char(*query, '=');
                }
                if (f2->value != NULL)
                {
                    if (fi->parent->method == FORM_METHOD_INTERNAL)
                        Strcat(*query, Str_form_quote(f2->value));
                    else
                    {
                        Strcat(*query,
                               Str_form_quote(conv_form_encoding(f2->value, fi, Currentbuf)));
                    }
                }
            }
            if (f2->next)
                Strcat_char(*query, '&');
        }
    }
    if (multipart)
    {
        fprintf(body, "--%s--\r\n", fi->parent->boundary);
        fclose(body);
    }
    else
    {
        /* remove trailing & */
        while (Strlastchar(*query) == '&')
            Strshrink(*query, 1);
    }
}

int on_target = 1;

/* follow HREF link in the buffer */
void bufferA(void)
{
    on_target = FALSE;
    followA();
    on_target = TRUE;
}

Buffer *
loadLink(char *url, char *target, char *referer, FormList *request)
{
    Buffer *buf, *nfbuf;
    union frameset_element *f_element = NULL;
    int flag = 0;
    ParsedURL *base, pu;

    message(Sprintf("loading %s", url)->ptr, 0, 0);
    refresh();

    base = baseURL(Currentbuf);
    if (base == NULL ||
        base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2Str(&Currentbuf->currentURL)->ptr;
    buf = loadGeneralFile(url, baseURL(Currentbuf), referer, flag, request);
    if (buf == NULL)
    {
        char *emsg = Sprintf("Can't load %s", url)->ptr;
        disp_err_message(emsg, FALSE);
        return NULL;
    }

    parseURL2(url, &pu, base);
    pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

    if (buf == NO_BUFFER)
    {
        return NULL;
    }
    if (!on_target) /* open link as an indivisual page */
        return loadNormalBuf(buf, TRUE);

    if (do_download) /* download (thus no need to render frame) */
        return loadNormalBuf(buf, FALSE);

    if (target == NULL ||                    /* no target specified (that means this page is not a frame page) */
        !strcmp(target, "_top") ||           /* this link is specified to be opened as an indivisual * page */
        !(Currentbuf->bufferprop & BP_FRAME) /* This page is not a frame page */
    )
    {
        return loadNormalBuf(buf, TRUE);
    }
    nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
    if (nfbuf == NULL)
    {
        /* original page (that contains <frameset> tag) doesn't exist */
        return loadNormalBuf(buf, TRUE);
    }

    f_element = search_frame(nfbuf->frameset, target);
    if (f_element == NULL)
    {
        /* specified target doesn't exist in this frameset */
        return loadNormalBuf(buf, TRUE);
    }

    /* frame page */

    /* stack current frameset */
    pushFrameTree(&(nfbuf->frameQ), copyFrameSet(nfbuf->frameset), Currentbuf);
    /* delete frame view buffer */
    delBuffer(Currentbuf);
    Currentbuf = nfbuf;
    /* nfbuf->frameset = copyFrameSet(nfbuf->frameset); */
    resetFrameElement(f_element, buf, referer, request);
    discardBuffer(buf);
    rFrame();
    {
        Anchor *al = NULL;
        char *label = pu.label;

        if (label && f_element->element->attr == F_BODY)
        {
            al = searchAnchor(f_element->body->nameList, label);
        }
        if (!al)
        {
            label = Strnew_m_charp("_", target, NULL)->ptr;
            al = searchURLLabel(Currentbuf, label);
        }
        if (al)
        {
            gotoLine(Currentbuf, al->start.line);
            if (label_topline)
                Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
                                               Currentbuf->currentLine->linenumber -
                                                   Currentbuf->topLine->linenumber,
                                               FALSE);
            Currentbuf->pos = al->start.pos;
            arrangeCursor(Currentbuf);
        }
    }
    displayBuffer(Currentbuf, B_NORMAL);
    return buf;
}

FormItemList *save_submit_formlist(FormItemList *src)
{
    FormList *list;
    FormList *srclist;
    FormItemList *srcitem;
    FormItemList *item;
    FormItemList *ret = NULL;
#ifdef MENU_SELECT
    FormSelectOptionItem *opt;
    FormSelectOptionItem *curopt;
    FormSelectOptionItem *srcopt;
#endif /* MENU_SELECT */

    if (src == NULL)
        return NULL;
    srclist = src->parent;
    list = New(FormList);
    list->method = srclist->method;
    list->action = Strdup(srclist->action);
#ifdef USE_M17N
    list->charset = srclist->charset;
#endif
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next)
    {
        item = New(FormItemList);
        item->type = srcitem->type;
        item->name = Strdup(srcitem->name);
        item->value = Strdup(srcitem->value);
        item->checked = srcitem->checked;
        item->accept = srcitem->accept;
        item->size = srcitem->size;
        item->rows = srcitem->rows;
        item->maxlength = srcitem->maxlength;
        item->readonly = srcitem->readonly;
#ifdef MENU_SELECT
        opt = curopt = NULL;
        for (srcopt = srcitem->select_option; srcopt; srcopt = srcopt->next)
        {
            if (!srcopt->checked)
                continue;
            opt = New(FormSelectOptionItem);
            opt->value = Strdup(srcopt->value);
            opt->label = Strdup(srcopt->label);
            opt->checked = srcopt->checked;
            if (item->select_option == NULL)
            {
                item->select_option = curopt = opt;
            }
            else
            {
                curopt->next = opt;
                curopt = curopt->next;
            }
        }
        item->select_option = opt;
        if (srcitem->label)
            item->label = Strdup(srcitem->label);
#endif /* MENU_SELECT */
        item->parent = list;
        item->next = NULL;

        if (list->lastitem == NULL)
        {
            list->item = list->lastitem = item;
        }
        else
        {
            list->lastitem->next = item;
            list->lastitem = item;
        }

        if (srcitem == src)
            ret = item;
    }

    return ret;
}

Str conv_form_encoding(Str val, FormItemList *fi, Buffer *buf)
{
    wc_ces charset = SystemCharset;

    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (buf->document_charset && buf->document_charset != WC_CES_US_ASCII)
        charset = buf->document_charset;
    return wc_Str_conv_strict(val, InnerCharset, charset);
}

Buffer *loadNormalBuf(Buffer *buf, int renderframe)
{
    pushBuffer(buf);
    if (renderframe && RenderFrame && Currentbuf->frameset != NULL)
        rFrame();
    return buf;
}

/* go to the next [visited] anchor */
void _nextA(int visited)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    BufferPoint *po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    ParsedURL url;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (visited != TRUE && an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    y = Currentbuf->currentLine->linenumber;
    x = Currentbuf->pos;

    if (visited == TRUE)
    {
        n = hl->nmark;
    }

    for (i = 0; i < n; i++)
    {
        pan = an;
        if (an && an->hseq >= 0)
        {
            int hseq = an->hseq + 1;
            do
            {
                if (hseq >= hl->nmark)
                {
                    if (visited == TRUE)
                        return;
                    an = pan;
                    goto _end;
                }
                po = &hl->marks[hseq];
                an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
                if (visited != TRUE && an == NULL)
                    an = retrieveAnchor(Currentbuf->formitem, po->line,
                                        po->pos);
                hseq++;
                if (visited == TRUE && an)
                {
                    parseURL2(an->url, &url, baseURL(Currentbuf));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = closest_next_anchor(Currentbuf->href, NULL, x, y);
            if (visited != TRUE)
                an = closest_next_anchor(Currentbuf->formitem, an, x, y);
            if (an == NULL)
            {
                if (visited == TRUE)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == TRUE)
            {
                parseURL2(an->url, &url, baseURL(Currentbuf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr))
                {
                    goto _end;
                }
            }
        }
    }
    if (visited == TRUE)
        return;

_end:
    if (an == NULL || an->hseq < 0)
        return;
    po = &hl->marks[an->hseq];
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the previous anchor */
void _prevA(int visited)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    BufferPoint *po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    ParsedURL url;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (visited != TRUE && an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    y = Currentbuf->currentLine->linenumber;
    x = Currentbuf->pos;

    if (visited == TRUE)
    {
        n = hl->nmark;
    }

    for (i = 0; i < n; i++)
    {
        pan = an;
        if (an && an->hseq >= 0)
        {
            int hseq = an->hseq - 1;
            do
            {
                if (hseq < 0)
                {
                    if (visited == TRUE)
                        return;
                    an = pan;
                    goto _end;
                }
                po = hl->marks + hseq;
                an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
                if (visited != TRUE && an == NULL)
                    an = retrieveAnchor(Currentbuf->formitem, po->line,
                                        po->pos);
                hseq--;
                if (visited == TRUE && an)
                {
                    parseURL2(an->url, &url, baseURL(Currentbuf));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = closest_prev_anchor(Currentbuf->href, NULL, x, y);
            if (visited != TRUE)
                an = closest_prev_anchor(Currentbuf->formitem, an, x, y);
            if (an == NULL)
            {
                if (visited == TRUE)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == TRUE && an)
            {
                parseURL2(an->url, &url, baseURL(Currentbuf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr))
                {
                    goto _end;
                }
            }
        }
    }
    if (visited == TRUE)
        return;

_end:
    if (an == NULL || an->hseq < 0)
        return;
    po = hl->marks + an->hseq;
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

void gotoLabel(char *label)
{
    Buffer *buf;
    Anchor *al;
    int i;

    al = searchURLLabel(Currentbuf, label);
    if (al == NULL)
    {
        /* FIXME: gettextize? */
        disp_message(Sprintf("%s is not found", label)->ptr, TRUE);
        return;
    }
    buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
    for (i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    buf->currentURL.label = allocStr(label, -1);
    pushHashHist(URLHist, parsedURL2Str(&buf->currentURL)->ptr);
    (*buf->clone)++;
    pushBuffer(buf);
    gotoLine(Currentbuf, al->start.line);
    if (label_topline)
        Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
                                       Currentbuf->currentLine->linenumber - Currentbuf->topLine->linenumber,
                                       FALSE);
    Currentbuf->pos = al->start.pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
}

static int s_check_target = TRUE;
int check_target()
{
    return s_check_target;
}
void set_check_target(int check)
{
    s_check_target = check;
}

void _newT()
{
    TabBuffer *tag;
    Buffer *buf;
    int i;

    tag = newTab();
    if (!tag)
        return;

    buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
    buf->nextBuffer = NULL;
    for (i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    (*buf->clone)++;
    tag->firstBuffer = tag->currentBuffer = buf;

    tag->nextTab = CurrentTab->nextTab;
    tag->prevTab = CurrentTab;
    if (CurrentTab->nextTab)
        CurrentTab->nextTab->prevTab = tag;
    else
        LastTab = tag;
    CurrentTab->nextTab = tag;
    CurrentTab = tag;
    nTab++;
}

/* go to the next left/right anchor */
void nextX(int d, int dy)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    Anchor *an, *pan;
    Line *l;
    int i, x, y, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    l = Currentbuf->currentLine;
    x = Currentbuf->pos;
    y = l->linenumber;
    pan = NULL;
    for (i = 0; i < n; i++)
    {
        if (an)
            x = (d > 0) ? an->end.pos : an->start.pos - 1;
        an = NULL;
        while (1)
        {
            for (; x >= 0 && x < l->len; x += d)
            {
                an = retrieveAnchor(Currentbuf->href, y, x);
                if (!an)
                    an = retrieveAnchor(Currentbuf->formitem, y, x);
                if (an)
                {
                    pan = an;
                    break;
                }
            }
            if (!dy || an)
                break;
            l = (dy > 0) ? l->next : l->prev;
            if (!l)
                break;
            x = (d > 0) ? 0 : l->len - 1;
            y = l->linenumber;
        }
        if (!an)
            break;
    }

    if (pan == NULL)
        return;
    gotoLine(Currentbuf, y);
    Currentbuf->pos = pan->start.pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next downward/upward anchor */
void nextY(int d)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    int hseq;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    x = Currentbuf->pos;
    y = Currentbuf->currentLine->linenumber + d;
    pan = NULL;
    hseq = -1;
    for (i = 0; i < n; i++)
    {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= Currentbuf->lastLine->linenumber; y += d)
        {
            an = retrieveAnchor(Currentbuf->href, y, x);
            if (!an)
                an = retrieveAnchor(Currentbuf->formitem, y, x);
            if (an && hseq != abs(an->hseq))
            {
                pan = an;
                break;
            }
        }
        if (!an)
            break;
    }

    if (pan == NULL)
        return;
    gotoLine(Currentbuf, pan->start.line);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

int checkBackBuffer(Buffer *buf)
{
    Buffer *fbuf = buf->linkBuffer[LB_N_FRAME];

    if (fbuf)
    {
        if (fbuf->frameQ)
            return TRUE; /* Currentbuf has stacked frames */
        /* when no frames stacked and next is frame source, try next's
	 * nextBuffer */
        if (RenderFrame && fbuf == buf->nextBuffer)
        {
            if (fbuf->nextBuffer != NULL)
                return TRUE;
            else
                return FALSE;
        }
    }

    if (buf->nextBuffer)
        return TRUE;

    return FALSE;
}

/* go to specified URL */
void goURL0(char *prompt, int relative)
{
    char *url, *referer;
    ParsedURL p_url, *current;
    Buffer *cur_buf = Currentbuf;

    url = searchKeyData();
    if (url == NULL)
    {
        Hist *hist = copyHist(URLHist);
        Anchor *a;

        current = baseURL(Currentbuf);
        if (current)
        {
            char *c_url = parsedURL2Str(current)->ptr;
            if (DefaultURLString == DEFAULT_URL_CURRENT)
            {
                url = c_url;
                if (DecodeURL)
                    url = url_unquote_conv(url, 0);
            }
            else
                pushHist(hist, c_url);
        }
        a = retrieveCurrentAnchor(Currentbuf);
        if (a)
        {
            char *a_url;
            parseURL2(a->url, &p_url, current);
            a_url = parsedURL2Str(&p_url)->ptr;
            if (DefaultURLString == DEFAULT_URL_LINK)
            {
                url = a_url;
                if (DecodeURL)
                    url = url_unquote_conv(url, Currentbuf->document_charset);
            }
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(prompt, url, IN_URL, hist);
        if (url != NULL)
            SKIP_BLANKS(url);
    }
#ifdef USE_M17N
    if (url != NULL)
    {
        if ((relative || *url == '#') && Currentbuf->document_charset)
            url = wc_conv_strict(url, InnerCharset,
                                 Currentbuf->document_charset)
                      ->ptr;
        else
            url = conv_to_system(url);
    }
#endif
    if (url == NULL || *url == '\0')
    {
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    }
    if (*url == '#')
    {
        gotoLabel(url + 1);
        return;
    }
    if (relative)
    {
        current = baseURL(Currentbuf);
        referer = parsedURL2Str(&Currentbuf->currentURL)->ptr;
    }
    else
    {
        current = NULL;
        referer = NULL;
    }
    parseURL2(url, &p_url, current);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, current, referer, NULL);
    if (Currentbuf != cur_buf) /* success */
        pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

void cmd_loadBuffer(Buffer *buf, int prop, int linkid)
{
    if (buf == NULL)
    {
        disp_err_message("Can't load string", FALSE);
    }
    else if (buf != NO_BUFFER)
    {
        buf->bufferprop |= (BP_INTERNAL | prop);
        if (!(buf->bufferprop & BP_NO_URL))
            copyParsedURL(&buf->currentURL, &Currentbuf->currentURL);
        if (linkid != LB_NOLINK)
        {
            buf->linkBuffer[REV_LB[linkid]] = Currentbuf;
            Currentbuf->linkBuffer[linkid] = buf;
        }
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void anchorMn(Anchor *(*menu_func)(Buffer *), int go)
{
    Anchor *a;
    BufferPoint *po;

    if (!Currentbuf->href || !Currentbuf->hmarklist)
        return;
    a = menu_func(Currentbuf);
    if (!a || a->hseq < 0)
        return;
    po = &Currentbuf->hmarklist->marks[a->hseq];
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
    if (go)
        followA();
}

int s_prev_key = -1;
int prev_key()
{
    return s_prev_key;
}
void set_prev_key(int key)
{
    s_prev_key = key;
}

void _peekURL(int only_img)
{

    Anchor *a;
    ParsedURL pu;
    static Str s = NULL;
#ifdef USE_M17N
    static Lineprop *p = NULL;
    Lineprop *pp;
#endif
    static int offset = 0, n;

    if (Currentbuf->firstLine == NULL)
        return;
    if (CurrentKey == prev_key && s != NULL)
    {
        if (s->length - offset >= COLS)
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
        goto disp;
    }
    else
    {
        offset = 0;
    }
    s = NULL;
    a = (only_img ? NULL : retrieveCurrentAnchor(Currentbuf));
    if (a == NULL)
    {
        a = (only_img ? NULL : retrieveCurrentForm(Currentbuf));
        if (a == NULL)
        {
            a = retrieveCurrentImg(Currentbuf);
            if (a == NULL)
                return;
        }
        else
            s = Strnew_charp(form2str((FormItemList *)a->url));
    }
    if (s == NULL)
    {
        parseURL2(a->url, &pu, baseURL(Currentbuf));
        s = parsedURL2Str(&pu);
    }
    if (DecodeURL)
        s = Strnew_charp(url_unquote_conv(s->ptr, Currentbuf->document_charset));
#ifdef USE_M17N
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->length);
    bcopy((void *)pp, (void *)p, s->length * sizeof(Lineprop));
#endif
disp:
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
#ifdef USE_M17N
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
#endif
    disp_message_nomouse(&s->ptr[offset], TRUE);
}

/* show current URL */
Str currentURL(void)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return parsedURL2Str(&Currentbuf->currentURL);
}

void repBuffer(Buffer *oldbuf, Buffer *buf)
{
    Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
    Currentbuf = buf;
}

void _docCSet(wc_ces charset)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (Currentbuf->sourcefile == NULL)
    {
        disp_message("Can't reload...", FALSE);
        return;
    }
    Currentbuf->document_charset = charset;
    Currentbuf->need_reshape = TRUE;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static int s_display_ok = FALSE;
int display_ok()
{
    return s_display_ok;
}

/* spawn external browser */
void invoke_browser(char *url)
{
    Str cmd;
    char *browser = NULL;
    int bg = 0, len;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    browser = searchKeyData();
    if (browser == NULL || *browser == '\0')
    {
        switch (prec_num())
        {
        case 0:
        case 1:
            browser = ExtBrowser;
            break;
        case 2:
            browser = ExtBrowser2;
            break;
        case 3:
            browser = ExtBrowser3;
            break;
        }
        if (browser == NULL || *browser == '\0')
        {
            browser = inputStr("Browse command: ", NULL);
            if (browser != NULL)
                browser = conv_to_system(browser);
        }
    }
    else
    {
        browser = conv_to_system(browser);
    }
    if (browser == NULL || *browser == '\0')
    {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }

    if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' &&
        browser[len - 2] != '\\')
    {
        browser = allocStr(browser, len - 2);
        bg = 1;
    }
    cmd = myExtCommand(browser, shell_quote(url), FALSE);
    Strremovetrailingspaces(cmd);
    fmTerm();
    mySystem(cmd->ptr, bg);
    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void process_mouse(int btn, int x, int y)
{
    int delta_x, delta_y, i;
    static int press_btn = MOUSE_BTN_RESET, press_x, press_y;
    TabBuffer *t;
    int ny = -1;

    if (nTab > 1 || mouse_action.menu_str)
        ny = LastTab->y + 1;
    if (btn == MOUSE_BTN_UP)
    {
        switch (press_btn)
        {
        case MOUSE_BTN1_DOWN:
            if (press_y == y && press_x == x)
                do_mouse_action(press_btn, x, y);
            else if (ny > 0 && y < ny)
            {
                if (press_y < ny)
                {
                    moveTab(posTab(press_x, press_y), posTab(x, y),
                            (press_y == y) ? (press_x < x) : (press_y < y));
                    return;
                }
                else if (press_x >= Currentbuf->rootX)
                {
                    Buffer *buf = Currentbuf;
                    int cx = Currentbuf->cursorX, cy = Currentbuf->cursorY;

                    t = posTab(x, y);
                    if (t == NULL)
                        return;
                    if (t == NO_TABBUFFER)
                        t = NULL; /* open new tab */
                    cursorXY(Currentbuf, press_x - Currentbuf->rootX,
                             press_y - Currentbuf->rootY);
                    if (Currentbuf->cursorY == press_y - Currentbuf->rootY &&
                        (Currentbuf->cursorX == press_x - Currentbuf->rootX
#ifdef USE_M17N
                         || (WcOption.use_wide &&
                             Currentbuf->currentLine != NULL &&
                             (CharType(Currentbuf->currentLine->propBuf[Currentbuf->pos]) == PC_KANJI1) && Currentbuf->cursorX == press_x - Currentbuf->rootX - 1)
#endif
                             ))
                    {
                        displayBuffer(Currentbuf, B_NORMAL);
                        followTab(t);
                    }
                    if (buf == Currentbuf)
                        cursorXY(Currentbuf, cx, cy);
                }
                return;
            }
            else
            {
                delta_x = x - press_x;
                delta_y = y - press_y;

                if (abs(delta_x) < abs(delta_y) / 3)
                    delta_x = 0;
                if (abs(delta_y) < abs(delta_x) / 3)
                    delta_y = 0;
                if (reverse_mouse)
                {
                    delta_y = -delta_y;
                    delta_x = -delta_x;
                }
                if (delta_y > 0)
                {
                    set_prec_num(delta_y);
                    ldown1();
                }
                else if (delta_y < 0)
                {
                    set_prec_num(-delta_y);
                    lup1();
                }
                if (delta_x > 0)
                {
                    set_prec_num(delta_x);
                    col1L();
                }
                else if (delta_x < 0)
                {
                    set_prec_num(-delta_x);
                    col1R();
                }
            }
            break;
        case MOUSE_BTN2_DOWN:
        case MOUSE_BTN3_DOWN:
            if (press_y == y && press_x == x)
                do_mouse_action(press_btn, x, y);
            break;
        case MOUSE_BTN4_DOWN_RXVT:
            for (i = 0; i < mouse_scroll_line(); i++)
                ldown1();
            break;
        case MOUSE_BTN5_DOWN_RXVT:
            for (i = 0; i < mouse_scroll_line(); i++)
                lup1();
            break;
        }
    }
    else if (btn == MOUSE_BTN4_DOWN_XTERM)
    {
        for (i = 0; i < mouse_scroll_line(); i++)
            ldown1();
    }
    else if (btn == MOUSE_BTN5_DOWN_XTERM)
    {
        for (i = 0; i < mouse_scroll_line(); i++)
            lup1();
    }

    if (btn != MOUSE_BTN4_DOWN_RXVT || press_btn == MOUSE_BTN_RESET)
    {
        press_btn = btn;
        press_x = x;
        press_y = y;
    }
    else
    {
        press_btn = MOUSE_BTN_RESET;
    }
}

void do_mouse_action(int btn, int x, int y)
{
    MouseActionMap *map = NULL;
    int ny = -1;

    if (nTab > 1 || mouse_action.menu_str)
        ny = LastTab->y + 1;

    switch (btn)
    {
    case MOUSE_BTN1_DOWN:
        btn = 0;
        break;
    case MOUSE_BTN2_DOWN:
        btn = 1;
        break;
    case MOUSE_BTN3_DOWN:
        btn = 2;
        break;
    default:
        return;
    }
    if (y < ny)
    {
        if (mouse_action.menu_str && x >= 0 && x < mouse_action.menu_width)
        {
            if (mouse_action.menu_map[btn])
                map = &mouse_action.menu_map[btn][x];
        }
        else
            map = &mouse_action.tab_map[btn];
    }
    else if (y == LASTLINE)
    {
        if (mouse_action.lastline_str && x >= 0 &&
            x < mouse_action.lastline_width)
        {
            if (mouse_action.lastline_map[btn])
                map = &mouse_action.lastline_map[btn][x];
        }
    }
    else if (y > ny)
    {
        if (y == Currentbuf->cursorY + Currentbuf->rootY &&
            (x == Currentbuf->cursorX + Currentbuf->rootX
#ifdef USE_M17N
             || (WcOption.use_wide && Currentbuf->currentLine != NULL &&
                 (CharType(Currentbuf->currentLine->propBuf[Currentbuf->pos]) == PC_KANJI1) && x == Currentbuf->cursorX + Currentbuf->rootX + 1)
#endif
                 ))
        {
            if (retrieveCurrentAnchor(Currentbuf) ||
                retrieveCurrentForm(Currentbuf))
            {
                map = &mouse_action.active_map[btn];
                if (!(map && map->func))
                    map = &mouse_action.anchor_map[btn];
            }
        }
        else
        {
            int cx = Currentbuf->cursorX, cy = Currentbuf->cursorY;
            cursorXY(Currentbuf, x - Currentbuf->rootX, y - Currentbuf->rootY);
            if (y == Currentbuf->cursorY + Currentbuf->rootY &&
                (x == Currentbuf->cursorX + Currentbuf->rootX
#ifdef USE_M17N
                 || (WcOption.use_wide && Currentbuf->currentLine != NULL &&
                     (CharType(Currentbuf->currentLine->propBuf[Currentbuf->pos]) == PC_KANJI1) && x == Currentbuf->cursorX + Currentbuf->rootX + 1)
#endif
                     ) &&
                (retrieveCurrentAnchor(Currentbuf) ||
                 retrieveCurrentForm(Currentbuf)))
                map = &mouse_action.anchor_map[btn];
            cursorXY(Currentbuf, cx, cy);
        }
    }
    else
    {
        return;
    }
    if (!(map && map->func))
        map = &mouse_action.default_map[btn];
    if (map && map->func)
    {
        mouse_action.in_action = TRUE;
        mouse_action.cursorX = x;
        mouse_action.cursorY = y;
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = map->data;
        (*map->func)();
        CurrentCmdData = NULL;
    }
}

void moveTab(TabBuffer *t, TabBuffer *t2, int right)
{
    if (t2 == NO_TABBUFFER)
        t2 = FirstTab;
    if (!t || !t2 || t == t2 || t == NO_TABBUFFER)
        return;
    if (t->prevTab)
    {
        if (t->nextTab)
            t->nextTab->prevTab = t->prevTab;
        else
            LastTab = t->prevTab;
        t->prevTab->nextTab = t->nextTab;
    }
    else
    {
        t->nextTab->prevTab = NULL;
        FirstTab = t->nextTab;
    }
    if (right)
    {
        t->nextTab = t2->nextTab;
        t->prevTab = t2;
        if (t2->nextTab)
            t2->nextTab->prevTab = t;
        else
            LastTab = t;
        t2->nextTab = t;
    }
    else
    {
        t->prevTab = t2->prevTab;
        t->nextTab = t2;
        if (t2->prevTab)
            t2->prevTab->nextTab = t;
        else
            FirstTab = t;
        t2->prevTab = t;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

TabBuffer *posTab(int x, int y)
{
    TabBuffer *tab;

    if (mouse_action.menu_str && x < mouse_action.menu_width && y == 0)
        return NO_TABBUFFER;
    if (y > LastTab->y)
        return NULL;
    for (tab = FirstTab; tab; tab = tab->nextTab)
    {
        if (tab->x1 <= x && x <= tab->x2 && tab->y == y)
            return tab;
    }
    return NULL;
}

void followTab(TabBuffer *tab)
{
    Buffer *buf;
    Anchor *a;

#ifdef USE_IMAGE
    a = retrieveCurrentImg(Currentbuf);
    if (!(a && a->image && a->image->map))
#endif
        a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL)
        return;

    if (tab == CurrentTab)
    {
        set_check_target(FALSE);
        followA();
        set_check_target(TRUE);
        return;
    }
    _newT();
    buf = Currentbuf;
    set_check_target(FALSE);
    followA();
    set_check_target(TRUE);
    if (tab == NULL)
    {
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab);
    }
    else if (buf != Currentbuf)
    {
        /* buf <- p <- ... <- Currentbuf = c */
        Buffer *c, *p;

        c = Currentbuf;
        p = prevBuffer(c, buf);
        p->nextBuffer = NULL;
        Firstbuf = buf;
        deleteTab(CurrentTab);
        CurrentTab = tab;
        for (buf = p; buf; buf = p)
        {
            p = prevBuffer(c, buf);
            pushBuffer(buf);
        }
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

int mouse_scroll_line()
{
    if (relative_wheel_scroll)
        return (relative_wheel_scroll_ratio * LASTLINE + 99) / 100;
    else
        return fixed_wheel_scroll_count;
}

void execdict(char *word)
{
    char *w, *dictcmd;
    Buffer *buf;

    if (!UseDictCommand || word == NULL || *word == '\0')
    {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    w = conv_to_system(word);
    if (*w == '\0')
    {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    dictcmd = Sprintf("%s?%s", DictCommand,
                      Str_form_quote(Strnew_charp(w))->ptr)
                  ->ptr;
    buf = loadGeneralFile(dictcmd, NULL, NO_REFERER, 0, NULL);
    if (buf == NULL)
    {
        disp_message("Execution failed", TRUE);
        return;
    }
    else
    {
        buf->filename = w;
        buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

char *GetWord(Buffer *buf)
{
    int b, e;
    char *p;

    if ((p = getCurWord(buf, &b, &e)) != NULL)
    {
        return Strnew_charp_n(p, e - b)->ptr;
    }
    return NULL;
}

#ifdef USE_ALARM
static AlarmEvent s_DefaultAlarm = {
    0, AL_UNSET, FUNCNAME_nulcmd, NULL};
AlarmEvent *DefaultAlarm()
{
    return &s_DefaultAlarm;
}
static AlarmEvent *s_CurrentAlarm = &s_DefaultAlarm;
AlarmEvent *CurrentAlarm()
{
    return s_CurrentAlarm;
}
void SetCurrentAlarm(AlarmEvent *alarm)
{
    s_CurrentAlarm = alarm;
}
// static MySignalHandler SigAlarm(SIGNAL_ARG);
MySignalHandler SigAlarm(SIGNAL_ARG)
{
    char *data;

    if (CurrentAlarm()->sec > 0)
    {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = data = (char *)CurrentAlarm()->data;
#ifdef USE_MOUSE
        if (use_mouse)
            mouse_inactive();
#endif
        w3mFuncList[CurrentAlarm()->cmd].func();
#ifdef USE_MOUSE
        if (use_mouse)
            mouse_active();
#endif
        CurrentCmdData = NULL;
        if (CurrentAlarm()->status == AL_IMPLICIT_ONCE)
        {
            CurrentAlarm()->sec = 0;
            CurrentAlarm()->status = AL_UNSET;
        }
        if (Currentbuf->event)
        {
            if (Currentbuf->event->status != AL_UNSET)
                SetCurrentAlarm(Currentbuf->event);
            else
                Currentbuf->event = NULL;
        }
        if (!Currentbuf->event)
            SetCurrentAlarm(DefaultAlarm());
        if (CurrentAlarm()->sec > 0)
        {
            mySignal(SIGALRM, SigAlarm);
            alarm(CurrentAlarm()->sec);
        }
    }
    SIGNAL_RETURN;
}

#endif

TabBuffer *numTab(int n)
{
    TabBuffer *tab;
    int i;

    if (n == 0)
        return CurrentTab;
    if (n == 1)
        return FirstTab;
    if (nTab <= 1)
        return NULL;
    for (tab = FirstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
        ;
    return tab;
}

void tabURL0(TabBuffer *tab, char *prompt, int relative)
{
    Buffer *buf;

    if (tab == CurrentTab)
    {
        goURL0(prompt, relative);
        return;
    }
    _newT();
    buf = Currentbuf;
    goURL0(prompt, relative);
    if (tab == NULL)
    {
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab);
    }
    else if (buf != Currentbuf)
    {
        /* buf <- p <- ... <- Currentbuf = c */
        Buffer *c, *p;

        c = Currentbuf;
        p = prevBuffer(c, buf);
        p->nextBuffer = NULL;
        Firstbuf = buf;
        deleteTab(CurrentTab);
        CurrentTab = tab;
        for (buf = p; buf; buf = p)
        {
            p = prevBuffer(c, buf);
            pushBuffer(buf);
        }
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

Buffer *DownloadListBuffer()
{
    DownloadList *d;
    Str src = NULL;
    struct stat st;
    time_t cur_time;
    int duration, rate, eta;
    size_t size;

    if (!FirstDL)
        return NULL;
    cur_time = time(0);
    /* FIXME: gettextize? */
    src = Strnew_charp("<html><head><title>" DOWNLOAD_LIST_TITLE
                       "</title></head>\n<body><h1 align=center>" DOWNLOAD_LIST_TITLE "</h1>\n"
                       "<form method=internal action=download><hr>\n");
    for (d = LastDL; d != NULL; d = d->prev)
    {
        if (lstat(d->lock, &st))
            d->running = FALSE;
        Strcat_charp(src, "<pre>\n");
        Strcat(src, Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url),
                            html_quote(conv_from_system(d->save))));
        duration = cur_time - d->time;
        if (!stat(d->save, &st))
        {
            size = st.st_size;
            if (!d->running)
            {
                if (!d->err)
                    d->size = size;
                duration = st.st_mtime - d->time;
            }
        }
        else
            size = 0;
        if (d->size)
        {
            int i, l = COLS - 6;
            if (size < d->size)
                i = 1.0 * l * size / d->size;
            else
                i = l;
            l -= i;
            while (i-- > 0)
                Strcat_char(src, '#');
            while (l-- > 0)
                Strcat_char(src, '_');
            Strcat_char(src, '\n');
        }
        if ((d->running || d->err) && size < d->size)
            Strcat(src, Sprintf("  %s / %s bytes (%d%%)",
                                convert_size3(size), convert_size3(d->size),
                                (int)(100.0 * size / d->size)));
        else
            Strcat(src, Sprintf("  %s bytes loaded", convert_size3(size)));
        if (duration > 0)
        {
            rate = size / duration;
            Strcat(src, Sprintf("  %02d:%02d:%02d  rate %s/sec",
                                duration / (60 * 60), (duration / 60) % 60,
                                duration % 60, convert_size(rate, 1)));
            if (d->running && size < d->size && rate)
            {
                eta = (d->size - size) / rate;
                Strcat(src, Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60),
                                    (eta / 60) % 60, eta % 60));
            }
        }
        Strcat_char(src, '\n');
        if (!d->running)
        {
            Strcat(src, Sprintf("<input type=submit name=ok%d value=OK>",
                                d->pid));
            switch (d->err)
            {
            case 0:
                if (size < d->size)
                    Strcat_charp(src, " Download ended but probably not complete");
                else
                    Strcat_charp(src, " Download complete");
                break;
            case 1:
                Strcat_charp(src, " Error: could not open destination file");
                break;
            case 2:
                Strcat_charp(src, " Error: could not write to file (disk full)");
                break;
            default:
                Strcat_charp(src, " Error: unknown reason");
            }
        }
        else
            Strcat(src, Sprintf("<input type=submit name=stop%d value=STOP>",
                                d->pid));
        Strcat_charp(src, "\n</pre><hr>\n");
    }
    Strcat_charp(src, "</form></body></html>");
    return loadHTMLString(src);
}

char *convert_size3(clen_t size)
{
    Str tmp = Strnew();
    int n;

    do
    {
        n = size % 1000;
        size /= 1000;
        tmp = Sprintf((char *)(size ? ",%.3d%s" : "%d%s"), n, tmp->ptr);
    } while (size);
    return tmp->ptr;
}

void resetPos(BufferPos *b)
{
    Buffer buf;
    Line top, cur;

    top.linenumber = b->top_linenumber;
    cur.linenumber = b->cur_linenumber;
    cur.bpos = b->bpos;
    buf.topLine = &top;
    buf.currentLine = &cur;
    buf.pos = b->pos;
    buf.currentColumn = b->currentColumn;
    restorePosition(Currentbuf, &buf);
    Currentbuf->undo = b;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void save_buffer_position(Buffer *buf)
{
    BufferPos *b = buf->undo;

    if (!buf->firstLine)
        return;
    if (b && b->top_linenumber == TOP_LINENUMBER(buf) &&
        b->cur_linenumber == CUR_LINENUMBER(buf) &&
        b->currentColumn == buf->currentColumn && b->pos == buf->pos)
        return;
    b = New(BufferPos);
    b->top_linenumber = TOP_LINENUMBER(buf);
    b->cur_linenumber = CUR_LINENUMBER(buf);
    b->currentColumn = buf->currentColumn;
    b->pos = buf->pos;
    b->bpos = buf->currentLine ? buf->currentLine->bpos : 0;
    b->next = NULL;
    b->prev = buf->undo;
    if (buf->undo)
        buf->undo->next = b;
    buf->undo = b;
}

void stopDownload()
{
    DownloadList *d;

    if (!FirstDL)
        return;
    for (d = FirstDL; d != NULL; d = d->next)
    {
        if (!d->running)
            continue;
#ifndef __MINGW32_VERSION
        kill(d->pid, SIGKILL);
#endif
        unlink(d->lock);
    }
}

void download_action(struct parsed_tagarg *arg)
{
    DownloadList *d;
    pid_t pid;

    for (; arg; arg = arg->next)
    {
        if (!strncmp(arg->arg, "stop", 4))
        {
            pid = (pid_t)atoi(&arg->arg[4]);
#ifndef __MINGW32_VERSION
            kill(pid, SIGKILL);
#endif
        }
        else if (!strncmp(arg->arg, "ok", 2))
            pid = (pid_t)atoi(&arg->arg[2]);
        else
            continue;
        for (d = FirstDL; d; d = d->next)
        {
            if (d->pid == pid)
            {
                unlink(d->lock);
                if (d->prev)
                    d->prev->next = d->next;
                else
                    FirstDL = d->next;
                if (d->next)
                    d->next->prev = d->prev;
                else
                    LastDL = d->prev;
                break;
            }
        }
    }
    ldDL();
}

int checkDownloadList()
{
    DownloadList *d;
    struct stat st;

    if (!FirstDL)
        return FALSE;
    for (d = FirstDL; d != NULL; d = d->next)
    {
        if (d->running && !lstat(d->lock, &st))
            return TRUE;
    }
    return FALSE;
}

static int s_add_download_list = FALSE;
int add_download_list()
{
    return s_add_download_list;
}
void set_add_download_list(int add)
{
    s_add_download_list = add;
}

void addDownloadList(pid_t pid, char *url, char *save, char *lock, clen_t size)
{
    DownloadList *d;

    d = New(DownloadList);
    d->pid = pid;
    d->url = url;
    if (save[0] != '/' && save[0] != '~')
        save = Strnew_m_charp(CurrentDir, "/", save, NULL)->ptr;
    d->save = expandPath(save);
    d->lock = lock;
    d->size = size;
    d->time = time(0);
    d->running = TRUE;
    d->err = 0;
    d->next = NULL;
    d->prev = LastDL;
    if (LastDL)
        LastDL->next = d;
    else
        FirstDL = d;
    LastDL = d;
    set_add_download_list(TRUE);
}

TabBuffer *deleteTab(TabBuffer *tab)
{
    Buffer *buf, *next;

    if (nTab <= 1)
        return FirstTab;
    if (tab->prevTab)
    {
        if (tab->nextTab)
            tab->nextTab->prevTab = tab->prevTab;
        else
            LastTab = tab->prevTab;
        tab->prevTab->nextTab = tab->nextTab;
        if (tab == CurrentTab)
            CurrentTab = tab->prevTab;
    }
    else
    { /* tab == FirstTab */
        tab->nextTab->prevTab = NULL;
        FirstTab = tab->nextTab;
        if (tab == CurrentTab)
            CurrentTab = tab->nextTab;
    }
    nTab--;
    buf = tab->firstBuffer;
    while (buf && buf != NO_BUFFER)
    {
        next = buf->nextBuffer;
        discardBuffer(buf);
        buf = next;
    }
    return FirstTab;
}

void calcTabPos()
{
    TabBuffer *tab;
#if 0
    int lcol = 0, rcol = 2, col;
#else
    int lcol = 0, rcol = 0, col;
#endif
    int n1, n2, na, nx, ny, ix, iy;

#ifdef USE_MOUSE
    lcol = mouse_action.menu_str ? mouse_action.menu_width : 0;
#endif

    if (nTab <= 0)
        return;
    n1 = (COLS - rcol - lcol) / TabCols;
    if (n1 >= nTab)
    {
        n2 = 1;
        ny = 1;
    }
    else
    {
        if (n1 < 0)
            n1 = 0;
        n2 = COLS / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = FirstTab;
    for (iy = 0; iy < ny && tab; iy++)
    {
        if (iy == 0)
        {
            nx = n1;
            col = COLS - rcol - lcol;
        }
        else
        {
            nx = n2 - (na - nTab + (iy - 1)) / (ny - 1);
            col = COLS;
        }
        for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab)
        {
            tab->x1 = col * ix / nx;
            tab->x2 = col * (ix + 1) / nx - 1;
            tab->y = iy;
            if (iy == 0)
            {
                tab->x1 += lcol;
                tab->x2 += lcol;
            }
        }
    }
}

TabBuffer *newTab(void)
{
    TabBuffer *n;

    n = New(TabBuffer);
    if (n == NULL)
        return NULL;
    n->nextTab = NULL;
    n->currentBuffer = NULL;
    n->firstBuffer = NULL;
    return n;
}

AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status, int cmd, void *data)
{
    if (event == NULL)
        event = New(AlarmEvent);
    event->sec = sec;
    event->status = status;
    event->cmd = cmd;
    event->data = data;
    return event;
}

void w3m_exit(int i)
{
#ifdef USE_MIGEMO
    init_migemo(); /* close pipe to migemo */
#endif
    stopDownload();
    deleteFiles();
#ifdef USE_SSL
    free_ssl_ctx();
#endif
    disconnectFTP();
#ifdef USE_NNTP
    disconnectNews();
#endif
#ifdef __MINGW32_VERSION
    WSACleanup();
#endif
    exit(i);
}

void deleteFiles()
{
    Buffer *buf;
    char *f;

    for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab)
    {
        while (Firstbuf && Firstbuf != NO_BUFFER)
        {
            buf = Firstbuf->nextBuffer;
            discardBuffer(Firstbuf);
            Firstbuf = buf;
        }
    }
    while ((f = popText(fileToDelete)) != NULL)
        unlink(f);
}

char *searchKeyData()
{
    char *data = NULL;

    if (CurrentKeyData != NULL && *CurrentKeyData != '\0')
        data = CurrentKeyData;
    else if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
        data = CurrentCmdData;
    else if (CurrentKey >= 0)
        data = getKeyData(CurrentKey);
    CurrentKeyData = NULL;
    CurrentCmdData = NULL;
    if (data == NULL || *data == '\0')
        return NULL;
    return allocStr(data, -1);
}

void set_buffer_environ(Buffer *buf)
{
    static Buffer *prev_buf = NULL;
    static Line *prev_line = NULL;
    static int prev_pos = -1;
    Line *l;

    if (buf == NULL)
        return;
    if (buf != prev_buf)
    {
        set_environ("W3M_SOURCEFILE", buf->sourcefile);
        set_environ("W3M_FILENAME", buf->filename);
        set_environ("W3M_TITLE", buf->buffername);
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

int sysm_process_mouse(int x, int y, int nbs, int obs)
{
    int btn;
    int bits;

    if (obs & ~nbs)
        btn = MOUSE_BTN_UP;
    else if (nbs & ~obs)
    {
        bits = nbs & ~obs;
        btn = bits & 0x1 ? MOUSE_BTN1_DOWN : (bits & 0x2 ? MOUSE_BTN2_DOWN : (bits & 0x4 ? MOUSE_BTN3_DOWN : 0));
    }
    else /* nbs == obs */
        return 0;
    process_mouse(btn, x, y);
    return 0;
}

// int gpm_process_mouse(Gpm_Event *event, void *data)
// {
//     int btn = MOUSE_BTN_RESET, x, y;
//     if (event->type & GPM_UP)
//         btn = MOUSE_BTN_UP;
//     else if (event->type & GPM_DOWN)
//     {
//         switch (event->buttons)
//         {
//         case GPM_B_LEFT:
//             btn = MOUSE_BTN1_DOWN;
//             break;
//         case GPM_B_MIDDLE:
//             btn = MOUSE_BTN2_DOWN;
//             break;
//         case GPM_B_RIGHT:
//             btn = MOUSE_BTN3_DOWN;
//             break;
//         }
//     }
//     else
//     {
//         GPM_DRAWPOINTER(event);
//         return 0;
//     }
//     x = event->x;
//     y = event->y;
//     process_mouse(btn, x - 1, y - 1);
//     return 0;
// }

/* mark Message-ID-like patterns as NEWS anchors */
void chkNMIDBuffer(Buffer *buf)
{
    static char *url_like_pat[] = {
        "<[!-;=?-~]+@[a-zA-Z0-9\\.\\-_]+>",
        NULL,
    };
    int i;
    for (i = 0; url_like_pat[i]; i++)
    {
        reAnchorNews(buf, url_like_pat[i]);
    }
    buf->check_url |= CHK_NMID;
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(Buffer *buf)
{
    static char *url_like_pat[] = {
        "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
        "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
#ifdef USE_GOPHER
        "gopher://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
#endif /* USE_GOPHER */
        "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifdef USE_NNTP
        "news:[^<> 	][^<> 	]*",
        "nntp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
#endif                /* USE_NNTP */
#ifndef USE_W3MMAILER /* see also chkExternalURIBuffer() */
        "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
#ifdef INET6
        "https?://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*",
        "ftp://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
#endif /* INET6 */
        NULL};
    int i;
    for (i = 0; url_like_pat[i]; i++)
    {
        reAnchor(buf, url_like_pat[i]);
    }
#ifdef USE_EXTERNAL_URI_LOADER
    chkExternalURIBuffer(buf);
#endif
    buf->check_url |= CHK_URL;
}

void change_charset(struct parsed_tagarg *arg)
{
    Buffer *buf = Currentbuf->linkBuffer[LB_N_INFO];
    wc_ces charset;

    if (buf == NULL)
        return;
    delBuffer(Currentbuf);
    Currentbuf = buf;
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    charset = Currentbuf->document_charset;
    for (; arg; arg = arg->next)
    {
        if (!strcmp(arg->arg, "charset"))
            charset = atoi(arg->value);
    }
    _docCSet(charset);
}

void follow_map(struct parsed_tagarg *arg)
{
    char *name = tag_get_value(arg, "link");
#if defined(MENU_MAP) || defined(USE_IMAGE)
    Anchor *an;
    MapArea *a;
    int x, y;
    ParsedURL p_url;

    an = retrieveCurrentImg(Currentbuf);
    x = Currentbuf->cursorX + Currentbuf->rootX;
    y = Currentbuf->cursorY + Currentbuf->rootY;
    a = follow_map_menu(Currentbuf, name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0')
    {
#endif
#ifndef MENU_MAP
        Buffer *buf = follow_map_panel(Currentbuf, name);

        if (buf != NULL)
            cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
#endif
#if defined(MENU_MAP) || defined(USE_IMAGE)
        return;
    }
    if (*(a->url) == '#')
    {
        gotoLabel(a->url + 1);
        return;
    }
    parseURL2(a->url, &p_url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    if (check_target && open_tab_blank && a->target &&
        (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank")))
    {
        Buffer *buf;

        _newT();
        buf = Currentbuf;
        cmd_loadURL(a->url, baseURL(Currentbuf),
                    parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab);
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    }
    cmd_loadURL(a->url, baseURL(Currentbuf),
                parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
#endif
}

/* process form */
void followForm(void)
{
    _followForm(FALSE);
}

MySignalHandler SigPipe(SIGNAL_ARG)
{
#ifdef USE_MIGEMO
    init_migemo();
#endif
    mySignal(SIGPIPE, SigPipe);
    SIGNAL_RETURN;
}

static int s_need_resize_screen = FALSE;
int need_resize_screen()
{
    return s_need_resize_screen;
}
void set_need_resize_screen(int need)
{
    s_need_resize_screen = need;
}

MySignalHandler
    resize_hook(SIGNAL_ARG)
{
    s_need_resize_screen = TRUE;
    mySignal(SIGWINCH, resize_hook);
    SIGNAL_RETURN;
}

void resize_screen()
{
    s_need_resize_screen = FALSE;
    setlinescols();
    setupscreen();
    if (CurrentTab)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void saveBufferInfo()
{
    FILE *fp;

    if (w3m_dump)
        return;
    if ((fp = fopen(rcFile("bufinfo"), "w")) == NULL)
    {
        return;
    }
    fprintf(fp, "%s\n", currentURL()->ptr);
    fclose(fp);
}

void tmpClearBuffer(Buffer *buf)
{
    if (buf->pagerSource == NULL && writeBufferCache(buf) == 0)
    {
        buf->firstLine = NULL;
        buf->topLine = NULL;
        buf->currentLine = NULL;
        buf->lastLine = NULL;
    }
}

void escdmap(char c)
{
    int d;
    d = (int)c - (int)'0';
    c = getch();
    if (IS_DIGIT(c))
    {
        d = d * 10 + (int)c - (int)'0';
        c = getch();
    }
    if (c == '~')
        escKeyProc((int)d, K_ESCD, EscDKeymap);
}

typedef struct _Event
{
    int cmd;
    void *data;
    struct _Event *next;
} Event;
static Event *CurrentEvent = NULL;
static Event *LastEvent = NULL;

void pushEvent(int cmd, void *data)
{
    Event *event;

    event = New(Event);
    event->cmd = cmd;
    event->data = data;
    event->next = NULL;
    if (CurrentEvent)
        LastEvent->next = event;
    else
        CurrentEvent = event;
    LastEvent = event;
}

int ProcessEvent()
{
    if (CurrentEvent)
    {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = (char *)CurrentEvent->data;
        w3mFuncList[CurrentEvent->cmd].func();
        CurrentCmdData = NULL;
        CurrentEvent = CurrentEvent->next;
        return 1;
    }
    return 0;
}

void keyPressEventProc(int c)
{
    CurrentKey = c;
    w3mFuncList[(int)GlobalKeymap[c]].func();
}
