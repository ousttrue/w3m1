
#include "commands.h"

extern "C"
{

#include "fm.h"
#include "indep.h"
#include "etc.h"
#include "file.h"
#include "form.h"
#include "myctype.h"
#include "public.h"
#include "regex.h"
#include "frame.h"
#include "dispatcher.h"
#include "mouse.h"
#include "menu.h"
#include "image.h"
#include "url.h"
#include "display.h"
#include "tab.h"
#include "cookie.h"
#include <signal.h>

void nulcmd()
{ /* do nothing */
}

void escmap()
{
    char c = getch();
    escKeyProc(c);
}

void escbmap()
{
    char c = getch();
    escbKeyProc(c);
}

void multimap()
{
    char c = getch();
    MultiKeyProc(c);
}
/* Move page forward */

void pgFore()
{
    if (vi_prec_num)
        nscroll(searchKeyNum() * (GetCurrentbuf()->LINES - 1), B_NORMAL);
    else
        nscroll(prec_num() ? searchKeyNum() : searchKeyNum() * (GetCurrentbuf()->LINES - 1), prec_num() ? B_SCROLL : B_NORMAL);
}
/* Move page backward */

void pgBack()
{
    if (vi_prec_num)
        nscroll(-searchKeyNum() * (GetCurrentbuf()->LINES - 1), B_NORMAL);
    else
        nscroll(-(prec_num() ? searchKeyNum() : searchKeyNum() * (GetCurrentbuf()->LINES - 1)), prec_num() ? B_SCROLL : B_NORMAL);
}
/* 1 line up */

void lup1()
{
    nscroll(searchKeyNum(), B_SCROLL);
}
/* 1 line down */

void ldown1()
{
    nscroll(-searchKeyNum(), B_SCROLL);
}
/* move cursor position to the center of screen */

void ctrCsrV()
{
    int offsety;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    offsety = GetCurrentbuf()->LINES / 2 - GetCurrentbuf()->cursorY;
    if (offsety != 0)
    {
#if 0
        GetCurrentbuf()->currentLine = lineSkip(GetCurrentbuf(),
                                           GetCurrentbuf()->currentLine, offsety,
                                           FALSE);
#endif
        GetCurrentbuf()->topLine =
            lineSkip(GetCurrentbuf(), GetCurrentbuf()->topLine, -offsety, FALSE);
        arrangeLine(GetCurrentbuf());
        displayBuffer(GetCurrentbuf(), B_NORMAL);
    }
}

void ctrCsrH()
{
    int offsetx;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    offsetx = GetCurrentbuf()->cursorX - GetCurrentbuf()->COLS / 2;
    if (offsetx != 0)
    {
        columnSkip(GetCurrentbuf(), offsetx);
        arrangeCursor(GetCurrentbuf());
        displayBuffer(GetCurrentbuf(), B_NORMAL);
    }
}
/* Redraw screen */

void rdrwSc()
{
    clear();
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* Search regular expression forward */

void srchfor()
{
    srch(forwardSearch, "Forward: ");
}

void isrchfor()
{
    isrch(forwardSearch, "I-search: ");
}
/* Search regular expression backward */

void srchbak()
{
    srch(backwardSearch, "Backward: ");
}

void isrchbak()
{
    isrch(backwardSearch, "I-search backward: ");
}
/* Search next matching */

void srchnxt()
{
    srch_nxtprv(0);
}
/* Search previous matching */

void srchprv()
{
    srch_nxtprv(1);
}
/* Shift screen left */

void shiftl()
{
    int column;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    column = GetCurrentbuf()->currentColumn;
    columnSkip(GetCurrentbuf(), searchKeyNum() * (-GetCurrentbuf()->COLS + 1) + 1);
    shiftvisualpos(GetCurrentbuf(), GetCurrentbuf()->currentColumn - column);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* Shift screen right */

void shiftr()
{
    int column;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    column = GetCurrentbuf()->currentColumn;
    columnSkip(GetCurrentbuf(), searchKeyNum() * (GetCurrentbuf()->COLS - 1) - 1);
    shiftvisualpos(GetCurrentbuf(), GetCurrentbuf()->currentColumn - column);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void col1R()
{
    Buffer *buf = GetCurrentbuf();
    Line *l = buf->currentLine;
    int j, column, n = searchKeyNum();
    if (l == NULL)
        return;
    for (j = 0; j < n; j++)
    {
        column = buf->currentColumn;
        columnSkip(GetCurrentbuf(), 1);
        if (column == buf->currentColumn)
            break;
        shiftvisualpos(GetCurrentbuf(), 1);
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void col1L()
{
    Buffer *buf = GetCurrentbuf();
    Line *l = buf->currentLine;
    int j, n = searchKeyNum();
    if (l == NULL)
        return;
    for (j = 0; j < n; j++)
    {
        if (buf->currentColumn == 0)
            break;
        columnSkip(GetCurrentbuf(), -1);
        shiftvisualpos(GetCurrentbuf(), -1);
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void setEnv()
{
    char *env;
    char *var, *value;
    ClearCurrentKeyData();
    env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL)
    {
        if (env != NULL && *env != '\0')
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist("Set environ: ", env, TextHist);
        if (env == NULL || *env == '\0')
        {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
    }
    if ((value = strchr(env, '=')) != NULL && value > env)
    {
        var = allocStr(env, value - env);
        value++;
        set_environ(var, value);
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void pipeBuf()
{
    Buffer *buf;
    char *cmd, *tmpf;
    FILE *f;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0')
    {
        /* FIXME: gettextize? */
        cmd = inputLineHist("Pipe buffer to: ", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0')
    {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    f = fopen(tmpf, "w");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_message(Sprintf("Can't save buffer to %s", cmd)->ptr, TRUE);
        return;
    }
    saveBuffer(GetCurrentbuf(), f, TRUE);
    fclose(f);
    buf = getpipe(myExtCommand(cmd, shell_quote(tmpf), TRUE)->ptr);
    if (buf == NULL)
    {
        disp_message("Execution failed", TRUE);
        return;
    }
    else
    {
        buf->filename = cmd;
        buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME,
                                  conv_from_system(cmd))
                              ->ptr;
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        buf->currentURL.file = "-";
        pushBuffer(buf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* Execute shell command and read output ac pipe. */

void pipesh()
{
    Buffer *buf;
    char *cmd;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0')
    {
        cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0')
    {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    buf = getpipe(cmd);
    if (buf == NULL)
    {
        disp_message("Execution failed", TRUE);
        return;
    }
    else
    {
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* Execute shell command and load entire output to buffer */

void readsh()
{
    Buffer *buf;
    char *cmd;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0')
    {
        cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0')
    {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    MySignalHandler prevtrap = mySignal(SIGINT, intTrap);
    crmode();
    buf = getshell(cmd);
    mySignal(SIGINT, prevtrap);
    term_raw();
    if (buf == NULL)
    {
        /* FIXME: gettextize? */
        disp_message("Execution failed", TRUE);
        return;
    }
    else
    {
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* Execute shell command */

void execsh()
{
    char *cmd;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0')
    {
        cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd != NULL && *cmd != '\0')
    {
        fmTerm();
        printf("\n");
        system(cmd);
        /* FIXME: gettextize? */
        printf("\n[Hit any key]");
        fflush(stdout);
        fmInit();
        getch();
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* Load file */

void ldfile()
{
    char *fn;
    fn = searchKeyData();
    if (fn == NULL || *fn == '\0')
    {
        /* FIXME: gettextize? */
        fn = inputFilenameHist("(Load)Filename? ", NULL, LoadHist);
    }
    if (fn != NULL)
        fn = conv_to_system(fn);
    if (fn == NULL || *fn == '\0')
    {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    cmd_loadfile(fn);
}
/* Load help file */

void ldhelp()
{
#ifdef USE_HELP_CGI
    char *lang;
    int n;
    Str tmp;
    lang = AcceptLang;
    n = strcspn(lang, ";, \t");
    tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
                  Str_form_quote(Strnew_charp(w3m_version))->ptr,
                  Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    cmd_loadURL(tmp->ptr, NULL, NO_REFERER, NULL);
#else
    cmd_loadURL(helpFile(HELP_FILE), NULL, NO_REFERER, NULL);
#endif
}

void movL()
{
    _movL(GetCurrentbuf()->COLS / 2);
}

void movL1()
{
    _movL(1);
}

void movD()
{
    _movD((GetCurrentbuf()->LINES + 1) / 2);
}

void movD1()
{
    _movD(1);
}

void movU()
{
    _movU((GetCurrentbuf()->LINES + 1) / 2);
}

void movU1()
{
    _movU(1);
}

void movR()
{
    _movR(GetCurrentbuf()->COLS / 2);
}

void movR1()
{
    _movR(1);
}

void movLW()
{
    char *lb;
    Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    for (i = 0; i < n; i++)
    {
        pline = GetCurrentbuf()->currentLine;
        ppos = GetCurrentbuf()->pos;
        if (prev_nonnull_line(GetCurrentbuf()->currentLine) < 0)
            goto end;
        while (1)
        {
            l = GetCurrentbuf()->currentLine;
            lb = l->lineBuf;
            while (GetCurrentbuf()->pos > 0)
            {
                int tmp = GetCurrentbuf()->pos;
                prevChar(&tmp, l);
                if (is_wordchar(getChar(&lb[tmp])))
                    break;
                GetCurrentbuf()->pos = tmp;
            }
            if (GetCurrentbuf()->pos > 0)
                break;
            if (prev_nonnull_line(GetCurrentbuf()->currentLine->prev) < 0)
            {
                GetCurrentbuf()->currentLine = pline;
                GetCurrentbuf()->pos = ppos;
                goto end;
            }
            GetCurrentbuf()->pos = GetCurrentbuf()->currentLine->len;
        }
        l = GetCurrentbuf()->currentLine;
        lb = l->lineBuf;
        while (GetCurrentbuf()->pos > 0)
        {
            int tmp = GetCurrentbuf()->pos;
            prevChar(&tmp, l);
            if (!is_wordchar(getChar(&lb[tmp])))
                break;
            GetCurrentbuf()->pos = tmp;
        }
    }
end:
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void movRW()
{
    char *lb;
    Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    for (i = 0; i < n; i++)
    {
        pline = GetCurrentbuf()->currentLine;
        ppos = GetCurrentbuf()->pos;
        if (next_nonnull_line(GetCurrentbuf()->currentLine) < 0)
            goto end;
        l = GetCurrentbuf()->currentLine;
        lb = l->lineBuf;
        while (GetCurrentbuf()->pos < l->len &&
               is_wordchar(getChar(&lb[GetCurrentbuf()->pos])))
            nextChar(&GetCurrentbuf()->pos, l);
        while (1)
        {
            while (GetCurrentbuf()->pos < l->len &&
                   !is_wordchar(getChar(&lb[GetCurrentbuf()->pos])))
                nextChar(&GetCurrentbuf()->pos, l);
            if (GetCurrentbuf()->pos < l->len)
                break;
            if (next_nonnull_line(GetCurrentbuf()->currentLine->next) < 0)
            {
                GetCurrentbuf()->currentLine = pline;
                GetCurrentbuf()->pos = ppos;
                goto end;
            }
            GetCurrentbuf()->pos = 0;
            l = GetCurrentbuf()->currentLine;
            lb = l->lineBuf;
        }
    }
end:
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* Quit */

void quitfm()
{
    _quitfm(FALSE);
}
/* Question and Quit */

void qquitfm()
{
    _quitfm(confirm_on_quit);
}
/* Select buffer */

void selBuf()
{
    Buffer *buf;
    int ok;
    char cmd;
    ok = FALSE;
    do
    {
        buf = selectBuffer(GetFirstbuf(), GetCurrentbuf(), &cmd);
        switch (cmd)
        {
        case 'B':
            ok = TRUE;
            break;
        case '\n':
        case ' ':
            SetCurrentbuf(buf);
            ok = TRUE;
            break;
        case 'D':
            delBuffer(buf);
            if (GetFirstbuf() == NULL)
            {
                /* No more buffer */
                SetFirstbuf(nullBuffer());
                SetCurrentbuf(GetFirstbuf());
            }
            break;
        case 'q':
            qquitfm();
            break;
        case 'Q':
            quitfm();
            break;
        }
    } while (!ok);

    for (buf = GetFirstbuf(); buf != NULL; buf = buf->nextBuffer)
    {
        if (buf == GetCurrentbuf())
            continue;
#ifdef USE_IMAGE
        deleteImage(buf);
#endif
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* Suspend (on BSD), or run interactive shell (on SysV) */

void susp()
{
#ifndef SIGSTOP
    char *shell;
#endif /* not SIGSTOP */
    move(LASTLINE, 0);
    clrtoeolx();
    refresh();
    fmTerm();
#ifndef SIGSTOP
    shell = getenv("SHELL");
    if (shell == NULL)
        shell = "/bin/sh";
    system(shell);
#else  /* SIGSTOP */
    kill((pid_t)0, SIGSTOP);
#endif /* SIGSTOP */
    fmInit();
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void goLine()
{
    char *str = searchKeyData();
    if (prec_num())
        _goLine("^");
    else if (str)
        _goLine(str);
    else
        /* FIXME: gettextize? */
        _goLine(inputStr("Goto line: ", ""));
}

void goLineF()
{
    _goLine("^");
}

void goLineL()
{
    _goLine("$");
}
/* Go to the beginning of the line */

void linbeg()
{
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    while (GetCurrentbuf()->currentLine->prev && GetCurrentbuf()->currentLine->bpos)
        cursorUp0(GetCurrentbuf(), 1);
    GetCurrentbuf()->pos = 0;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* Go to the bottom of the line */

void linend()
{
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    while (GetCurrentbuf()->currentLine->next && GetCurrentbuf()->currentLine->next->bpos)
        cursorDown0(GetCurrentbuf(), 1);
    GetCurrentbuf()->pos = GetCurrentbuf()->currentLine->len - 1;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* Run editor on the current buffer */

void editBf()
{
    char *fn = GetCurrentbuf()->filename;
    Str cmd;
    if (fn == NULL || GetCurrentbuf()->pagerSource != NULL ||                                     /* Behaving as a pager */
        (GetCurrentbuf()->type == NULL && GetCurrentbuf()->edit == NULL) ||                            /* Reading shell */
        GetCurrentbuf()->real_scheme != SCM_LOCAL || !strcmp(GetCurrentbuf()->currentURL.file, "-") || /* file is std input  */
        GetCurrentbuf()->bufferprop & BP_FRAME)
    { /* Frame */
        disp_err_message("Can't edit other than local file", TRUE);
        return;
    }
    if (GetCurrentbuf()->edit)
        cmd = unquote_mailcap(GetCurrentbuf()->edit, GetCurrentbuf()->real_type, fn,
                              checkHeader(GetCurrentbuf(), "Content-Type:"), NULL);
    else
        cmd = myEditor(Editor, shell_quote(fn),
                       cur_real_linenumber(GetCurrentbuf()));
    fmTerm();
    system(cmd->ptr);
    fmInit();
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
    reload();
}
/* Run editor on the current screen */

void editScr()
{
    char *tmpf;
    FILE *f;
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    f = fopen(tmpf, "w");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_err_message(Sprintf("Can't open %s", tmpf)->ptr, TRUE);
        return;
    }
    saveBuffer(GetCurrentbuf(), f, TRUE);
    fclose(f);
    fmTerm();
    system(myEditor(Editor, shell_quote(tmpf),
                    cur_real_linenumber(GetCurrentbuf()))
               ->ptr);
    fmInit();
    unlink(tmpf);
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* Set / unset mark */

void _mark()
{
    Line *l;
    if (!use_mark)
        return;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    l = GetCurrentbuf()->currentLine;
    l->propBuf[GetCurrentbuf()->pos] ^= PE_MARK;
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* Go to next mark */

void nextMk()
{
    Line *l;
    int i;
    if (!use_mark)
        return;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    i = GetCurrentbuf()->pos + 1;
    l = GetCurrentbuf()->currentLine;
    if (i >= l->len)
    {
        i = 0;
        l = l->next;
    }
    while (l != NULL)
    {
        for (; i < l->len; i++)
        {
            if (l->propBuf[i] & PE_MARK)
            {
                GetCurrentbuf()->currentLine = l;
                GetCurrentbuf()->pos = i;
                arrangeCursor(GetCurrentbuf());
                displayBuffer(GetCurrentbuf(), B_NORMAL);
                return;
            }
        }
        l = l->next;
        i = 0;
    }
    /* FIXME: gettextize? */
    disp_message("No mark exist after here", TRUE);
}
/* Go to previous mark */

void prevMk()
{
    Line *l;
    int i;
    if (!use_mark)
        return;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    i = GetCurrentbuf()->pos - 1;
    l = GetCurrentbuf()->currentLine;
    if (i < 0)
    {
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    while (l != NULL)
    {
        for (; i >= 0; i--)
        {
            if (l->propBuf[i] & PE_MARK)
            {
                GetCurrentbuf()->currentLine = l;
                GetCurrentbuf()->pos = i;
                arrangeCursor(GetCurrentbuf());
                displayBuffer(GetCurrentbuf(), B_NORMAL);
                return;
            }
        }
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    /* FIXME: gettextize? */
    disp_message("No mark exist before here", TRUE);
}
/* Mark place to which the regular expression matches */

void reMark()
{
    Line *l;
    char *str;
    char *p, *p1, *p2;
    if (!use_mark)
        return;
    str = searchKeyData();
    if (str == NULL || *str == '\0')
    {
        str = inputStrHist("(Mark)Regexp: ", MarkString(), TextHist);
        if (str == NULL || *str == '\0')
        {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
    }
    str = conv_search_string(str, DisplayCharset);
    if ((str = regexCompile(str, 1)) != NULL)
    {
        disp_message(str, TRUE);
        return;
    }
    SetMarkString(str);
    for (l = GetCurrentbuf()->firstLine; l != NULL; l = l->next)
    {
        p = l->lineBuf;
        for (;;)
        {
            if (regexMatch(p, &l->lineBuf[l->len] - p, p == l->lineBuf) == 1)
            {
                matchedPosition(&p1, &p2);
                l->propBuf[p1 - l->lineBuf] |= PE_MARK;
                p = p2;
            }
            else
                break;
        }
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* view inline image */

void followI()
{
    Line *l;
    Anchor *a;
    Buffer *buf;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    l = GetCurrentbuf()->currentLine;
    a = retrieveCurrentImg(GetCurrentbuf());
    if (a == NULL)
        return;
    /* FIXME: gettextize? */
    message(Sprintf("loading %s", a->url)->ptr, 0, 0);
    refresh();
    buf = loadGeneralFile(a->url, baseURL(GetCurrentbuf()), NULL, 0, NULL);
    if (buf == NULL)
    {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("Can't load %s", a->url)->ptr;
        disp_err_message(emsg, FALSE);
    }
    else if (buf != NO_BUFFER)
    {
        pushBuffer(buf);
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* submit form */

void submitForm()
{
    _followForm(TRUE);
}
/* go to the top anchor */

void topA()
{
    HmarkerList *hl = GetCurrentbuf()->hmarklist;
    BufferPoint *po;
    Anchor *an;
    int hseq = 0;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;
    if (prec_num() > hl->nmark)
        hseq = hl->nmark - 1;
    else if (prec_num() > 0)
        hseq = prec_num() - 1;
    do
    {
        if (hseq >= hl->nmark)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(GetCurrentbuf()->href, po->line, po->pos);
        if (an == NULL)
            an = retrieveAnchor(GetCurrentbuf()->formitem, po->line, po->pos);
        hseq++;
    } while (an == NULL);
    gotoLine(GetCurrentbuf(), po->line);
    GetCurrentbuf()->pos = po->pos;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* go to the last anchor */

void lastA()
{
    HmarkerList *hl = GetCurrentbuf()->hmarklist;
    BufferPoint *po;
    Anchor *an;
    int hseq;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;
    if (prec_num() >= hl->nmark)
        hseq = 0;
    else if (prec_num() > 0)
        hseq = hl->nmark - prec_num();
    else
        hseq = hl->nmark - 1;
    do
    {
        if (hseq < 0)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(GetCurrentbuf()->href, po->line, po->pos);
        if (an == NULL)
            an = retrieveAnchor(GetCurrentbuf()->formitem, po->line, po->pos);
        hseq--;
    } while (an == NULL);
    gotoLine(GetCurrentbuf(), po->line);
    GetCurrentbuf()->pos = po->pos;
    arrangeCursor(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* go to the next anchor */

void nextA()
{
    _nextA(FALSE);
}
/* go to the previous anchor */

void prevA()
{
    _prevA(FALSE);
}
/* go to the next visited anchor */

void nextVA()
{
    _nextA(TRUE);
}
/* go to the previous visited anchor */

void prevVA()
{
    _prevA(TRUE);
}
/* follow HREF link */

void followA()
{
    Line *l;
    Anchor *a;
    ParsedURL u;
#ifdef USE_IMAGE
    int x = 0, y = 0, map = 0;
#endif
    char *url;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    l = GetCurrentbuf()->currentLine;
#ifdef USE_IMAGE
    a = retrieveCurrentImg(GetCurrentbuf());
    if (a && a->image && a->image->map)
    {
        _followForm(FALSE);
        return;
    }
    if (a && a->image && a->image->ismap)
    {
        getMapXY(GetCurrentbuf(), a, &x, &y);
        map = 1;
    }
#else
    a = retrieveCurrentMap(GetCurrentbuf());
    if (a)
    {
        _followForm(FALSE);
        return;
    }
#endif
    a = retrieveCurrentAnchor(GetCurrentbuf());
    if (a == NULL)
    {
        _followForm(FALSE);
        return;
    }
    if (*a->url == '#')
    { /* index within this buffer */
        gotoLabel(a->url + 1);
        return;
    }
    parseURL2(a->url, &u, baseURL(GetCurrentbuf()));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&GetCurrentbuf()->currentURL)) == 0)
    {
        /* index within this buffer */
        if (u.label)
        {
            gotoLabel(u.label);
            return;
        }
    }
    if (handleMailto(a->url))
        return;
#if 0
    else if (!strncasecmp(a->url, "news:", 5) && strchr(a->url, '@') == NULL) {
        /* news:newsgroup is not supported */
        /* FIXME: gettextize? */
        disp_err_message("news:newsgroup_name is not supported", TRUE);
        return;
    }
#endif /* USE_NNTP */
    url = a->url;
#ifdef USE_IMAGE
    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;
#endif
    if (check_target && open_tab_blank && a->target &&
        (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank")))
    {
        Buffer *buf;
        _newT();
        buf = GetCurrentbuf();
        loadLink(url, a->target, a->referer, NULL);
        if (buf != GetCurrentbuf())
            delBuffer(buf);
        else
            deleteTab(GetCurrentTab());
        displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
        return;
    }
    loadLink(url, a->target, a->referer, NULL);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* go to the next left anchor */

void nextL()
{
    nextX(-1, 0);
}
/* go to the next left-up anchor */

void nextLU()
{
    nextX(-1, -1);
}
/* go to the next right anchor */

void nextR()
{
    nextX(1, 0);
}
/* go to the next right-down anchor */

void nextRD()
{
    nextX(1, 1);
}
/* go to the next downward anchor */

void nextD()
{
    nextY(1);
}
/* go to the next upward anchor */

void nextU()
{
    nextY(-1);
}
/* go to the next bufferr */

void nextBf()
{
    Buffer *buf;
    int i;
    for (i = 0; i < PREC_NUM(); i++)
    {
        buf = prevBuffer(GetFirstbuf(), GetCurrentbuf());
        if (!buf)
        {
            if (i == 0)
                return;
            break;
        }
        SetCurrentbuf(buf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* go to the previous bufferr */

void prevBf()
{
    Buffer *buf;
    int i;
    for (i = 0; i < PREC_NUM(); i++)
    {
        buf = GetCurrentbuf()->nextBuffer;
        if (!buf)
        {
            if (i == 0)
                return;
            break;
        }
        SetCurrentbuf(buf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* delete current buffer and back to the previous buffer */

void backBf()
{
    Buffer *buf = GetCurrentbuf()->linkBuffer[LB_N_FRAME];
    if (!checkBackBuffer(GetCurrentbuf()))
    {
        if (close_tab_back && GetTabCount() >= 1)
        {
            deleteTab(GetCurrentTab());
            displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
        }
        else
            /* FIXME: gettextize? */
            disp_message("Can't back...", TRUE);
        return;
    }
    delBuffer(GetCurrentbuf());
    if (buf)
    {
        if (buf->frameQ)
        {
            struct frameset *fs;
            long linenumber = buf->frameQ->linenumber;
            long top = buf->frameQ->top_linenumber;
            int pos = buf->frameQ->pos;
            int currentColumn = buf->frameQ->currentColumn;
            AnchorList *formitem = buf->frameQ->formitem;
            fs = popFrameTree(&(buf->frameQ));
            deleteFrameSet(buf->frameset);
            buf->frameset = fs;
            if (buf == GetCurrentbuf())
            {
                rFrame();
                GetCurrentbuf()->topLine = lineSkip(GetCurrentbuf(),
                                               GetCurrentbuf()->firstLine, top - 1,
                                               FALSE);
                gotoLine(GetCurrentbuf(), linenumber);
                GetCurrentbuf()->pos = pos;
                GetCurrentbuf()->currentColumn = currentColumn;
                arrangeCursor(GetCurrentbuf());
                formResetBuffer(GetCurrentbuf(), formitem);
            }
        }
        else if (RenderFrame && buf == GetCurrentbuf())
        {
            delBuffer(GetCurrentbuf());
        }
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void deletePrevBuf()
{
    Buffer *buf = GetCurrentbuf()->nextBuffer;
    if (buf)
        delBuffer(buf);
}

void goURL()
{
    goURL0("Goto URL: ", FALSE);
}

void gorURL()
{
    goURL0("Goto relative URL: ", TRUE);
}
/* load bookmark */

void ldBmark()
{
    cmd_loadURL(BookmarkFile, NULL, NO_REFERER, NULL);
}
/* Add current to bookmark */

void adBmark()
{
    Str tmp;
    FormList *request;
    tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
#ifdef USE_M17N
                  "&charset=%s"
#endif
                  ,
                  (Str_form_quote(localCookie()))->ptr,
                  (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
                  (Str_form_quote(parsedURL2Str(&GetCurrentbuf()->currentURL)))->ptr,
#ifdef USE_M17N
                  (Str_form_quote(wc_conv_strict(GetCurrentbuf()->buffername,
                                                 InnerCharset,
                                                 BookmarkCharset)))
                      ->ptr,
                  wc_ces_to_charset(BookmarkCharset));
#else
                  (Str_form_quote(Strnew_charp(GetCurrentbuf()->buffername)))->ptr);
#endif
    request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    request->body = tmp->ptr;
    request->length = tmp->length;
    cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, NO_REFERER,
                request);
}
/* option setting */

void ldOpt()
{
    cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}
/* error message list */

void msgs()
{
    cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}
/* page info */

void pginfo()
{
    Buffer *buf;
    if ((buf = GetCurrentbuf()->linkBuffer[LB_N_INFO]) != NULL) {
        SetCurrentbuf(buf);
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    if ((buf = GetCurrentbuf()->linkBuffer[LB_INFO]) != NULL)
        delBuffer(buf);
    buf = page_info_panel(GetCurrentbuf());
    cmd_loadBuffer(buf, BP_NORMAL, LB_INFO);
}
/* link menu */

void linkMn()
{
    LinkList *l = link_menu(GetCurrentbuf());
    ParsedURL p_url;
    if (!l || !l->url)
        return;
    if (*(l->url) == '#') {
        gotoLabel(l->url + 1);
        return;
    }
    parseURL2(l->url, &p_url, baseURL(GetCurrentbuf()));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(l->url, baseURL(GetCurrentbuf()),
                parsedURL2Str(&GetCurrentbuf()->currentURL)->ptr, NULL);
}
/* accesskey */

void accessKey()
{
    anchorMn(accesskey_menu, TRUE);
}
/* set an option */

void setOpt()
{
    char *opt;
    ClearCurrentKeyData();	/* not allowed in w3m-control: */
    opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
        if (opt != NULL && *opt != '\0') {
            char *v = get_param_option(opt);
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist("Set option: ", opt, TextHist);
        if (opt == NULL || *opt == '\0') {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
    }
    if (set_param_option(opt))
        sync_with_option();
    displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
}
/* list menu */

void listMn()
{
    anchorMn(list_menu, TRUE);
}

void movlistMn()
{
    anchorMn(list_menu, FALSE);
}
/* link,anchor,image list */

void linkLst()
{
    Buffer *buf;
    buf = link_list_panel(GetCurrentbuf());
    if (buf != NULL) {
#ifdef USE_M17N
        buf->document_charset = GetCurrentbuf()->document_charset;
#endif
        cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
    }
}
/* cookie list */

void cooLst()
{
    Buffer *buf;
    buf = cookie_list_panel();
    if (buf != NULL)
        cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
}
/* History page */

void ldHist()
{
    cmd_loadBuffer(historyBuffer(URLHist), BP_NO_URL, LB_NOLINK);
}
/* download HREF link */

void svA()
{
    ClearCurrentKeyData();	/* not allowed in w3m-control: */
    do_download = TRUE;
    followA();
    do_download = FALSE;
}
/* download IMG link */

void svI()
{
    ClearCurrentKeyData();	/* not allowed in w3m-control: */
    do_download = TRUE;
    followI();
    do_download = FALSE;
}
/* save buffer */

void svBuf()
{
    char *qfile = NULL, *file;
    FILE *f;
    int is_pipe;
    ClearCurrentKeyData();	/* not allowed in w3m-control: */
    file = searchKeyData();
    if (file == NULL || *file == '\0') {
        /* FIXME: gettextize? */
        qfile = inputLineHist("Save buffer to: ", NULL, IN_COMMAND, SaveHist);
        if (qfile == NULL || *qfile == '\0') {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file);
    if (*file == '|') {
        is_pipe = TRUE;
        f = popen(file + 1, "w");
    }
    else {
        if (qfile) {
            file = unescape_spaces(Strnew_charp(qfile))->ptr;
            file = conv_to_system(file);
        }
        file = expandPath(file);
        if (checkOverWrite(file) < 0) {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
        f = fopen(file, "w");
        is_pipe = FALSE;
    }
    if (f == NULL) {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
        disp_err_message(emsg, TRUE);
        return;
    }
    saveBuffer(GetCurrentbuf(), f, TRUE);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* save source */

void svSrc()
{
    char *file;
    if (GetCurrentbuf()->sourcefile == NULL)
        return;
    ClearCurrentKeyData();	/* not allowed in w3m-control: */
    PermitSaveToPipe = TRUE;
    if (GetCurrentbuf()->real_scheme == SCM_LOCAL)
        file = conv_from_system(guess_save_name(NULL,
                                                GetCurrentbuf()->currentURL.
                                                real_file));
    else
        file = guess_save_name(GetCurrentbuf(), GetCurrentbuf()->currentURL.file);
    doFileCopy(GetCurrentbuf()->sourcefile, file);
    PermitSaveToPipe = FALSE;
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* peek URL */

void peekURL()
{
    _peekURL(0);
}
/* peek URL of image */

void peekIMG()
{
    _peekURL(1);
}

void curURL()
{
    static Str s = NULL;
#ifdef USE_M17N
    static Lineprop *p = NULL;
    Lineprop *pp;
#endif
    static int offset = 0, n;
    if (GetCurrentbuf()->bufferprop & BP_INTERNAL)
        return;
    if (CurrentKey() == PrevKey() && s != NULL) {
        if (s->length - offset >= COLS)
            offset++;
        else if (s->length <= offset)	/* bug ? */
            offset = 0;
    }
    else {
        offset = 0;
        s = currentURL();
        if (DecodeURL)
            s = Strnew_charp(url_unquote_conv(s->ptr, 0));
#ifdef USE_M17N
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->length);
        bcopy((void *)pp, (void *)p, s->length * sizeof(Lineprop));
#endif
    }
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
#ifdef USE_M17N
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
#endif
    disp_message_nomouse(&s->ptr[offset], TRUE);
}
/* view HTML source */

void vwSrc()
{
    Buffer *buf;
    if (GetCurrentbuf()->type == NULL || GetCurrentbuf()->bufferprop & BP_FRAME)
        return;
    if ((buf = GetCurrentbuf()->linkBuffer[LB_SOURCE]) != NULL ||
        (buf = GetCurrentbuf()->linkBuffer[LB_N_SOURCE]) != NULL) {
        SetCurrentbuf(buf);
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    if (GetCurrentbuf()->sourcefile == NULL) {
        if (GetCurrentbuf()->pagerSource &&
            !strcasecmp(GetCurrentbuf()->type, "text/plain")) {
#ifdef USE_M17N
            wc_ces old_charset;
            wc_bool old_fix_width_conv;
#endif
            FILE *f;
            Str tmpf = tmpfname(TMPF_SRC, NULL);
            f = fopen(tmpf->ptr, "w");
            if (f == NULL)
                return;
#ifdef USE_M17N
            old_charset = DisplayCharset;
            old_fix_width_conv = WcOption.fix_width_conv;
            DisplayCharset = (GetCurrentbuf()->document_charset != WC_CES_US_ASCII)
                ? GetCurrentbuf()->document_charset : 0;
            WcOption.fix_width_conv = WC_FALSE;
#endif
            saveBufferBody(GetCurrentbuf(), f, TRUE);
#ifdef USE_M17N
            DisplayCharset = old_charset;
            WcOption.fix_width_conv = old_fix_width_conv;
#endif
            fclose(f);
            GetCurrentbuf()->sourcefile = tmpf->ptr;
        }
        else {
            return;
        }
    }
    buf = newBuffer(INIT_BUFFER_WIDTH);
    if (is_html_type(GetCurrentbuf()->type)) {
        buf->type = "text/plain";
        if (GetCurrentbuf()->real_type &&
            is_html_type(GetCurrentbuf()->real_type))
            buf->real_type = "text/plain";
        else
            buf->real_type = GetCurrentbuf()->real_type;
        buf->buffername = Sprintf("source of %s", GetCurrentbuf()->buffername)->ptr;
        buf->linkBuffer[LB_N_SOURCE] = GetCurrentbuf();
        GetCurrentbuf()->linkBuffer[LB_SOURCE] = buf;
    }
    else if (!strcasecmp(GetCurrentbuf()->type, "text/plain")) {
        buf->type = "text/html";
        if (GetCurrentbuf()->real_type &&
            !strcasecmp(GetCurrentbuf()->real_type, "text/plain"))
            buf->real_type = "text/html";
        else
            buf->real_type = GetCurrentbuf()->real_type;
        buf->buffername = Sprintf("HTML view of %s",
                                  GetCurrentbuf()->buffername)->ptr;
        buf->linkBuffer[LB_SOURCE] = GetCurrentbuf();
        GetCurrentbuf()->linkBuffer[LB_N_SOURCE] = buf;
    }
    else {
        return;
    }
    buf->currentURL = GetCurrentbuf()->currentURL;
    buf->real_scheme = GetCurrentbuf()->real_scheme;
    buf->filename = GetCurrentbuf()->filename;
    buf->sourcefile = GetCurrentbuf()->sourcefile;
    buf->header_source = GetCurrentbuf()->header_source;
    buf->search_header = GetCurrentbuf()->search_header;
#ifdef USE_M17N
    buf->document_charset = GetCurrentbuf()->document_charset;
#endif
    buf->clone = GetCurrentbuf()->clone;
    (*buf->clone)++;
    buf->need_reshape = TRUE;
    reshapeBuffer(buf);
    pushBuffer(buf);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
/* reload */

void reload()
{
    Buffer *buf, *fbuf = NULL, sbuf;
#ifdef USE_M17N
    wc_ces old_charset;
#endif
    Str url;
    FormList *request;
    int multipart;
    if (GetCurrentbuf()->bufferprop & BP_INTERNAL) {
        if (!strcmp(GetCurrentbuf()->buffername, DOWNLOAD_LIST_TITLE)) {
            ldDL();
            return;
        }
        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    if (GetCurrentbuf()->currentURL.scheme == SCM_LOCAL &&
        !strcmp(GetCurrentbuf()->currentURL.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't reload stdin", TRUE);
        return;
    }
    copyBuffer(&sbuf, GetCurrentbuf());
    if (GetCurrentbuf()->bufferprop & BP_FRAME &&
        (fbuf = GetCurrentbuf()->linkBuffer[LB_N_FRAME])) {
        if (fmInitialized) {
            message("Rendering frame", 0, 0);
            refresh();
        }
        if (!(buf = renderFrame(fbuf, 1))) {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
        if (fbuf->linkBuffer[LB_FRAME]) {
            if (buf->sourcefile &&
                fbuf->linkBuffer[LB_FRAME]->sourcefile &&
                !strcmp(buf->sourcefile,
                        fbuf->linkBuffer[LB_FRAME]->sourcefile))
                fbuf->linkBuffer[LB_FRAME]->sourcefile = NULL;
            delBuffer(fbuf->linkBuffer[LB_FRAME]);
        }
        fbuf->linkBuffer[LB_FRAME] = buf;
        buf->linkBuffer[LB_N_FRAME] = fbuf;
        pushBuffer(buf);
        SetCurrentbuf(buf);
        if (GetCurrentbuf()->firstLine) {
            COPY_BUFROOT(GetCurrentbuf(), &sbuf);
            restorePosition(GetCurrentbuf(), &sbuf);
        }
        displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
        return;
    }
    else if (GetCurrentbuf()->frameset != NULL)
        fbuf = GetCurrentbuf()->linkBuffer[LB_FRAME];
    multipart = 0;
    if (GetCurrentbuf()->form_submit) {
        request = GetCurrentbuf()->form_submit->parent;
        if (request->method == FORM_METHOD_POST
            && request->enctype == FORM_ENCTYPE_MULTIPART) {
            Str query;
            struct stat st;
            multipart = 1;
            query_from_followform(&query, GetCurrentbuf()->form_submit, multipart);
            stat(request->body, &st);
            request->length = st.st_size;
        }
    }
    else {
        request = NULL;
    }
    url = parsedURL2Str(&GetCurrentbuf()->currentURL);
    /* FIXME: gettextize? */
    message("Reloading...", 0, 0);
    refresh();
#ifdef USE_M17N
    old_charset = DocumentCharset;
    if (GetCurrentbuf()->document_charset != WC_CES_US_ASCII)
        DocumentCharset = GetCurrentbuf()->document_charset;
#endif
    SearchHeader = GetCurrentbuf()->search_header;
    DefaultType = GetCurrentbuf()->real_type;
    buf = loadGeneralFile(url->ptr, NULL, NO_REFERER, RG_NOCACHE, request);
#ifdef USE_M17N
    DocumentCharset = old_charset;
#endif
    SearchHeader = FALSE;
    DefaultType = NULL;
    if (multipart)
        unlink(request->body);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    else if (buf == NO_BUFFER) {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    if (fbuf != NULL)
        SetFirstbuf(deleteBuffer(GetFirstbuf(), fbuf));
    repBuffer(GetCurrentbuf(), buf);
    if ((buf->type != NULL) && (sbuf.type != NULL) &&
        ((!strcasecmp(buf->type, "text/plain") &&
          is_html_type(sbuf.type)) ||
         (is_html_type(buf->type) &&
          !strcasecmp(sbuf.type, "text/plain")))) {
        vwSrc();
        if (GetCurrentbuf() != buf)
            SetFirstbuf(deleteBuffer(GetFirstbuf(), buf));
    }
    GetCurrentbuf()->search_header = sbuf.search_header;
    GetCurrentbuf()->form_submit = sbuf.form_submit;
    if (GetCurrentbuf()->firstLine) {
        COPY_BUFROOT(GetCurrentbuf(), &sbuf);
        restorePosition(GetCurrentbuf(), &sbuf);
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* reshape */

void reshape()
{
    GetCurrentbuf()->need_reshape = TRUE;
    reshapeBuffer(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void docCSet()
{
    char *cs;
    wc_ces charset;
    cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Document charset: ",
                      wc_ces_to_charset(GetCurrentbuf()->document_charset));
    charset = wc_guess_charset_short(cs, 0);
    if (charset == 0) {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    _docCSet(charset);
}

void defCSet()
{
    char *cs;
    wc_ces charset;
    cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Default document charset: ",
                      wc_ces_to_charset(DocumentCharset));
    charset = wc_guess_charset_short(cs, 0);
    if (charset != 0)
        DocumentCharset = charset;
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void chkURL()
{
    chkURLBuffer(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void chkWORD()
{
    char *p;
    int spos, epos;
    p = getCurWord(GetCurrentbuf(), &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(GetCurrentbuf(), GetCurrentbuf()->currentLine, spos, epos);
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void chkNMID()
{
    chkNMIDBuffer(GetCurrentbuf());
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
/* render frame */

void rFrame()
{
    Buffer *buf;
    if ((buf = GetCurrentbuf()->linkBuffer[LB_FRAME]) != NULL) {
        SetCurrentbuf(buf);
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    if (GetCurrentbuf()->frameset == NULL) {
        if ((buf = GetCurrentbuf()->linkBuffer[LB_N_FRAME]) != NULL) {
            SetCurrentbuf(buf);
            displayBuffer(GetCurrentbuf(), B_NORMAL);
        }
        return;
    }
    if (fmInitialized) {
        message("Rendering frame", 0, 0);
        refresh();
    }
    buf = renderFrame(GetCurrentbuf(), 0);
    if (buf == NULL) {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    buf->linkBuffer[LB_N_FRAME] = GetCurrentbuf();
    GetCurrentbuf()->linkBuffer[LB_FRAME] = buf;
    pushBuffer(buf);
    if (fmInitialized && display_ok())
        displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void extbrz()
{
    if (GetCurrentbuf()->bufferprop & BP_INTERNAL) {
        /* FIXME: gettextize? */
        disp_err_message("Can't browse...", TRUE);
        return;
    }
    if (GetCurrentbuf()->currentURL.scheme == SCM_LOCAL &&
        !strcmp(GetCurrentbuf()->currentURL.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't browse stdin", TRUE);
        return;
    }
    invoke_browser(parsedURL2Str(&GetCurrentbuf()->currentURL)->ptr);
}

void linkbrz()
{
    Anchor *a;
    ParsedURL pu;
    if (GetCurrentbuf()->firstLine == NULL)
        return;
    a = retrieveCurrentAnchor(GetCurrentbuf());
    if (a == NULL)
        return;
    parseURL2(a->url, &pu, baseURL(GetCurrentbuf()));
    invoke_browser(parsedURL2Str(&pu)->ptr);
}
/* show current line number and number of lines in the entire document */

void curlno()
{
    Line *l = GetCurrentbuf()->currentLine;
    Str tmp;
    int cur = 0, all = 0, col = 0, len = 0;
    if (l != NULL) {
        cur = l->real_linenumber;
        col = l->bwidth + GetCurrentbuf()->currentColumn + GetCurrentbuf()->cursorX + 1;
        while (l->next && l->next->bpos)
            l = l->next;
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        len = l->bwidth + l->width;
    }
    if (GetCurrentbuf()->lastLine)
        all = GetCurrentbuf()->lastLine->real_linenumber;
    if (GetCurrentbuf()->pagerSource && !(GetCurrentbuf()->bufferprop & BP_CLOSE))
        tmp = Sprintf("line %d col %d/%d", cur, col, len);
    else
        tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                      (int)((double)cur * 100.0 / (double)(all ? all : 1)
                            + 0.5), col, len);
#ifdef USE_M17N
    Strcat_charp(tmp, "  ");
    Strcat_charp(tmp, wc_ces_to_charset_desc(GetCurrentbuf()->document_charset));
#endif
    disp_message(tmp->ptr, FALSE);
}

void dispI()
{
    if (!displayImage)
        initImage();
    if (!activeImage)
        return;
    displayImage = TRUE;
    /*
     * if (!(GetCurrentbuf()->type && is_html_type(GetCurrentbuf()->type)))
     * return;
     */
    GetCurrentbuf()->image_flag = IMG_FLAG_AUTO;
    GetCurrentbuf()->need_reshape = TRUE;
    displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
}

void stopI()
{
    if (!activeImage)
        return;
    /*
     * if (!(GetCurrentbuf()->type && is_html_type(GetCurrentbuf()->type)))
     * return;
     */
    GetCurrentbuf()->image_flag = IMG_FLAG_SKIP;
    displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
}

void msToggle()
{
    if (use_mouse) {
        use_mouse = FALSE;
    }
    else {
        use_mouse = TRUE;
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void mouse()
{
    int btn, x, y;
    btn = (unsigned char)getch() - 32;
#if defined(__CYGWIN__) && CYGWIN_VERSION_DLL_MAJOR < 1005
    if (cygwin_mouse_btn_swapped) {
        if (btn == MOUSE_BTN2_DOWN)
            btn = MOUSE_BTN3_DOWN;
        else if (btn == MOUSE_BTN3_DOWN)
            btn = MOUSE_BTN2_DOWN;
    }
#endif
    x = (unsigned char)getch() - 33;
    if (x < 0)
        x += 0x100;
    y = (unsigned char)getch() - 33;
    if (y < 0)
        y += 0x100;
    if (x < 0 || x >= COLS || y < 0 || y > LASTLINE)
        return;
    process_mouse(btn, x, y);
}

void movMs()
{
    if (!mouse_action.in_action)
        return;
    if ((GetTabCount() > 1 || mouse_action.menu_str) &&
        mouse_action.cursorY < GetLastTab()->y + 1)
        return;
    else if (mouse_action.cursorX >= GetCurrentbuf()->rootX &&
             mouse_action.cursorY < LASTLINE) {
        cursorXY(GetCurrentbuf(), mouse_action.cursorX - GetCurrentbuf()->rootX,
                 mouse_action.cursorY - GetCurrentbuf()->rootY);
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}
#ifdef KANJI_SYMBOLS
#define FRAME_WIDTH 2
#else
#define FRAME_WIDTH 1
#endif

void menuMs()
{
    if (!mouse_action.in_action)
        return;
    if ((GetTabCount() > 1 || mouse_action.menu_str) &&
        mouse_action.cursorY < GetLastTab()->y + 1)
        mouse_action.cursorX -= FRAME_WIDTH + 1;
    else if (mouse_action.cursorX >= GetCurrentbuf()->rootX &&
             mouse_action.cursorY < LASTLINE) {
        cursorXY(GetCurrentbuf(), mouse_action.cursorX - GetCurrentbuf()->rootX,
                 mouse_action.cursorY - GetCurrentbuf()->rootY);
        displayBuffer(GetCurrentbuf(), B_NORMAL);
    }
    mainMn();
}

void tabMs()
{
    if (!mouse_action.in_action)
        return;
    SelectTabByPosition(mouse_action.cursorX, mouse_action.cursorY);
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void closeTMs()
{
    TabBuffer *tab;
    if (!mouse_action.in_action)
        return;
    tab = posTab(mouse_action.cursorX, mouse_action.cursorY);
    if (!tab || tab == NO_TABBUFFER)
        return;
    deleteTab(tab);
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void dispVer()
{
    disp_message(Sprintf("w3m version %s", w3m_version)->ptr, TRUE);
}

void wrapToggle()
{
    if (WrapSearch) {
        WrapSearch = FALSE;
        /* FIXME: gettextize? */
        disp_message("Wrap search off", TRUE);
    }
    else {
        WrapSearch = TRUE;
        /* FIXME: gettextize? */
        disp_message("Wrap search on", TRUE);
    }
}

void dictword()
{
    execdict(inputStr("(dictionary)!", ""));
}

void dictwordat()
{
    execdict(GetWord(GetCurrentbuf()));
}

void execCmd()
{
    char *data, *p;
    int cmd;
    ClearCurrentKeyData();	/* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("command [; ...]: ", "", TextHist);
        if (data == NULL) {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
    }
    ExecuteCommand(data);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void setAlarm()
{
    char *data;
    int sec = 0;
    Command cmd = nullptr;
    ClearCurrentKeyData();	/* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("(Alarm)sec command: ", "", TextHist);
        if (data == NULL) {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
    }
    if (*data != '\0') {
        sec = atoi(getWord(&data));
        if (sec > 0)
            cmd = getFuncList(getWord(&data));
    }
    if (cmd) {
        data = getQWord(&data);
        setAlarmEvent(DefaultAlarm(), sec, AL_EXPLICIT, cmd, data);
        disp_message_nsec(Sprintf("%dsec %s %s", sec, "ID",
                                  data)->ptr, FALSE, 1, FALSE, TRUE);
    }
    else {
        setAlarmEvent(DefaultAlarm(), 0, AL_UNSET, &nulcmd, NULL);
    }
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void reinit()
{
    char *resource = searchKeyData();
    if (resource == NULL) {
        init_rc();
        sync_with_option();
#ifdef USE_COOKIE
        initCookie();
#endif
        displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
        return;
    }
    if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
        init_rc();
        sync_with_option();
        displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
        return;
    }
#ifdef USE_COOKIE
    if (!strcasecmp(resource, "COOKIE")) {
        initCookie();
        return;
    }
#endif
    if (!strcasecmp(resource, "KEYMAP")) {
        initKeymap(TRUE);
        return;
    }
    if (!strcasecmp(resource, "MAILCAP")) {
        initMailcap();
        return;
    }
#ifdef USE_MOUSE
    if (!strcasecmp(resource, "MOUSE")) {
        initMouseAction();
        displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
        return;
    }
#endif
#ifdef USE_MENU
    if (!strcasecmp(resource, "MENU")) {
        initMenu();
        return;
    }
#endif
    if (!strcasecmp(resource, "MIMETYPES")) {
        initMimeTypes();
        return;
    }
#ifdef USE_EXTERNAL_URI_LOADER
    if (!strcasecmp(resource, "URIMETHODS")) {
        initURIMethods();
        return;
    }
#endif
    disp_err_message(Sprintf("Don't know how to reinitialize '%s'", resource)->
                     ptr, FALSE);
}

void defKey()
{
    char *data;
    ClearCurrentKeyData();	/* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("Key definition: ", "", TextHist);
        if (data == NULL || *data == '\0') {
            displayBuffer(GetCurrentbuf(), B_NORMAL);
            return;
        }
    }
    SetKeymap(allocStr(data, -1), -1, TRUE);
    displayBuffer(GetCurrentbuf(), B_NORMAL);
}

void newT()
{
    _newT();
    displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
}

void closeT()
{
    if (GetTabCount() <= 1)
        return;
    TabBuffer *tab;
    if (prec_num())
        tab = numTab(PREC_NUM());
    else
        tab = GetCurrentTab();
    if (tab)
        deleteTab(tab);
    displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
}

void nextT()
{
    SelectRelativeTab(PREC_NUM());
    displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
}

void prevT()
{
    SelectRelativeTab(-PREC_NUM());
    displayBuffer(GetCurrentbuf(), B_REDRAW_IMAGE);
}

void tabA()
{
    followTab(prec_num() ? numTab(PREC_NUM()) : NULL);
}

void tabURL()
{
    tabURL0(prec_num() ? numTab(PREC_NUM()) : NULL,
            "Goto URL on new tab: ", FALSE);
}

void tabrURL()
{
    tabURL0(prec_num() ? numTab(PREC_NUM()) : NULL,
            "Goto relative URL on new tab: ", TRUE);
}

void tabR()
{
    MoveTab(PREC_NUM());
}

void tabL()
{
    MoveTab(-PREC_NUM());
}
/* download panel */

void ldDL()
{
    Buffer *buf;
    int replace = FALSE, new_tab = FALSE;
    if (GetCurrentbuf()->bufferprop & BP_INTERNAL &&
        !strcmp(GetCurrentbuf()->buffername, DOWNLOAD_LIST_TITLE))
        replace = TRUE;
    if (!FirstDL) {
        if (replace) {
            if (GetCurrentbuf() == GetFirstbuf() && GetCurrentbuf()->nextBuffer == NULL) {
                DeleteCurrentTab();
            }
            else
                delBuffer(GetCurrentbuf());
            displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
        }
        return;
    }

    auto nReload = checkDownloadList();

    buf = DownloadListBuffer();
    if (!buf) {
        displayBuffer(GetCurrentbuf(), B_NORMAL);
        return;
    }
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace) {
        COPY_BUFROOT(buf, GetCurrentbuf());
        restorePosition(buf, GetCurrentbuf());
    }
    if (!replace && open_tab_dl_list) {
        _newT();
        new_tab = TRUE;
    }
    pushBuffer(buf);
    if (replace || new_tab)
        deletePrevBuf();

    if (nReload)
        GetCurrentbuf()->event = setAlarmEvent(GetCurrentbuf()->event, 1, AL_IMPLICIT,
                                          &reload, NULL);

    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void undoPos()
{
    BufferPos *b = GetCurrentbuf()->undo;
    int i;
    if (!GetCurrentbuf()->firstLine)
        return;
    if (!b || !b->prev)
        return;
    for (i = 0; i < PREC_NUM() && b->prev; i++, b = b->prev) ;
    resetPos(b);
}

void redoPos()
{
    BufferPos *b = GetCurrentbuf()->undo;
    int i;
    if (!GetCurrentbuf()->firstLine)
        return;
    if (!b || !b->next)
        return;
    for (i = 0; i < PREC_NUM() && b->next; i++, b = b->next) ;
    resetPos(b);
}

void mainMn()
{
    PopupMenu();
}

void selMn()
{
    PopupBufferMenu();
}

void tabMn()
{
    PopupTabMenu();
}
}
