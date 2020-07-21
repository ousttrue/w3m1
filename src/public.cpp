
#include "fm.h"
#include "indep.h"
#include "rc.h"
#include "etc.h"
#include "frame.h"
#include "display.h"
#include "parsetag.h"
#include "public.h"
#include "file.h"
#include <setjmp.h>
#include <signal.h>
#include "ucs.h"
#include "terms.h"
#include "dispatcher.h"
#include "image.h"
#include "commands.h"
#include "url.h"
#include "cookie.h"
#include "ctrlcode.h"
#include "mouse.h"

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
    Buffer *buf = GetCurrentbuf();
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
        GetCurrentbuf()->pos += 1;
    result = srchcore(SearchString, routine[reverse]);
    if (result & SR_FOUND)
        clear_mark(GetCurrentbuf()->currentLine);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
    disp_srchresult(result, (char *)(reverse ? "Backward: " : "Forward: "),
                    SearchString);
}

#ifndef __MINGW32_VERSION
JMP_BUF IntReturn;
#else
_JBTYPE IntReturn[_JBLEN];
#endif /* __MINGW32_VERSION */

void
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
    auto prevtrap = mySignal(SIGINT, intTrap);
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
                    s = Strnew_charp(url_unquote_conv(s->ptr, GetCurrentbuf()->document_charset));
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
    MySignalHandler prevtrap = mySignal(SIGINT, intTrap);
    crmode();
    if (SETJMP(IntReturn) == 0)
    {
        for (i = 0; i < PREC_NUM(); i++)
        {
            result = func(GetCurrentbuf(), str);
            if (i < PREC_NUM() - 1 && result & SR_FOUND)
                clear_mark(GetCurrentbuf()->currentLine);
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
                GetCurrentbuf()->pos += 1;
            SAVE_BUFPOSITION(&sbuf);
            if (srchcore(str, searchRoutine) == SR_NOTFOUND && searchRoutine == forwardSearch)
            {
                GetCurrentbuf()->pos -= 1;
                SAVE_BUFPOSITION(&sbuf);
            }
            arrangeCursor(GetCurrentbuf());
            displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
            clear_mark(GetCurrentbuf()->currentLine);
            return -1;
        }
        else
            return 020; /* _prev completion for C-s C-s */
    }
    else if (*str)
    {
        RESTORE_BUFPOSITION(&sbuf);
        arrangeCursor(GetCurrentbuf());
        srchcore(str, searchRoutine);
        arrangeCursor(GetCurrentbuf());
        currentLine = GetCurrentbuf()->currentLine;
        pos = GetCurrentbuf()->pos;
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
    clear_mark(GetCurrentbuf()->currentLine);
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
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
        disp = TRUE;
    }
    pos = GetCurrentbuf()->pos;
    if (func == forwardSearch)
        GetCurrentbuf()->pos += 1;
    result = srchcore(str, func);
    if (result & SR_FOUND)
        clear_mark(GetCurrentbuf()->currentLine);
    else
        GetCurrentbuf()->pos = pos;
    displayBuffer(GetCurrentbuf(), B_NORMAL);
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
        GetCurrentTab()->BufferPushFront(buf);
        if (RenderFrame && GetCurrentbuf()->frameset != NULL)
            rFrame();
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
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
        GetCurrentTab()->BufferPushFront(buf);
        if (RenderFrame && GetCurrentbuf()->frameset != NULL)
            rFrame();
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
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
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
    pushHashHist(URLHist, url);
    return 1;
}

