
#include "fm.h"
#include "indep.h"
#include "rc.h"

#include "html/frame.h"
#include "frontend/display.h"
#include "html/parsetag.h"
#include "public.h"
#include "file.h"
#include "frontend/linein.h"
#include <setjmp.h>
#include <signal.h>
#include "ucs.h"
#include "frontend/terms.h"
#include "dispatcher.h"
#include "html/image.h"
#include "commands.h"
#include "transport/url.h"
#include "http/cookie.h"
#include "ctrlcode.h"
#include "frontend/mouse.h"
#include "html/anchor.h"
#include "html/maparea.h"
#include "frontend/buffer.h"
#include "entity.h"
#include "transport/loader.h"

int searchKeyNum(void)
{
    int n = 1;
    auto d = searchKeyData();
    if (d != NULL)
        n = atoi(d);
    return n * (std::max(1, prec_num()));
}

void nscroll(int n)
{
    BufferPtr buf = GetCurrentTab()->GetCurrentBuffer();
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
}

static char *SearchString = NULL;
int (*searchRoutine)(BufferPtr, char *);

void clear_mark(Line *l)
{
    int pos;
    if (!l)
        return;
    for (pos = 0; pos < l->size; pos++)
        l->propBuf[pos] &= ~PE_MARK;
}

void disp_srchresult(int result, const char* prompt, char *str)
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
    static int (*routine[2])(BufferPtr, char *) = {
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
        GetCurrentTab()->GetCurrentBuffer()->pos += 1;
    result = srchcore(SearchString, routine[reverse]);
    if (result & SR_FOUND)
        clear_mark(GetCurrentTab()->GetCurrentBuffer()->currentLine);
    displayCurrentbuf(B_NORMAL);
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
dump_extra(BufferPtr buf)
{
    printf("W3m-current-url: %s\n", buf->currentURL.ToStr()->ptr);
    if (buf->baseURL)
        printf("W3m-base-url: %s\n", buf->baseURL.ToStr()->ptr);
    printf("W3m-document-charset: %s\n",
           wc_ces_to_charset(buf->document_charset));

#ifdef USE_SSL
    if (buf->ssl_certificate)
    {
        Str tmp = Strnew();
        char *p;
        for (p = buf->ssl_certificate; *p; p++)
        {
            tmp->Push(*p);
            if (*p == '\n')
            {
                for (; *(p + 1) == '\n'; p++)
                    ;
                if (*(p + 1))
                    tmp->Push('\t');
            }
        }
        if (tmp->Back() != '\n')
            tmp->Push('\n');
        printf("W3m-ssl-certificate: %s", tmp->ptr);
    }
#endif
}

static void
dump_source(BufferPtr buf)
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
dump_head(BufferPtr buf)
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

void do_dump(BufferPtr buf)
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
            for (i = 0; i < buf->href.size(); i++)
            {
                ParsedURL pu;
                static Str s = NULL;
                if (buf->href.anchors[i].slave)
                    continue;
                pu.Parse2(buf->href.anchors[i].url, buf->BaseURL());
                s = pu.ToStr();
                if (DecodeURL)
                    s = Strnew(url_unquote_conv(s->ptr, GetCurrentTab()->GetCurrentBuffer()->document_charset));
                printf("[%d] %s\n", buf->href.anchors[i].hseq + 1, s->ptr);
            }
        }
    }
    mySignal(SIGINT, prevtrap);
}

/* search by regular expression */
int srchcore(char *str, int (*func)(BufferPtr, char *))
{
    int i, result = SR_NOTFOUND;

    if (str != NULL && str != SearchString)
        SearchString = str;
    if (SearchString == NULL || *SearchString == '\0')
        return SR_NOTFOUND;

    str = conv_search_string(SearchString, DisplayCharset);
    MySignalHandler prevtrap = mySignal(SIGINT, intTrap);
    crmode();
    if (SETJMP(IntReturn) == 0)
    {
        int prec = prec_num() ? prec_num() : 1;
        for (i = 0; i < prec; i++)
        {
            result = func(GetCurrentTab()->GetCurrentBuffer(), str);
            if (i < prec - 1 && result & SR_FOUND)
                clear_mark(GetCurrentTab()->GetCurrentBuffer()->currentLine);
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
                GetCurrentTab()->GetCurrentBuffer()->pos += 1;
            SAVE_BUFPOSITION(&sbuf);
            if (srchcore(str, searchRoutine) == SR_NOTFOUND && searchRoutine == forwardSearch)
            {
                GetCurrentTab()->GetCurrentBuffer()->pos -= 1;
                SAVE_BUFPOSITION(&sbuf);
            }
            arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
            displayCurrentbuf(B_FORCE_REDRAW);
            clear_mark(GetCurrentTab()->GetCurrentBuffer()->currentLine);
            return -1;
        }
        else
            return 020; /* _prev completion for C-s C-s */
    }
    else if (*str)
    {
        RESTORE_BUFPOSITION(&sbuf);
        arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
        srchcore(str, searchRoutine);
        arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
        currentLine = GetCurrentTab()->GetCurrentBuffer()->currentLine;
        pos = GetCurrentTab()->GetCurrentBuffer()->pos;
    }
    displayCurrentbuf(B_FORCE_REDRAW);
    clear_mark(GetCurrentTab()->GetCurrentBuffer()->currentLine);
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

void isrch(int (*func)(BufferPtr, char *), const char* prompt)
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
    displayCurrentbuf(B_FORCE_REDRAW);
}

void srch(int (*func)(BufferPtr, char *), const char* prompt)
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
            displayCurrentbuf(B_NORMAL);
            return;
        }
        disp = TRUE;
    }
    pos = GetCurrentTab()->GetCurrentBuffer()->pos;
    if (func == forwardSearch)
        GetCurrentTab()->GetCurrentBuffer()->pos += 1;
    result = srchcore(str, func);
    if (result & SR_FOUND)
        clear_mark(GetCurrentTab()->GetCurrentBuffer()->currentLine);
    else
        GetCurrentTab()->GetCurrentBuffer()->pos = pos;
    displayCurrentbuf(B_NORMAL);
    if (disp)
        disp_srchresult(result, prompt, str);
    searchRoutine = func;
}

void shiftvisualpos(BufferPtr buf, int shift)
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
    BufferPtr buf;

    buf = loadGeneralFile(file_to_url(fn), NULL, NO_REFERER, RG_NONE, NULL);
    if (buf == NULL)
    {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
        disp_err_message(emsg, FALSE);
    }
    else
    {
        GetCurrentTab()->PushBufferCurrentPrev(buf);
        if (RenderFrame && GetCurrentTab()->GetCurrentBuffer()->frameset != NULL)
            rFrame();
    }
    displayCurrentbuf(B_NORMAL);
}

void cmd_loadURL(char *url, ParsedURL *current, char *referer, FormList *request)
{
    BufferPtr buf;

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
    buf = loadGeneralFile(url, current, referer, RG_NONE, request);
    if (buf == NULL)
    {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
        disp_err_message(emsg, FALSE);
    }
    else
    {
        GetCurrentTab()->PushBufferCurrentPrev(buf);
        if (RenderFrame && GetCurrentTab()->GetCurrentBuffer()->frameset != NULL)
            rFrame();
    }
    displayCurrentbuf(B_NORMAL);
}

int handleMailto(const char *url)
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
        to = Strnew(html_unquote(const_cast<char*>(url)));
    }
    else
    {
        to = Strnew(url + 7);
        if ((pos = strchr(to->ptr, '?')) != NULL)
            to->Truncate(pos - to->ptr);
    }
    fmTerm();
    system(myExtCommand(Mailer, shell_quote(file_unquote(to->ptr)),
                        FALSE)
               ->ptr);
    fmInit();
    displayCurrentbuf(B_FORCE_REDRAW);
    pushHashHist(URLHist, url);
    return 1;
}

/* Move cursor left */
void _movL(int n)
{
    int i, m = searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorLeft(GetCurrentTab()->GetCurrentBuffer(), n);
    displayCurrentbuf(B_NORMAL);
}

/* Move cursor downward */
void _movD(int n)
{
    int i, m = searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorDown(GetCurrentTab()->GetCurrentBuffer(), n);
    displayCurrentbuf(B_NORMAL);
}

/* move cursor upward */
void _movU(int n)
{
    int i, m = searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorUp(GetCurrentTab()->GetCurrentBuffer(), n);
    displayCurrentbuf(B_NORMAL);
}

/* Move cursor right */
void _movR(int n)
{
    int i, m = searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorRight(GetCurrentTab()->GetCurrentBuffer(), n);
    displayCurrentbuf(B_NORMAL);
}

int prev_nonnull_line(Line *line)
{
    Line *l;

    for (l = line; l != NULL && l->len == 0; l = l->prev)
        ;
    if (l == NULL || l->len == 0)
        return -1;

    GetCurrentTab()->GetCurrentBuffer()->currentLine = l;
    if (l != line)
        GetCurrentTab()->GetCurrentBuffer()->pos = GetCurrentTab()->GetCurrentBuffer()->currentLine->len;
    return 0;
}

int next_nonnull_line(Line *line)
{
    Line *l;

    for (l = line; l != NULL && l->len == 0; l = l->next)
        ;

    if (l == NULL || l->len == 0)
        return -1;

    GetCurrentTab()->GetCurrentBuffer()->currentLine = l;
    if (l != line)
        GetCurrentTab()->GetCurrentBuffer()->pos = 0;
    return 0;
}

char *
getCurWord(BufferPtr buf, int *spos, int *epos)
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

uint32_t getChar(char *p)
{
    return wc_any_to_ucs(wtf_parse1((uint8_t **)&p));
}

int is_wordchar(uint32_t c)
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
        displayCurrentbuf(B_NORMAL);
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

/* Go to specified line */
void _goLine(char *l)
{
    if (l == NULL || *l == '\0' || GetCurrentTab()->GetCurrentBuffer()->currentLine == NULL)
    {
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }
    GetCurrentTab()->GetCurrentBuffer()->pos = 0;
    if (((*l == '^') || (*l == '$')) && prec_num())
    {
        gotoRealLine(GetCurrentTab()->GetCurrentBuffer(), prec_num());
    }
    else if (*l == '^')
    {
        GetCurrentTab()->GetCurrentBuffer()->topLine = GetCurrentTab()->GetCurrentBuffer()->currentLine = GetCurrentTab()->GetCurrentBuffer()->firstLine;
    }
    else if (*l == '$')
    {
        GetCurrentTab()->GetCurrentBuffer()->topLine =
            lineSkip(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->lastLine,
                     -(GetCurrentTab()->GetCurrentBuffer()->LINES + 1) / 2, TRUE);
        GetCurrentTab()->GetCurrentBuffer()->currentLine = GetCurrentTab()->GetCurrentBuffer()->lastLine;
    }
    else
        gotoRealLine(GetCurrentTab()->GetCurrentBuffer(), atoi(l));
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
}