/* Move cursor left */
void _movL(int n)
{
    int i, m = searchKeyNum();
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorLeft(GetCurrentbuf(), n);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

/* Move cursor downward */
void _movD(int n)
{
    int i, m = searchKeyNum();
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorDown(GetCurrentbuf(), n);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

/* move cursor upward */
void _movU(int n)
{
    int i, m = searchKeyNum();
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorUp(GetCurrentbuf(), n);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

/* Move cursor right */
void _movR(int n)
{
    int i, m = searchKeyNum();
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorRight(GetCurrentbuf(), n);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

int prev_nonnull_line(Line *line)
{
    Line *l;

    for (l = line; l != NULL && l->len == 0; l = l->prev)
        ;
    if (l == NULL || l->len == 0)
        return -1;

    GetCurrentbuf()->currentLine = l;
    if (l != line)
        GetCurrentbuf()->pos = GetCurrentbuf()->currentLine->len;
    return 0;
}

int next_nonnull_line(Line *line)
{
    Line *l;

    for (l = line; l != NULL && l->len == 0; l = l->next)
        ;

    if (l == NULL || l->len == 0)
        return -1;

    GetCurrentbuf()->currentLine = l;
    if (l != line)
        GetCurrentbuf()->pos = 0;
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
        displayBuffer(GetCurrentbuf(), B_NORMAL);
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
    if (GetCurrentbuf() == buf)
        SetCurrentbuf(buf->nextBuffer);
    SetFirstbuf(deleteBuffer(GetFirstbuf(), buf));
    if (!GetCurrentbuf())
        SetCurrentbuf(GetFirstbuf());
}

/* Go to specified line */
void _goLine(char *l)
{
    if (l == NULL || *l == '\0' || GetCurrentbuf()->currentLine == NULL)
    {
        displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
        return;
    }
    GetCurrentbuf()->pos = 0;
    if (((*l == '^') || (*l == '$')) && prec_num())
    {
        gotoRealLine(GetCurrentbuf(), prec_num());
    }
    else if (*l == '^')
    {
        GetCurrentbuf()->topLine = GetCurrentbuf()->currentLine = GetCurrentbuf()->firstLine;
    }
    else if (*l == '$')
    {
        GetCurrentbuf()->topLine =
            lineSkip(GetCurrentbuf(), GetCurrentbuf()->lastLine,
                     -(GetCurrentbuf()->LINES + 1) / 2, TRUE);
        GetCurrentbuf()->currentLine = GetCurrentbuf()->lastLine;
    }
    else
        gotoRealLine(GetCurrentbuf(), atoi(l));
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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

    if (GetCurrentbuf()->firstLine == NULL)
        return;
    l = GetCurrentbuf()->currentLine;

    a = retrieveCurrentForm(GetCurrentbuf());
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
        formUpdateBuffer(a, GetCurrentbuf(), fi);
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
        formUpdateBuffer(a, GetCurrentbuf(), fi);
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
        formUpdateBuffer(a, GetCurrentbuf(), fi);
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
        formUpdateBuffer(a, GetCurrentbuf(), fi);
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
        formRecheckRadio(a, GetCurrentbuf(), fi);
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
        formUpdateBuffer(a, GetCurrentbuf(), fi);
        break;
#ifdef MENU_SELECT
    case FORM_SELECT:
        if (submit)
            goto do_submit;
        if (!formChooseOptionByMenu(fi,
                                    GetCurrentbuf()->cursorX - GetCurrentbuf()->pos +
                                        a->start.pos + GetCurrentbuf()->rootX,
                                    GetCurrentbuf()->cursorY + GetCurrentbuf()->rootY))
            break;
        formUpdateBuffer(a, GetCurrentbuf(), fi);
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
            tmp2 = parsedURL2Str(&GetCurrentbuf()->currentURL);
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
        else if ((fi->parent->method == FORM_METHOD_INTERNAL && (!Strcmp_charp(fi->parent->action, "map") || !Strcmp_charp(fi->parent->action, "none"))) || GetCurrentbuf()->bufferprop & BP_INTERNAL)
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
        for (i = 0; i < GetCurrentbuf()->formitem->nanchor; i++)
        {
            a2 = &GetCurrentbuf()->formitem->anchors[i];
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
                formUpdateBuffer(a2, GetCurrentbuf(), f2);
            }
        }
        break;
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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
                getMapXY(GetCurrentbuf(), retrieveCurrentImg(GetCurrentbuf()), &x, &y);
#endif
                *query = Strdup(conv_form_encoding(f2->name, fi, GetCurrentbuf()));
                Strcat_charp(*query, ".x");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                                Sprintf("%d", x)->ptr);
                *query = Strdup(conv_form_encoding(f2->name, fi, GetCurrentbuf()));
                Strcat_charp(*query, ".y");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                                Sprintf("%d", y)->ptr);
            }
            else if (f2->name && f2->name->length > 0 && f2->value != NULL)
            {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value, fi, GetCurrentbuf());
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                                         conv_form_encoding(f2->name, fi,
                                                            GetCurrentbuf())
                                             ->ptr,
                                         (*query)->ptr,
                                         Str_conv_to_system(f2->value)->ptr);
                else
                    form_write_data(body, fi->parent->boundary,
                                    conv_form_encoding(f2->name, fi,
                                                       GetCurrentbuf())
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
                getMapXY(GetCurrentbuf(), retrieveCurrentImg(GetCurrentbuf()), &x, &y);
#endif
                Strcat(*query,
                       Str_form_quote(conv_form_encoding(f2->name, fi, GetCurrentbuf())));
                Strcat(*query, Sprintf(".x=%d&", x));
                Strcat(*query,
                       Str_form_quote(conv_form_encoding(f2->name, fi, GetCurrentbuf())));
                Strcat(*query, Sprintf(".y=%d", y));
            }
            else
            {
                /* not IMAGE */
                if (f2->name && f2->name->length > 0)
                {
                    Strcat(*query,
                           Str_form_quote(conv_form_encoding(f2->name, fi, GetCurrentbuf())));
                    Strcat_char(*query, '=');
                }
                if (f2->value != NULL)
                {
                    if (fi->parent->method == FORM_METHOD_INTERNAL)
                        Strcat(*query, Str_form_quote(f2->value));
                    else
                    {
                        Strcat(*query,
                               Str_form_quote(conv_form_encoding(f2->value, fi, GetCurrentbuf())));
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

    base = baseURL(GetCurrentbuf());
    if (base == NULL ||
        base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2Str(&GetCurrentbuf()->currentURL)->ptr;
    buf = loadGeneralFile(url, baseURL(GetCurrentbuf()), referer, flag, request);
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

    if (target == NULL ||                         /* no target specified (that means this page is not a frame page) */
        !strcmp(target, "_top") ||                /* this link is specified to be opened as an indivisual * page */
        !(GetCurrentbuf()->bufferprop & BP_FRAME) /* This page is not a frame page */
    )
    {
        return loadNormalBuf(buf, TRUE);
    }
    nfbuf = GetCurrentbuf()->linkBuffer[LB_N_FRAME];
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
    pushFrameTree(&(nfbuf->frameQ), copyFrameSet(nfbuf->frameset), GetCurrentbuf());
    /* delete frame view buffer */
    delBuffer(GetCurrentbuf());
    SetCurrentbuf(nfbuf);
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
            al = searchURLLabel(GetCurrentbuf(), label);
        }
        if (al)
        {
            gotoLine(GetCurrentbuf(), al->start.line);
            if (label_topline)
                GetCurrentbuf()->topLine = lineSkip(GetCurrentbuf(), GetCurrentbuf()->topLine,
                                                    GetCurrentbuf()->currentLine->linenumber -
                                                        GetCurrentbuf()->topLine->linenumber,
                                                    FALSE);
            GetCurrentbuf()->pos = al->start.pos;
            arrangeCursor(GetCurrentbuf());
        }
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
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
    GetCurrentTab()->BufferPushFront(buf);
    if (renderframe && RenderFrame && GetCurrentbuf()->frameset != NULL)
        rFrame();
    return buf;
}

/* go to the next [visited] anchor */
void _nextA(int visited)
{
    HmarkerList *hl = GetCurrentbuf()->hmarklist;
    BufferPoint *po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    ParsedURL url;

    if (GetCurrentbuf()->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(GetCurrentbuf());
    if (visited != TRUE && an == NULL)
        an = retrieveCurrentForm(GetCurrentbuf());

    y = GetCurrentbuf()->currentLine->linenumber;
    x = GetCurrentbuf()->pos;

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
                an = retrieveAnchor(GetCurrentbuf()->href, po->line, po->pos);
                if (visited != TRUE && an == NULL)
                    an = retrieveAnchor(GetCurrentbuf()->formitem, po->line,
                                        po->pos);
                hseq++;
                if (visited == TRUE && an)
                {
                    parseURL2(an->url, &url, baseURL(GetCurrentbuf()));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = closest_next_anchor(GetCurrentbuf()->href, NULL, x, y);
            if (visited != TRUE)
                an = closest_next_anchor(GetCurrentbuf()->formitem, an, x, y);
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
                parseURL2(an->url, &url, baseURL(GetCurrentbuf()));
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
    gotoLine(GetCurrentbuf(), po->line);
    GetCurrentbuf()->pos = po->pos;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

/* go to the previous anchor */
void _prevA(int visited)
{
    HmarkerList *hl = GetCurrentbuf()->hmarklist;
    BufferPoint *po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    ParsedURL url;

    if (GetCurrentbuf()->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(GetCurrentbuf());
    if (visited != TRUE && an == NULL)
        an = retrieveCurrentForm(GetCurrentbuf());

    y = GetCurrentbuf()->currentLine->linenumber;
    x = GetCurrentbuf()->pos;

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
                an = retrieveAnchor(GetCurrentbuf()->href, po->line, po->pos);
                if (visited != TRUE && an == NULL)
                    an = retrieveAnchor(GetCurrentbuf()->formitem, po->line,
                                        po->pos);
                hseq--;
                if (visited == TRUE && an)
                {
                    parseURL2(an->url, &url, baseURL(GetCurrentbuf()));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = closest_prev_anchor(GetCurrentbuf()->href, NULL, x, y);
            if (visited != TRUE)
                an = closest_prev_anchor(GetCurrentbuf()->formitem, an, x, y);
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
                parseURL2(an->url, &url, baseURL(GetCurrentbuf()));
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
    gotoLine(GetCurrentbuf(), po->line);
    GetCurrentbuf()->pos = po->pos;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void gotoLabel(char *label)
{
    Buffer *buf;
    Anchor *al;
    int i;

    al = searchURLLabel(GetCurrentbuf(), label);
    if (al == NULL)
    {
        /* FIXME: gettextize? */
        disp_message(Sprintf("%s is not found", label)->ptr, TRUE);
        return;
    }
    buf = newBuffer(GetCurrentbuf()->width);
    copyBuffer(buf, GetCurrentbuf());
    for (i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    buf->currentURL.label = allocStr(label, -1);
    pushHashHist(URLHist, parsedURL2Str(&buf->currentURL)->ptr);
    (*buf->clone)++;
    GetCurrentTab()->BufferPushFront(buf);
    gotoLine(GetCurrentbuf(), al->start.line);
    if (label_topline)
        GetCurrentbuf()->topLine = lineSkip(GetCurrentbuf(), GetCurrentbuf()->topLine,
                                            GetCurrentbuf()->currentLine->linenumber - GetCurrentbuf()->topLine->linenumber,
                                            FALSE);
    GetCurrentbuf()->pos = al->start.pos;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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

/* go to the next left/right anchor */
void nextX(int d, int dy)
{
    HmarkerList *hl = GetCurrentbuf()->hmarklist;
    Anchor *an, *pan;
    Line *l;
    int i, x, y, n = searchKeyNum();

    if (GetCurrentbuf()->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(GetCurrentbuf());
    if (an == NULL)
        an = retrieveCurrentForm(GetCurrentbuf());

    l = GetCurrentbuf()->currentLine;
    x = GetCurrentbuf()->pos;
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
                an = retrieveAnchor(GetCurrentbuf()->href, y, x);
                if (!an)
                    an = retrieveAnchor(GetCurrentbuf()->formitem, y, x);
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
    gotoLine(GetCurrentbuf(), y);
    GetCurrentbuf()->pos = pan->start.pos;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

/* go to the next downward/upward anchor */
void nextY(int d)
{
    HmarkerList *hl = GetCurrentbuf()->hmarklist;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    int hseq;

    if (GetCurrentbuf()->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(GetCurrentbuf());
    if (an == NULL)
        an = retrieveCurrentForm(GetCurrentbuf());

    x = GetCurrentbuf()->pos;
    y = GetCurrentbuf()->currentLine->linenumber + d;
    pan = NULL;
    hseq = -1;
    for (i = 0; i < n; i++)
    {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= GetCurrentbuf()->lastLine->linenumber; y += d)
        {
            an = retrieveAnchor(GetCurrentbuf()->href, y, x);
            if (!an)
                an = retrieveAnchor(GetCurrentbuf()->formitem, y, x);
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
    gotoLine(GetCurrentbuf(), pan->start.line);
    arrangeLine(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
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
    Buffer *cur_buf = GetCurrentbuf();

    url = searchKeyData();
    if (url == NULL)
    {
        Hist *hist = copyHist(URLHist);
        Anchor *a;

        current = baseURL(GetCurrentbuf());
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
        a = retrieveCurrentAnchor(GetCurrentbuf());
        if (a)
        {
            char *a_url;
            parseURL2(a->url, &p_url, current);
            a_url = parsedURL2Str(&p_url)->ptr;
            if (DefaultURLString == DEFAULT_URL_LINK)
            {
                url = a_url;
                if (DecodeURL)
                    url = url_unquote_conv(url, GetCurrentbuf()->document_charset);
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
        if ((relative || *url == '#') && GetCurrentbuf()->document_charset)
            url = wc_conv_strict(url, InnerCharset,
                                 GetCurrentbuf()->document_charset)
                      ->ptr;
        else
            url = conv_to_system(url);
    }
#endif
    if (url == NULL || *url == '\0')
    {
        displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
        return;
    }
    if (*url == '#')
    {
        gotoLabel(url + 1);
        return;
    }
    if (relative)
    {
        current = baseURL(GetCurrentbuf());
        referer = parsedURL2Str(&GetCurrentbuf()->currentURL)->ptr;
    }
    else
    {
        current = NULL;
        referer = NULL;
    }
    parseURL2(url, &p_url, current);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, current, referer, NULL);
    if (GetCurrentbuf() != cur_buf) /* success */
        pushHashHist(URLHist, parsedURL2Str(&GetCurrentbuf()->currentURL)->ptr);
}

void anchorMn(Anchor *(*menu_func)(Buffer *), int go)
{
    Anchor *a;
    BufferPoint *po;

    if (!GetCurrentbuf()->href || !GetCurrentbuf()->hmarklist)
        return;
    a = menu_func(GetCurrentbuf());
    if (!a || a->hseq < 0)
        return;
    po = &GetCurrentbuf()->hmarklist->marks[a->hseq];
    gotoLine(GetCurrentbuf(), po->line);
    GetCurrentbuf()->pos = po->pos;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
    if (go)
        followA();
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

    if (GetCurrentbuf()->firstLine == NULL)
        return;
    if (CurrentKey == PrevKey && s != NULL)
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
    a = (only_img ? NULL : retrieveCurrentAnchor(GetCurrentbuf()));
    if (a == NULL)
    {
        a = (only_img ? NULL : retrieveCurrentForm(GetCurrentbuf()));
        if (a == NULL)
        {
            a = retrieveCurrentImg(GetCurrentbuf());
            if (a == NULL)
                return;
        }
        else
            s = Strnew_charp(form2str((FormItemList *)a->url));
    }
    if (s == NULL)
    {
        parseURL2(a->url, &pu, baseURL(GetCurrentbuf()));
        s = parsedURL2Str(&pu);
    }
    if (DecodeURL)
        s = Strnew_charp(url_unquote_conv(s->ptr, GetCurrentbuf()->document_charset));
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
    if (GetCurrentbuf()->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return parsedURL2Str(&GetCurrentbuf()->currentURL);
}

void repBuffer(Buffer *oldbuf, Buffer *buf)
{
    SetFirstbuf(replaceBuffer(GetFirstbuf(), oldbuf, buf));
    SetCurrentbuf(buf);
}

void _docCSet(wc_ces charset)
{
    if (GetCurrentbuf()->bufferprop & BP_INTERNAL)
        return;
    if (GetCurrentbuf()->sourcefile == NULL)
    {
        disp_message("Can't reload...", FALSE);
        return;
    }
    GetCurrentbuf()->document_charset = charset;
    GetCurrentbuf()->need_reshape = TRUE;
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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

    ClearCurrentKeyData(); /* not allowed in w3m-control: */
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
        displayBuffer(GetCurrentbuf(), B_NORMAL);
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
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void execdict(char *word)
{
    char *w, *dictcmd;
    Buffer *buf;

    if (!UseDictCommand || word == NULL || *word == '\0')
    {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    w = conv_to_system(word);
    if (*w == '\0')
    {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
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
        GetCurrentTab()->BufferPushFront(buf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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
    0, AL_UNSET, &nulcmd, NULL};
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

void SigAlarm(SIGNAL_ARG)
{
    char *data;

    if (CurrentAlarm()->sec > 0)
    {
        ClearCurrentKey();
        ClearCurrentKeyData();
        CurrentCmdData = data = (char *)CurrentAlarm()->data;
#ifdef USE_MOUSE
        if (use_mouse)
            mouse_inactive();
#endif
        CurrentAlarm()->cmd();
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
        if (GetCurrentbuf()->event)
        {
            if (GetCurrentbuf()->event->status != AL_UNSET)
                SetCurrentAlarm(GetCurrentbuf()->event);
            else
                GetCurrentbuf()->event = NULL;
        }
        if (!GetCurrentbuf()->event)
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

void tabURL0(TabPtr tab, char *prompt, int relative)
{
    Buffer *buf;

    if (tab == GetCurrentTab())
    {
        goURL0(prompt, relative);
        return;
    }
    _newT();
    buf = GetCurrentbuf();
    goURL0(prompt, relative);
    if (tab == NULL)
    {
        if (buf != GetCurrentbuf())
            delBuffer(buf);
        else
            deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentbuf())
    {
        /* buf <- p <- ... <- Currentbuf = c */
        Buffer *c, *p;

        c = GetCurrentbuf();
        p = prevBuffer(c, buf);
        p->nextBuffer = NULL;
        SetFirstbuf(buf);
        deleteTab(GetCurrentTab());
        SetCurrentTab(tab);
        for (buf = p; buf; buf = p)
        {
            p = prevBuffer(c, buf);
            GetCurrentTab()->BufferPushFront(buf);
        }
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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
    restorePosition(GetCurrentbuf(), &buf);
    GetCurrentbuf()->undo = b;
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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

AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status, Command cmd, void *data)
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
    DeleteAllTabs();
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
    char *f;
    while ((f = popText(fileToDelete)) != NULL)
        unlink(f);
}

char *searchKeyData()
{
    char *data = NULL;

    if (CurrentKeyData() != NULL && *CurrentKeyData() != '\0')
        data = CurrentKeyData();
    else if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
        data = CurrentCmdData;
    else if (CurrentKey >= 0)
        data = GetKeyData(CurrentKey());
    ClearCurrentKeyData();
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
    process_mouse(static_cast<MouseBtnAction>(btn), x, y);
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
    Buffer *buf = GetCurrentbuf()->linkBuffer[LB_N_INFO];
    wc_ces charset;

    if (buf == NULL)
        return;
    delBuffer(GetCurrentbuf());
    SetCurrentbuf(buf);
    if (GetCurrentbuf()->bufferprop & BP_INTERNAL)
        return;
    charset = GetCurrentbuf()->document_charset;
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

    an = retrieveCurrentImg(GetCurrentbuf());
    x = GetCurrentbuf()->cursorX + GetCurrentbuf()->rootX;
    y = GetCurrentbuf()->cursorY + GetCurrentbuf()->rootY;
    a = follow_map_menu(GetCurrentbuf(), name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0')
    {
#endif
#ifndef MENU_MAP
        Buffer *buf = follow_map_panel(GetCurrentbuf(), name);

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
    parseURL2(a->url, &p_url, baseURL(GetCurrentbuf()));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    if (check_target && open_tab_blank && a->target &&
        (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank")))
    {
        _newT();
        Buffer *buf = GetCurrentbuf();
        cmd_loadURL(a->url, baseURL(GetCurrentbuf()),
                    parsedURL2Str(&GetCurrentbuf()->currentURL)->ptr, NULL);
        if (buf != GetCurrentbuf())
            delBuffer(buf);
        else
            deleteTab(GetCurrentTab());
        displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
        return;
    }
    cmd_loadURL(a->url, baseURL(GetCurrentbuf()),
                parsedURL2Str(&GetCurrentbuf()->currentURL)->ptr, NULL);
#endif
}

/* process form */
void followForm(void)
{
    _followForm(FALSE);
}

void SigPipe(SIGNAL_ARG)
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

void
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
    if (GetCurrentTab())
        displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
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

struct Event
{
    Command cmd;
    void *data;
    Event *next;
};
static Event *CurrentEvent = NULL;
static Event *LastEvent = NULL;

void pushEvent(Command cmd, void *data)
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
        ClearCurrentKey();
        ClearCurrentKeyData();
        CurrentCmdData = (char *)CurrentEvent->data;
        CurrentEvent->cmd();
        CurrentCmdData = NULL;
        CurrentEvent = CurrentEvent->next;
        return 1;
    }
    return 0;
}

static int
parse_ansi_color(char **str, Lineprop *effect, Linecolor *color)
{
    char *p = *str, *q;
    Lineprop e = *effect;
    Linecolor c = *color;
    int i;

    if (*p != ESC_CODE || *(p + 1) != '[')
        return 0;
    p += 2;
    for (q = p; IS_DIGIT(*q) || *q == ';'; q++)
        ;
    if (*q != 'm')
        return 0;
    *str = q + 1;
    while (1)
    {
        if (*p == 'm')
        {
            e = PE_NORMAL;
            c = 0;
            break;
        }
        if (IS_DIGIT(*p))
        {
            q = p;
            for (p++; IS_DIGIT(*p); p++)
                ;
            i = atoi(allocStr(q, p - q));
            switch (i)
            {
            case 0:
                e = PE_NORMAL;
                c = 0;
                break;
            case 1:
            case 5:
                e = PE_BOLD;
                break;
            case 4:
                e = PE_UNDER;
                break;
            case 7:
                e = PE_STAND;
                break;
            case 100: /* for EWS4800 kterm */
                c = 0;
                break;
            case 39:
                c &= 0xf0;
                break;
            case 49:
                c &= 0x0f;
                break;
            default:
                if (i >= 30 && i <= 37)
                    c = (c & 0xf0) | (i - 30) | 0x08;
                else if (i >= 40 && i <= 47)
                    c = (c & 0x0f) | ((i - 40) << 4) | 0x80;
                break;
            }
            if (*p == 'm')
                break;
        }
        else
        {
            e = PE_NORMAL;
            c = 0;
            break;
        }
        p++; /* *p == ';' */
    }
    *effect = e;
    *color = c;
    return 1;
}

/* 
 * Check character type
 */
Str checkType(Str s, Lineprop **oprop, Linecolor **ocolor)
{
    Lineprop mode;
    Lineprop effect = PE_NORMAL;
    Lineprop *prop;
    static Lineprop *prop_buffer = NULL;
    static int prop_size = 0;
    char *str = s->ptr, *endp = &s->ptr[s->length], *bs = NULL;
#ifdef USE_ANSI_COLOR
    Lineprop ceffect = PE_NORMAL;
    Linecolor cmode = 0;
    int check_color = FALSE;
    Linecolor *color = NULL;
    static Linecolor *color_buffer = NULL;
    static int color_size = 0;
    char *es = NULL;
#endif
    int do_copy = FALSE;
    int i;
    int plen = 0, clen;

    if (prop_size < s->length)
    {
        prop_size = (s->length > LINELEN) ? s->length : LINELEN;
        prop_buffer = New_Reuse(Lineprop, prop_buffer, prop_size);
    }
    prop = prop_buffer;

    if (ShowEffect)
    {
        bs = (char *)memchr(str, '\b', s->length);
#ifdef USE_ANSI_COLOR
        if (ocolor)
        {
            es = (char *)memchr(str, ESC_CODE, s->length);
            if (es)
            {
                if (color_size < s->length)
                {
                    color_size = (s->length > LINELEN) ? s->length : LINELEN;
                    color_buffer = New_Reuse(Linecolor, color_buffer,
                                             color_size);
                }
                color = color_buffer;
            }
        }
#endif
        if ((bs != NULL)
#ifdef USE_ANSI_COLOR
            || (es != NULL)
#endif
        )
        {
            char *sp = str, *ep;
            s = Strnew_size(s->length);
            do_copy = TRUE;
            ep = bs ? (bs - 2) : endp;
#ifdef USE_ANSI_COLOR
            if (es && ep > es - 2)
                ep = es - 2;
#endif
            for (; str < ep && IS_ASCII(*str); str++)
            {
                *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
#ifdef USE_ANSI_COLOR
                if (color)
                    *(color++) = 0;
#endif
            }
            Strcat_charp_n(s, sp, (int)(str - sp));
        }
    }
    if (!do_copy)
    {
        for (; str < endp && IS_ASCII(*str); str++)
            *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
    }

    while (str < endp)
    {
        if (prop - prop_buffer >= prop_size)
            break;
        if (bs != NULL)
        {
#ifdef USE_M17N
            if (str == bs - 2 && !strncmp(str, "__\b\b", 4))
            {
                str += 4;
                effect = PE_UNDER;
                if (str < endp)
                    bs = (char *)memchr(str, '\b', endp - str);
                continue;
            }
            else
#endif
                if (str == bs - 1 && *str == '_')
            {
                str += 2;
                effect = PE_UNDER;
                if (str < endp)
                    bs = (char *)memchr(str, '\b', endp - str);
                continue;
            }
            else if (str == bs)
            {
                if (*(str + 1) == '_')
                {
                    if (s->length)
                    {
                        str += 2;
#ifdef USE_M17N
                        for (i = 1; i <= plen; i++)
                            *(prop - i) |= PE_UNDER;
#else
                        *(prop - 1) |= PE_UNDER;
#endif
                    }
                    else
                    {
                        str++;
                    }
                }
#ifdef USE_M17N
                else if (!strncmp(str + 1, "\b__", 3))
                {
                    if (s->length)
                    {
                        str += (plen == 1) ? 3 : 4;
                        for (i = 1; i <= plen; i++)
                            *(prop - i) |= PE_UNDER;
                    }
                    else
                    {
                        str += 2;
                    }
                }
                else if (*(str + 1) == '\b')
                {
                    if (s->length)
                    {
                        clen = get_mclen(str + 2);
                        if (plen == clen &&
                            !strncmp(str - plen, str + 2, plen))
                        {
                            for (i = 1; i <= plen; i++)
                                *(prop - i) |= PE_BOLD;
                            str += 2 + clen;
                        }
                        else
                        {
                            Strshrink(s, plen);
                            prop -= plen;
                            str += 2;
                        }
                    }
                    else
                    {
                        str += 2;
                    }
                }
#endif
                else
                {
                    if (s->length)
                    {
#ifdef USE_M17N
                        clen = get_mclen(str + 1);
                        if (plen == clen &&
                            !strncmp(str - plen, str + 1, plen))
                        {
                            for (i = 1; i <= plen; i++)
                                *(prop - i) |= PE_BOLD;
                            str += 1 + clen;
                        }
                        else
                        {
                            Strshrink(s, plen);
                            prop -= plen;
                            str++;
                        }
#else
                        if (*(str - 1) == *(str + 1))
                        {
                            *(prop - 1) |= PE_BOLD;
                            str += 2;
                        }
                        else
                        {
                            Strshrink(s, 1);
                            prop--;
                            str++;
                        }
#endif
                    }
                    else
                    {
                        str++;
                    }
                }
                if (str < endp)
                    bs = (char *)memchr(str, '\b', endp - str);
                continue;
            }
#ifdef USE_ANSI_COLOR
            else if (str > bs)
                bs = (char *)memchr(str, '\b', endp - str);
#endif
        }
#ifdef USE_ANSI_COLOR
        if (es != NULL)
        {
            if (str == es)
            {
                int ok = parse_ansi_color(&str, &ceffect, &cmode);
                if (str < endp)
                    es = (char *)memchr(str, ESC_CODE, endp - str);
                if (ok)
                {
                    if (cmode)
                        check_color = TRUE;
                    continue;
                }
            }
            else if (str > es)
                es = (char *)memchr(str, ESC_CODE, endp - str);
        }
#endif

        plen = get_mclen(str);
        mode = get_mctype(str) | effect;
#ifdef USE_ANSI_COLOR
        if (color)
        {
            *(color++) = cmode;
            mode |= ceffect;
        }
#endif
        *(prop++) = mode;
#ifdef USE_M17N
        if (plen > 1)
        {
            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
            for (i = 1; i < plen; i++)
            {
                *(prop++) = mode;
#ifdef USE_ANSI_COLOR
                if (color)
                    *(color++) = cmode;
#endif
            }
            if (do_copy)
                Strcat_charp_n(s, (char *)str, plen);
            str += plen;
        }
        else
#endif
        {
            if (do_copy)
                Strcat_char(s, (char)*str);
            str++;
        }
        effect = PE_NORMAL;
    }
    *oprop = prop_buffer;
#ifdef USE_ANSI_COLOR
    if (ocolor)
        *ocolor = check_color ? color_buffer : NULL;
#endif
    return s;
}

void pcmap(void)
{
}