int cur_real_linenumber(BufferPtr buf)
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

char *inputLineHist(const char* prompt, const char *def_str, int flag, Hist *hist)
{
    return inputLineHistSearch(prompt, def_str, flag, hist, NULL);
}

char *inputStrHist(const char* prompt, char *def_str, Hist *hist)
{
    return inputLineHist(prompt, def_str, IN_STRING, hist);
}

char *inputLine(const char* prompt, char *def_str, int flag)
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

    char *p;
    FormItemList *fi, *f2;
    Str tmp, tmp2;
    int multipart = 0, i;

    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    l = GetCurrentTab()->GetCurrentBuffer()->currentLine;

    auto a = retrieveCurrentForm(GetCurrentTab()->GetCurrentBuffer());
    if (a == NULL)
        return;
    fi = a->item;
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
        fi->value = Strnew(p);
        formUpdateBuffer(a, GetCurrentTab()->GetCurrentBuffer(), fi);
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
        fi->value = Strnew(p);
        formUpdateBuffer(a, GetCurrentTab()->GetCurrentBuffer(), fi);
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
        fi->value = Strnew(p);
        formUpdateBuffer(a, GetCurrentTab()->GetCurrentBuffer(), fi);
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
        formUpdateBuffer(a, GetCurrentTab()->GetCurrentBuffer(), fi);
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
        formRecheckRadio(a, GetCurrentTab()->GetCurrentBuffer(), fi);
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
        formUpdateBuffer(a, GetCurrentTab()->GetCurrentBuffer(), fi);
        break;
#ifdef MENU_SELECT
    case FORM_SELECT:
        if (submit)
            goto do_submit;
        if (!formChooseOptionByMenu(fi,
                                    GetCurrentTab()->GetCurrentBuffer()->cursorX - GetCurrentTab()->GetCurrentBuffer()->pos +
                                        a->start.pos + GetCurrentTab()->GetCurrentBuffer()->rootX,
                                    GetCurrentTab()->GetCurrentBuffer()->cursorY + GetCurrentTab()->GetCurrentBuffer()->rootY))
            break;
        formUpdateBuffer(a, GetCurrentTab()->GetCurrentBuffer(), fi);
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

        tmp2 = fi->parent->action->Clone();
        if (tmp2->Cmp("!CURRENT_URL!") == 0)
        {
            /* It means "current URL" */
            tmp2 = GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr();
            if ((p = strchr(tmp2->ptr, '?')) != NULL)
                tmp2->Pop((tmp2->ptr + tmp2->Size()) - p);
        }

        if (fi->parent->method == FORM_METHOD_GET)
        {
            if ((p = strchr(tmp2->ptr, '?')) != NULL)
                tmp2->Pop((tmp2->ptr + tmp2->Size()) - p);
            tmp2->Push("?");
            tmp2->Push(tmp);
            loadLink(tmp2->ptr, a->target.c_str(), NULL, NULL);
        }
        else if (fi->parent->method == FORM_METHOD_POST)
        {
            BufferPtr buf;
            if (multipart)
            {
                struct stat st;
                stat(fi->parent->body, &st);
                fi->parent->length = st.st_size;
            }
            else
            {
                fi->parent->body = tmp->ptr;
                fi->parent->length = tmp->Size();
            }
            buf = loadLink(tmp2->ptr, a->target.c_str(), NULL, fi->parent);
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
        else if ((fi->parent->method == FORM_METHOD_INTERNAL && (fi->parent->action->Cmp("map") == 0 || fi->parent->action->Cmp("none") == 0)) || GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
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
        for (i = 0; i < GetCurrentTab()->GetCurrentBuffer()->formitem.size(); i++)
        {
            auto a2 = &GetCurrentTab()->GetCurrentBuffer()->formitem.anchors[i];
            f2 = a2->item;
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
                formUpdateBuffer(a2, GetCurrentTab()->GetCurrentBuffer(), f2);
            }
        }
        break;
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
    displayCurrentbuf(B_FORCE_REDRAW);
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
        if (f2->name->Size() == 0 &&
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
                getMapXY(GetCurrentTab()->GetCurrentBuffer(), retrieveCurrentImg(GetCurrentTab()->GetCurrentBuffer()), &x, &y);
#endif
                *query = conv_form_encoding(f2->name, fi, GetCurrentTab()->GetCurrentBuffer())->Clone();
                (*query)->Push(".x");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                                Sprintf("%d", x)->ptr);
                *query = conv_form_encoding(f2->name, fi, GetCurrentTab()->GetCurrentBuffer())->Clone();
                (*query)->Push(".y");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                                Sprintf("%d", y)->ptr);
            }
            else if (f2->name && f2->name->Size() > 0 && f2->value != NULL)
            {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value, fi, GetCurrentTab()->GetCurrentBuffer());
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                                         conv_form_encoding(f2->name, fi,
                                                            GetCurrentTab()->GetCurrentBuffer())
                                             ->ptr,
                                         (*query)->ptr,
                                         Str_conv_to_system(f2->value)->ptr);
                else
                    form_write_data(body, fi->parent->boundary,
                                    conv_form_encoding(f2->name, fi,
                                                       GetCurrentTab()->GetCurrentBuffer())
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
                getMapXY(GetCurrentTab()->GetCurrentBuffer(), retrieveCurrentImg(GetCurrentTab()->GetCurrentBuffer()), &x, &y);
                (*query)->Push(
                    conv_form_encoding(f2->name, fi, GetCurrentTab()->GetCurrentBuffer())->UrlEncode());
                (*query)->Push(Sprintf(".x=%d&", x));
                (*query)->Push(conv_form_encoding(f2->name, fi, GetCurrentTab()->GetCurrentBuffer())->UrlEncode());
                (*query)->Push(Sprintf(".y=%d", y));
            }
            else
            {
                /* not IMAGE */
                if (f2->name && f2->name->Size() > 0)
                {
                    (*query)->Push(conv_form_encoding(f2->name, fi, GetCurrentTab()->GetCurrentBuffer())->UrlEncode());
                    (*query)->Push('=');
                }
                if (f2->value != NULL)
                {
                    if (fi->parent->method == FORM_METHOD_INTERNAL)
                        (*query)->Push(f2->value->UrlEncode());
                    else
                    {
                        (*query)->Push(conv_form_encoding(f2->value, fi, GetCurrentTab()->GetCurrentBuffer())->UrlEncode());
                    }
                }
            }
            if (f2->next)
                (*query)->Push('&');
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
        while ((*query)->Back() == '&')
            (*query)->Pop(1);
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

BufferPtr loadLink(const char *url, const char *target, const char *referer, FormList *request)
{
    BufferPtr nfbuf;
    union frameset_element *f_element = NULL;
    ParsedURL *base, pu;

    message(Sprintf("loading %s", url)->ptr, 0, 0);
    refresh();

    base = GetCurrentTab()->GetCurrentBuffer()->BaseURL();
    if (base == NULL ||
        base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->ptr;
    auto buf = loadGeneralFile(const_cast<char*>(url), GetCurrentTab()->GetCurrentBuffer()->BaseURL(), const_cast<char*>(referer), RG_NONE, request);
    if (buf == NULL)
    {
        char *emsg = Sprintf("Can't load %s", url)->ptr;
        disp_err_message(emsg, FALSE);
        return NULL;
    }

    pu.Parse2(url, base);
    pushHashHist(URLHist, pu.ToStr()->ptr);

    if (!on_target) /* open link as an indivisual page */
        return loadNormalBuf(buf, TRUE);

    if (do_download) /* download (thus no need to render frame) */
        return loadNormalBuf(buf, FALSE);

    if (target == NULL ||                                             /* no target specified (that means this page is not a frame page) */
        !strcmp(target, "_top") ||                                    /* this link is specified to be opened as an indivisual * page */
        !(GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_FRAME) /* This page is not a frame page */
    )
    {
        return loadNormalBuf(buf, TRUE);
    }
    nfbuf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_FRAME];
    if (nfbuf == NULL)
    {
        /* original page (that contains <frameset> tag) doesn't exist */
        return loadNormalBuf(buf, TRUE);
    }

    f_element = search_frame(nfbuf->frameset, const_cast<char*>(target));
    if (f_element == NULL)
    {
        /* specified target doesn't exist in this frameset */
        return loadNormalBuf(buf, TRUE);
    }

    /* frame page */

    /* stack current frameset */
    pushFrameTree(&(nfbuf->frameQ), copyFrameSet(nfbuf->frameset), GetCurrentTab()->GetCurrentBuffer());
    /* delete frame view buffer */
    auto tab = GetCurrentTab();
    tab->DeleteBuffer(tab->GetCurrentBuffer());
    GetCurrentTab()->SetCurrentBuffer(nfbuf);
    /* nfbuf->frameset = copyFrameSet(nfbuf->frameset); */
    resetFrameElement(f_element, buf, const_cast<char*>(referer), request);
    rFrame();
    {
        const Anchor *al = NULL;
        auto label = pu.label;

        if (label.size() && f_element->element->attr == F_BODY)
        {
            al = f_element->body->nameList.SearchByUrl(label.c_str());
        }
        if (!al)
        {
            label = std::string("_") + target;
            al = searchURLLabel(GetCurrentTab()->GetCurrentBuffer(), const_cast<char*>(label.c_str()));
        }
        if (al)
        {
            gotoLine(GetCurrentTab()->GetCurrentBuffer(), al->start.line);
            if (label_topline)
                GetCurrentTab()->GetCurrentBuffer()->topLine = lineSkip(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->topLine,
                                                                        GetCurrentTab()->GetCurrentBuffer()->currentLine->linenumber -
                                                                            GetCurrentTab()->GetCurrentBuffer()->topLine->linenumber,
                                                                        FALSE);
            GetCurrentTab()->GetCurrentBuffer()->pos = al->start.pos;
            arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
        }
    }
    displayCurrentbuf(B_NORMAL);
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
    list->action = srclist->action->Clone();
    list->charset = srclist->charset;
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next)
    {
        item = New(FormItemList);
        item->type = srcitem->type;
        item->name = srcitem->name->Clone();
        item->value = srcitem->value->Clone();
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
            opt->value = srcopt->value->Clone();
            opt->label = srcopt->label->Clone();
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
            item->label = srcitem->label->Clone();
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

Str conv_form_encoding(Str val, FormItemList *fi, BufferPtr buf)
{
    wc_ces charset = SystemCharset;

    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (buf->document_charset && buf->document_charset != WC_CES_US_ASCII)
        charset = buf->document_charset;
    return wc_Str_conv_strict(val, InnerCharset, charset);
}

BufferPtr loadNormalBuf(BufferPtr buf, int renderframe)
{
    GetCurrentTab()->PushBufferCurrentPrev(buf);
    if (renderframe && RenderFrame && GetCurrentTab()->GetCurrentBuffer()->frameset != NULL)
        rFrame();
    return buf;
}

/* go to the next [visited] anchor */
void _nextA(int visited)
{
    auto &hl = GetCurrentTab()->GetCurrentBuffer()->hmarklist;
    BufferPoint *po;
    const Anchor *an;
    const Anchor *pan;
    int i, x, y, n = searchKeyNum();
    ParsedURL url;

    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    if (hl.empty())
        return;

    an = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
    if (visited != TRUE && an == NULL)
        an = retrieveCurrentForm(GetCurrentTab()->GetCurrentBuffer());

    y = GetCurrentTab()->GetCurrentBuffer()->currentLine->linenumber;
    x = GetCurrentTab()->GetCurrentBuffer()->pos;

    if (visited == TRUE)
    {
        n = hl.size();
    }

    for (i = 0; i < n; i++)
    {
        pan = an;
        if (an && an->hseq >= 0)
        {
            int hseq = an->hseq + 1;
            do
            {
                if (hseq >= hl.size())
                {
                    if (visited == TRUE)
                        return;
                    an = pan;
                    goto _end;
                }
                po = &hl[hseq];
                an = GetCurrentTab()->GetCurrentBuffer()->href.RetrieveAnchor(po->line, po->pos);
                if (visited != TRUE && an == NULL)
                    an = GetCurrentTab()->GetCurrentBuffer()->formitem.RetrieveAnchor(po->line,
                                                                                      po->pos);
                hseq++;
                if (visited == TRUE && an)
                {
                    url.Parse2(an->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
                    if (getHashHist(URLHist, url.ToStr()->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = GetCurrentTab()->GetCurrentBuffer()->href.ClosestNext(NULL, x, y);
            if (!visited)
                an = GetCurrentTab()->GetCurrentBuffer()->formitem.ClosestNext(an, x, y);
            if (an == NULL)
            {
                if (visited)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == TRUE)
            {
                url.Parse2(an->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
                if (getHashHist(URLHist, url.ToStr()->ptr))
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
    po = &hl[an->hseq];
    gotoLine(GetCurrentTab()->GetCurrentBuffer(), po->line);
    GetCurrentTab()->GetCurrentBuffer()->pos = po->pos;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
}

/* go to the previous anchor */
void _prevA(int visited)
{
    auto &hl = GetCurrentTab()->GetCurrentBuffer()->hmarklist;
    BufferPoint *po;
    int i, x, y, n = searchKeyNum();
    ParsedURL url;

    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    if (hl.empty())
        return;

    auto an = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
    if (visited != TRUE && an == NULL)
        an = retrieveCurrentForm(GetCurrentTab()->GetCurrentBuffer());

    y = GetCurrentTab()->GetCurrentBuffer()->currentLine->linenumber;
    x = GetCurrentTab()->GetCurrentBuffer()->pos;

    if (visited == TRUE)
    {
        n = hl.size();
    }

    for (i = 0; i < n; i++)
    {
        auto pan = an;
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
                po = &hl[hseq];
                an = GetCurrentTab()->GetCurrentBuffer()->href.RetrieveAnchor(po->line, po->pos);
                if (visited != TRUE && an == NULL)
                    an = GetCurrentTab()->GetCurrentBuffer()->formitem.RetrieveAnchor(po->line, po->pos);
                hseq--;
                if (visited == TRUE && an)
                {
                    url.Parse2(an->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
                    if (getHashHist(URLHist, url.ToStr()->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = GetCurrentTab()->GetCurrentBuffer()->href.ClosestPrev(NULL, x, y);
            if (visited != TRUE)
                an = GetCurrentTab()->GetCurrentBuffer()->formitem.ClosestPrev(an, x, y);
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
                url.Parse2(an->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
                if (getHashHist(URLHist, url.ToStr()->ptr))
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
    po = &hl[an->hseq];
    gotoLine(GetCurrentTab()->GetCurrentBuffer(), po->line);
    GetCurrentTab()->GetCurrentBuffer()->pos = po->pos;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
}

void gotoLabel(const char *label)
{
    auto al = searchURLLabel(GetCurrentTab()->GetCurrentBuffer(), const_cast<char*>(label));
    if (al == NULL)
    {
        /* FIXME: gettextize? */
        disp_message(Sprintf("%s is not found", label)->ptr, TRUE);
        return;
    }

    auto buf = GetCurrentTab()->GetCurrentBuffer()->Copy();

    for (int i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    buf->currentURL.label = allocStr(label, -1);
    pushHashHist(URLHist, buf->currentURL.ToStr()->ptr);
    (*buf->clone)++;
    GetCurrentTab()->PushBufferCurrentPrev(buf);
    gotoLine(GetCurrentTab()->GetCurrentBuffer(), al->start.line);
    if (label_topline)
        GetCurrentTab()->GetCurrentBuffer()->topLine = lineSkip(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->topLine,
                                                                GetCurrentTab()->GetCurrentBuffer()->currentLine->linenumber - GetCurrentTab()->GetCurrentBuffer()->topLine->linenumber,
                                                                FALSE);
    GetCurrentTab()->GetCurrentBuffer()->pos = al->start.pos;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
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
    auto &hl = GetCurrentTab()->GetCurrentBuffer()->hmarklist;
    Line *l;
    int i, x, y, n = searchKeyNum();

    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    if (hl.empty())
        return;

    auto an = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
    if (an == NULL)
        an = retrieveCurrentForm(GetCurrentTab()->GetCurrentBuffer());

    l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
    x = GetCurrentTab()->GetCurrentBuffer()->pos;
    y = l->linenumber;
    const Anchor *pan = NULL;
    for (i = 0; i < n; i++)
    {
        if (an)
            x = (d > 0) ? an->end.pos : an->start.pos - 1;
        an = NULL;
        while (1)
        {
            for (; x >= 0 && x < l->len; x += d)
            {
                an = GetCurrentTab()->GetCurrentBuffer()->href.RetrieveAnchor(y, x);
                if (!an)
                    an = GetCurrentTab()->GetCurrentBuffer()->formitem.RetrieveAnchor(y, x);
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
    gotoLine(GetCurrentTab()->GetCurrentBuffer(), y);
    GetCurrentTab()->GetCurrentBuffer()->pos = pan->start.pos;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
}

/* go to the next downward/upward anchor */
void nextY(int d)
{
    auto &hl = GetCurrentTab()->GetCurrentBuffer()->hmarklist;
    int i, x, y, n = searchKeyNum();
    int hseq;

    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    if (hl.empty())
        return;

    auto an = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
    if (an == NULL)
        an = retrieveCurrentForm(GetCurrentTab()->GetCurrentBuffer());

    x = GetCurrentTab()->GetCurrentBuffer()->pos;
    y = GetCurrentTab()->GetCurrentBuffer()->currentLine->linenumber + d;
    const Anchor *pan = NULL;
    hseq = -1;
    for (i = 0; i < n; i++)
    {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= GetCurrentTab()->GetCurrentBuffer()->lastLine->linenumber; y += d)
        {
            an = GetCurrentTab()->GetCurrentBuffer()->href.RetrieveAnchor(y, x);
            if (!an)
                an = GetCurrentTab()->GetCurrentBuffer()->formitem.RetrieveAnchor(y, x);
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
    gotoLine(GetCurrentTab()->GetCurrentBuffer(), pan->start.line);
    arrangeLine(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
}

int checkBackBuffer(TabPtr tab, BufferPtr buf)
{
    BufferPtr fbuf = buf->linkBuffer[LB_N_FRAME];

    if (fbuf)
    {
        if (fbuf->frameQ)
            return TRUE; /* Currentbuf has stacked frames */
        /* when no frames stacked and next is frame source, try next's
	 * nextBuffer */
        if (RenderFrame && fbuf == tab->NextBuffer(buf))
        {
            if (tab->NextBuffer(fbuf) != NULL)
                return TRUE;
            else
                return FALSE;
        }
    }

    if (tab->NextBuffer(buf))
        return TRUE;

    return FALSE;
}

/* go to specified URL */
void goURL0(const char* prompt, int relative)
{
    char *url, *referer;
    ParsedURL p_url, *current;
    BufferPtr cur_buf = GetCurrentTab()->GetCurrentBuffer();

    url = searchKeyData();
    if (url == NULL)
    {
        Hist *hist = copyHist(URLHist);
        current = GetCurrentTab()->GetCurrentBuffer()->BaseURL();
        if (current)
        {
            char *c_url = current->ToStr()->ptr;
            if (DefaultURLString == DEFAULT_URL_CURRENT)
            {
                url = c_url;
                if (DecodeURL)
                    url = url_unquote_conv(url, 0);
            }
            else
                pushHist(hist, c_url);
        }
        auto a = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
        if (a)
        {
            char *a_url;
            p_url.Parse2(a->url, current);
            a_url = p_url.ToStr()->ptr;
            if (DefaultURLString == DEFAULT_URL_LINK)
            {
                url = a_url;
                if (DecodeURL)
                    url = url_unquote_conv(url, GetCurrentTab()->GetCurrentBuffer()->document_charset);
            }
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(prompt, url, IN_URL, hist);
        if (url != NULL)
            SKIP_BLANKS(url);
    }

    if (url != NULL)
    {
        if ((relative || *url == '#') && GetCurrentTab()->GetCurrentBuffer()->document_charset)
            url = wc_conv_strict(url, InnerCharset,
                                 GetCurrentTab()->GetCurrentBuffer()->document_charset)
                      ->ptr;
        else
            url = conv_to_system(url);
    }

    if (url == NULL || *url == '\0')
    {
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }
    if (*url == '#')
    {
        gotoLabel(url + 1);
        return;
    }
    if (relative)
    {
        current = GetCurrentTab()->GetCurrentBuffer()->BaseURL();
        referer = GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->ptr;
    }
    else
    {
        current = NULL;
        referer = NULL;
    }
    p_url.Parse2(url, current);
    pushHashHist(URLHist, p_url.ToStr()->ptr);
    cmd_loadURL(url, current, referer, NULL);
    if (GetCurrentTab()->GetCurrentBuffer() != cur_buf) /* success */
        pushHashHist(URLHist, GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->ptr);
}

void anchorMn(Anchor *(*menu_func)(BufferPtr), int go)
{
    Anchor *a;
    BufferPoint *po;

    if (!GetCurrentTab()->GetCurrentBuffer()->href || GetCurrentTab()->GetCurrentBuffer()->hmarklist.empty())
        return;
    a = menu_func(GetCurrentTab()->GetCurrentBuffer());
    if (!a || a->hseq < 0)
        return;
    po = &GetCurrentTab()->GetCurrentBuffer()->hmarklist[a->hseq];
    gotoLine(GetCurrentTab()->GetCurrentBuffer(), po->line);
    GetCurrentTab()->GetCurrentBuffer()->pos = po->pos;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
    if (go)
        followA();
}

void _peekURL(int only_img)
{
    const Anchor *a;
    ParsedURL pu;
    static Str s = NULL;
    static Lineprop *p = NULL;
    Lineprop *pp;
    static int offset = 0, n;

    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    if (CurrentKey == PrevKey && s != NULL)
    {
        if (s->Size() - offset >= COLS)
            offset++;
        else if (s->Size() <= offset) /* bug ? */
            offset = 0;
        goto disp;
    }
    else
    {
        offset = 0;
    }
    s = NULL;
    a = (only_img ? NULL : retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer()));
    if (a == NULL)
    {
        a = (only_img ? NULL : retrieveCurrentForm(GetCurrentTab()->GetCurrentBuffer()));
        if (a == NULL)
        {
            a = retrieveCurrentImg(GetCurrentTab()->GetCurrentBuffer());
            if (a == NULL)
                return;
        }
        else
            s = Strnew(form2str(a->item));
    }
    if (s == NULL)
    {
        pu.Parse2(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
        s = pu.ToStr();
    }
    if (DecodeURL)
        s = Strnew(url_unquote_conv(s->ptr, GetCurrentTab()->GetCurrentBuffer()->document_charset));
#ifdef USE_M17N
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->Size());
    bcopy((void *)pp, (void *)p, s->Size() * sizeof(Lineprop));
#endif
disp:
    n = searchKeyNum();
    if (n > 1 && s->Size() > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
#ifdef USE_M17N
    while (offset < s->Size() && p[offset] & PC_WCHAR2)
        offset++;
#endif
    disp_message_nomouse(&s->ptr[offset], TRUE);
}

/* show current URL */
Str currentURL(void)
{
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr();
}

void repBuffer(BufferPtr oldbuf, BufferPtr buf)
{
    GetCurrentTab()->ReplaceBuffer(oldbuf, buf);
    GetCurrentTab()->SetCurrentBuffer(buf);
}

void _docCSet(wc_ces charset)
{
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
        return;
    if (GetCurrentTab()->GetCurrentBuffer()->sourcefile == NULL)
    {
        disp_message("Can't reload...", FALSE);
        return;
    }
    GetCurrentTab()->GetCurrentBuffer()->document_charset = charset;
    GetCurrentTab()->GetCurrentBuffer()->need_reshape = TRUE;
    displayCurrentbuf(B_FORCE_REDRAW);
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
        displayCurrentbuf(B_NORMAL);
        return;
    }

    if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' &&
        browser[len - 2] != '\\')
    {
        browser = allocStr(browser, len - 2);
        bg = 1;
    }
    cmd = myExtCommand(browser, shell_quote(url), FALSE);
    cmd->StripRight();
    fmTerm();
    mySystem(cmd->ptr, bg);
    fmInit();
    displayCurrentbuf(B_FORCE_REDRAW);
}

void execdict(char *word)
{
    char *w, *dictcmd;
    BufferPtr buf;

    if (!UseDictCommand || word == NULL || *word == '\0')
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    w = conv_to_system(word);
    if (*w == '\0')
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    dictcmd = Sprintf("%s?%s", DictCommand, Strnew(w)->UrlEncode()->ptr)->ptr;
    buf = loadGeneralFile(dictcmd, NULL, NO_REFERER, RG_NONE, NULL);
    if (buf == NULL)
    {
        disp_message("Execution failed", TRUE);
        return;
    }
    else
    {
        buf->filename = w;
        buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
        if (buf->type.empty())
            buf->type = "text/plain";
        GetCurrentTab()->PushBufferCurrentPrev(buf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

char *GetWord(BufferPtr buf)
{
    int b, e;
    char *p;

    if ((p = getCurWord(buf, &b, &e)) != NULL)
    {
        return Strnew_charp_n(p, e - b)->ptr;
    }
    return NULL;
}


void tabURL0(TabPtr tab, const char* prompt, int relative)
{
    if (tab == GetCurrentTab())
    {
        goURL0(prompt, relative);
        return;
    }

    CreateTabSetCurrent();
    auto buf = GetCurrentTab()->GetCurrentBuffer();
    goURL0(prompt, relative);
    if (tab == NULL)
    {
        if (buf != GetCurrentTab()->GetCurrentBuffer())
            GetCurrentTab()->DeleteBuffer(buf);
        else
            deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentTab()->GetCurrentBuffer())
    {
        // TODO:

        /* buf <- p <- ... <- Currentbuf = c */
        // BufferPtr c;
        // BufferPtr p;
        // c = GetCurrentTab()->GetCurrentBuffer();
        // p = prevBuffer(c, buf);
        // p->nextBuffer = NULL;
        // GetCurrentTab()->SetFirstBuffer(buf);
        deleteTab(GetCurrentTab());
        tab->SetFirstBuffer(buf);
        SetCurrentTab(tab);
        // for (buf = p; buf; buf = p)
        // {
        //     p = prevBuffer(c, buf);
        //     GetCurrentTab()->BufferPushBeforeCurrent(buf);
        // }
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

BufferPtr DownloadListBuffer()
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
    src = Strnew("<html><head><title>" DOWNLOAD_LIST_TITLE
                       "</title></head>\n<body><h1 align=center>" DOWNLOAD_LIST_TITLE "</h1>\n"
                       "<form method=internal action=download><hr>\n");
    for (d = LastDL; d != NULL; d = d->prev)
    {
        if (lstat(d->lock, &st))
            d->running = FALSE;
        src->Push("<pre>\n");
        src->Push(Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url),
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
                src->Push('#');
            while (l-- > 0)
                src->Push('_');
            src->Push('\n');
        }
        if ((d->running || d->err) && size < d->size)
            src->Push(Sprintf("  %s / %s bytes (%d%%)",
                              convert_size3(size), convert_size3(d->size),
                              (int)(100.0 * size / d->size)));
        else
            src->Push(Sprintf("  %s bytes loaded", convert_size3(size)));
        if (duration > 0)
        {
            rate = size / duration;
            src->Push(Sprintf("  %02d:%02d:%02d  rate %s/sec",
                              duration / (60 * 60), (duration / 60) % 60,
                              duration % 60, convert_size(rate, 1)));
            if (d->running && size < d->size && rate)
            {
                eta = (d->size - size) / rate;
                src->Push(Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60),
                                  (eta / 60) % 60, eta % 60));
            }
        }
        src->Push('\n');
        if (!d->running)
        {
            src->Push(Sprintf("<input type=submit name=ok%d value=OK>",
                              d->pid));
            switch (d->err)
            {
            case 0:
                if (size < d->size)
                    src->Push(" Download ended but probably not complete");
                else
                    src->Push(" Download complete");
                break;
            case 1:
                src->Push(" Error: could not open destination file");
                break;
            case 2:
                src->Push(" Error: could not write to file (disk full)");
                break;
            default:
                src->Push(" Error: unknown reason");
            }
        }
        else
            src->Push(Sprintf("<input type=submit name=stop%d value=STOP>",
                              d->pid));
        src->Push("\n</pre><hr>\n");
    }
    src->Push("</form></body></html>");
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
    restorePosition(GetCurrentTab()->GetCurrentBuffer(), &buf);
    GetCurrentTab()->GetCurrentBuffer()->undo = b;
    displayCurrentbuf(B_FORCE_REDRAW);
}

void save_buffer_position(BufferPtr buf)
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
void chkNMIDBuffer(BufferPtr buf)
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
void chkURLBuffer(BufferPtr buf)
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
    BufferPtr buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_INFO];
    wc_ces charset;

    if (buf == NULL)
        return;
    auto tab = GetCurrentTab();
    tab->DeleteBuffer(tab->GetCurrentBuffer());
    GetCurrentTab()->SetCurrentBuffer(buf);
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
        return;
    charset = GetCurrentTab()->GetCurrentBuffer()->document_charset;
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

    MapArea *a;
    int x, y;
    ParsedURL p_url;

    auto an = retrieveCurrentImg(GetCurrentTab()->GetCurrentBuffer());
    x = GetCurrentTab()->GetCurrentBuffer()->cursorX + GetCurrentTab()->GetCurrentBuffer()->rootX;
    y = GetCurrentTab()->GetCurrentBuffer()->cursorY + GetCurrentTab()->GetCurrentBuffer()->rootY;
    a = follow_map_menu(GetCurrentTab()->GetCurrentBuffer(), name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0')
    {

#ifndef MENU_MAP
        BufferPtr buf = follow_map_panel(GetCurrentTab()->GetCurrentBuffer(), name);

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
    p_url.Parse2(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
    pushHashHist(URLHist, p_url.ToStr()->ptr);
    if (check_target && open_tab_blank && a->target &&
        (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank")))
    {
        auto tab = CreateTabSetCurrent();
        BufferPtr buf = tab->GetCurrentBuffer();
        cmd_loadURL(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL(),
                    GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->ptr, NULL);
        if (buf != GetCurrentTab()->GetCurrentBuffer())
            GetCurrentTab()->DeleteBuffer(buf);
        else
            deleteTab(GetCurrentTab());
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }
    cmd_loadURL(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL(),
                GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->ptr, NULL);
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
        displayCurrentbuf(B_FORCE_REDRAW);
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
    char *str = s->ptr, *endp = &s->ptr[s->Size()], *bs = NULL;
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

    if (prop_size < s->Size())
    {
        prop_size = (s->Size() > LINELEN) ? s->Size() : LINELEN;
        prop_buffer = New_Reuse(Lineprop, prop_buffer, prop_size);
    }
    prop = prop_buffer;

    if (ShowEffect)
    {
        bs = (char *)memchr(str, '\b', s->Size());
#ifdef USE_ANSI_COLOR
        if (ocolor)
        {
            es = (char *)memchr(str, ESC_CODE, s->Size());
            if (es)
            {
                if (color_size < s->Size())
                {
                    color_size = (s->Size() > LINELEN) ? s->Size() : LINELEN;
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
            s = Strnew_size(s->Size());
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
            s->Push(sp, (int)(str - sp));
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
                    if (s->Size())
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
                    if (s->Size())
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
                    if (s->Size())
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
                            s->Pop(plen);
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
                    if (s->Size())
                    {

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
                            s->Pop(plen);
                            prop -= plen;
                            str++;
                        }
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
                s->Push((char *)str, plen);
            str += plen;
        }
        else
#endif
        {
            if (do_copy)
                s->Push((char)*str);
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
