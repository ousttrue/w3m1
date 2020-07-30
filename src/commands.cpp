#include "commands.h"
#include "dispatcher.h"
#include "fm.h"
#include "urimethod.h"
#include "gc_helper.h"
#include "indep.h"
#include "frontend/line.h"
#include "file.h"
#include "html/form.h"
#include "myctype.h"
#include "public.h"
#include "regex.h"
#include "html/frame.h"
#include "dispatcher.h"
#include "frontend/menu.h"
#include "html/image.h"
#include "transport/url.h"
#include "frontend/display.h"
#include "frontend/tab.h"
#include "http/cookie.h"
#include "frontend/terms.h"
#include "frontend/mouse.h"
#include "mime/mimetypes.h"
#include "transport/local.h"
#include "html/anchor.h"
#include "transport/loader.h"
#include "mime/mailcap.h"
#include "frontend/event.h"
#include "frontend/line.h"
#include <wc.h>
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
    {
        nscroll(searchKeyNum() * (GetCurrentTab()->GetCurrentBuffer()->LINES - 1));
        displayCurrentbuf(B_NORMAL);
    }
    else
    {
        nscroll(prec_num() ? searchKeyNum() : searchKeyNum() * (GetCurrentTab()->GetCurrentBuffer()->LINES - 1));
        displayCurrentbuf(prec_num() ? B_SCROLL : B_NORMAL);
    }
}

/* Move page backward */
void pgBack()
{
    if (vi_prec_num)
    {
        nscroll(-searchKeyNum() * (GetCurrentTab()->GetCurrentBuffer()->LINES - 1));
        displayCurrentbuf(B_NORMAL);
    }
    else
    {
        nscroll(-(prec_num() ? searchKeyNum() : searchKeyNum() * (GetCurrentTab()->GetCurrentBuffer()->LINES - 1)));
        displayCurrentbuf(prec_num() ? B_SCROLL : B_NORMAL);
    }
}

/* 1 line up */

void lup1()
{
    nscroll(searchKeyNum());
    displayCurrentbuf(B_SCROLL);
}
/* 1 line down */

void ldown1()
{
    nscroll(-searchKeyNum());
    displayCurrentbuf(B_SCROLL);
}

/* move cursor position to the center of screen */

void ctrCsrV()
{
    int offsety;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    offsety = GetCurrentTab()->GetCurrentBuffer()->LINES / 2 - GetCurrentTab()->GetCurrentBuffer()->cursorY;
    if (offsety != 0)
    {
#if 0
        GetCurrentTab()->GetCurrentBuffer()->currentLine = lineSkip(GetCurrentTab()->GetCurrentBuffer(),
                                           GetCurrentTab()->GetCurrentBuffer()->currentLine, offsety,
                                           FALSE);
#endif
        GetCurrentTab()->GetCurrentBuffer()->topLine =
            lineSkip(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->topLine, -offsety, FALSE);
        arrangeLine(GetCurrentTab()->GetCurrentBuffer());
        displayCurrentbuf(B_NORMAL);
    }
}

void ctrCsrH()
{
    int offsetx;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    offsetx = GetCurrentTab()->GetCurrentBuffer()->cursorX - GetCurrentTab()->GetCurrentBuffer()->COLS / 2;
    if (offsetx != 0)
    {
        columnSkip(GetCurrentTab()->GetCurrentBuffer(), offsetx);
        arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
        displayCurrentbuf(B_NORMAL);
    }
}
/* Redraw screen */

void rdrwSc()
{
    clear();
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
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
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    column = GetCurrentTab()->GetCurrentBuffer()->currentColumn;
    columnSkip(GetCurrentTab()->GetCurrentBuffer(), searchKeyNum() * (-GetCurrentTab()->GetCurrentBuffer()->COLS + 1) + 1);
    shiftvisualpos(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->currentColumn - column);
    displayCurrentbuf(B_NORMAL);
}
/* Shift screen right */

void shiftr()
{
    int column;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    column = GetCurrentTab()->GetCurrentBuffer()->currentColumn;
    columnSkip(GetCurrentTab()->GetCurrentBuffer(), searchKeyNum() * (GetCurrentTab()->GetCurrentBuffer()->COLS - 1) - 1);
    shiftvisualpos(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->currentColumn - column);
    displayCurrentbuf(B_NORMAL);
}

void col1R()
{
    BufferPtr buf = GetCurrentTab()->GetCurrentBuffer();
    Line *l = buf->currentLine;
    int j, column, n = searchKeyNum();
    if (l == NULL)
        return;
    for (j = 0; j < n; j++)
    {
        column = buf->currentColumn;
        columnSkip(GetCurrentTab()->GetCurrentBuffer(), 1);
        if (column == buf->currentColumn)
            break;
        shiftvisualpos(GetCurrentTab()->GetCurrentBuffer(), 1);
    }
    displayCurrentbuf(B_NORMAL);
}

void col1L()
{
    BufferPtr buf = GetCurrentTab()->GetCurrentBuffer();
    Line *l = buf->currentLine;
    int j, n = searchKeyNum();
    if (l == NULL)
        return;
    for (j = 0; j < n; j++)
    {
        if (buf->currentColumn == 0)
            break;
        columnSkip(GetCurrentTab()->GetCurrentBuffer(), -1);
        shiftvisualpos(GetCurrentTab()->GetCurrentBuffer(), -1);
    }
    displayCurrentbuf(B_NORMAL);
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
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    if ((value = strchr(env, '=')) != NULL && value > env)
    {
        var = allocStr(env, value - env);
        value++;
        set_environ(var, value);
    }
    displayCurrentbuf(B_NORMAL);
}

void pipeBuf()
{
    BufferPtr buf;
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
        displayCurrentbuf(B_NORMAL);
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
    saveBuffer(GetCurrentTab()->GetCurrentBuffer(), f, TRUE);
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
        if (buf->type.empty())
            buf->type = "text/plain";
        buf->currentURL.file = "-";
        GetCurrentTab()->PushBufferCurrentPrev(buf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Execute shell command and read output ac pipe. */

void pipesh()
{
    BufferPtr buf;
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
        displayCurrentbuf(B_NORMAL);
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
        if (buf->type.empty())
            buf->type = "text/plain";
        GetCurrentTab()->PushBufferCurrentPrev(buf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Execute shell command and load entire output to buffer */

void readsh()
{
    BufferPtr buf;
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
        displayCurrentbuf(B_NORMAL);
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
        if (buf->type.empty())
            buf->type = "text/plain";
        GetCurrentTab()->PushBufferCurrentPrev(buf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
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
    displayCurrentbuf(B_FORCE_REDRAW);
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
        displayCurrentbuf(B_NORMAL);
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
                  Strnew(w3m_version)->UrlEncode()->ptr,
                  Strnew_charp_n(lang, n)->UrlEncode()->ptr);
    cmd_loadURL(tmp->ptr, NULL, NO_REFERER, NULL);
#else
    cmd_loadURL(helpFile(HELP_FILE), NULL, NO_REFERER, NULL);
#endif
}

void movL()
{
    _movL(GetCurrentTab()->GetCurrentBuffer()->COLS / 2);
}

void movL1()
{
    _movL(1);
}

void movD()
{
    _movD((GetCurrentTab()->GetCurrentBuffer()->LINES + 1) / 2);
}

void movD1()
{
    _movD(1);
}

void movU()
{
    _movU((GetCurrentTab()->GetCurrentBuffer()->LINES + 1) / 2);
}

void movU1()
{
    _movU(1);
}

void movR()
{
    _movR(GetCurrentTab()->GetCurrentBuffer()->COLS / 2);
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
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    for (i = 0; i < n; i++)
    {
        pline = GetCurrentTab()->GetCurrentBuffer()->currentLine;
        ppos = GetCurrentTab()->GetCurrentBuffer()->pos;
        if (prev_nonnull_line(GetCurrentTab()->GetCurrentBuffer()->currentLine) < 0)
            goto end;
        while (1)
        {
            l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
            lb = l->lineBuf;
            while (GetCurrentTab()->GetCurrentBuffer()->pos > 0)
            {
                int tmp = GetCurrentTab()->GetCurrentBuffer()->pos;
                prevChar(&tmp, l);
                if (is_wordchar(getChar(&lb[tmp])))
                    break;
                GetCurrentTab()->GetCurrentBuffer()->pos = tmp;
            }
            if (GetCurrentTab()->GetCurrentBuffer()->pos > 0)
                break;
            if (prev_nonnull_line(GetCurrentTab()->GetCurrentBuffer()->currentLine->prev) < 0)
            {
                GetCurrentTab()->GetCurrentBuffer()->currentLine = pline;
                GetCurrentTab()->GetCurrentBuffer()->pos = ppos;
                goto end;
            }
            GetCurrentTab()->GetCurrentBuffer()->pos = GetCurrentTab()->GetCurrentBuffer()->currentLine->len;
        }
        l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
        lb = l->lineBuf;
        while (GetCurrentTab()->GetCurrentBuffer()->pos > 0)
        {
            int tmp = GetCurrentTab()->GetCurrentBuffer()->pos;
            prevChar(&tmp, l);
            if (!is_wordchar(getChar(&lb[tmp])))
                break;
            GetCurrentTab()->GetCurrentBuffer()->pos = tmp;
        }
    }
end:
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
}

void movRW()
{
    char *lb;
    Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    for (i = 0; i < n; i++)
    {
        pline = GetCurrentTab()->GetCurrentBuffer()->currentLine;
        ppos = GetCurrentTab()->GetCurrentBuffer()->pos;
        if (next_nonnull_line(GetCurrentTab()->GetCurrentBuffer()->currentLine) < 0)
            goto end;
        l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
        lb = l->lineBuf;
        while (GetCurrentTab()->GetCurrentBuffer()->pos < l->len &&
               is_wordchar(getChar(&lb[GetCurrentTab()->GetCurrentBuffer()->pos])))
            nextChar(&GetCurrentTab()->GetCurrentBuffer()->pos, l);
        while (1)
        {
            while (GetCurrentTab()->GetCurrentBuffer()->pos < l->len &&
                   !is_wordchar(getChar(&lb[GetCurrentTab()->GetCurrentBuffer()->pos])))
                nextChar(&GetCurrentTab()->GetCurrentBuffer()->pos, l);
            if (GetCurrentTab()->GetCurrentBuffer()->pos < l->len)
                break;
            if (next_nonnull_line(GetCurrentTab()->GetCurrentBuffer()->currentLine->next) < 0)
            {
                GetCurrentTab()->GetCurrentBuffer()->currentLine = pline;
                GetCurrentTab()->GetCurrentBuffer()->pos = ppos;
                goto end;
            }
            GetCurrentTab()->GetCurrentBuffer()->pos = 0;
            l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
            lb = l->lineBuf;
        }
    }
end:
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
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
    int ok = FALSE;
    do
    {
        char cmd;
        auto buf = GetCurrentTab()->SelectBuffer(GetCurrentTab()->GetCurrentBuffer(), &cmd);
        switch (cmd)
        {
        case 'B':
            ok = TRUE;
            break;

        case '\n':
        case ' ':
            GetCurrentTab()->SetCurrentBuffer(buf);
            ok = TRUE;
            break;

        case 'D':
            GetCurrentTab()->DeleteBuffer(buf);
            if (GetCurrentTab()->GetFirstBuffer() == NULL)
            {
                /* No more buffer */
                GetCurrentTab()->SetFirstBuffer(nullBuffer(), true);
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

    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Suspend (on BSD), or run interactive shell (on SysV) */

void susp()
{
#ifndef SIGSTOP
    char *shell;
#endif /* not SIGSTOP */
    move((LINES - 1), 0);
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
    displayCurrentbuf(B_FORCE_REDRAW);
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
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    while (GetCurrentTab()->GetCurrentBuffer()->currentLine->prev && GetCurrentTab()->GetCurrentBuffer()->currentLine->bpos)
        cursorUp0(GetCurrentTab()->GetCurrentBuffer(), 1);
    GetCurrentTab()->GetCurrentBuffer()->pos = 0;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
}
/* Go to the bottom of the line */

void linend()
{
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    while (GetCurrentTab()->GetCurrentBuffer()->currentLine->next && GetCurrentTab()->GetCurrentBuffer()->currentLine->next->bpos)
        cursorDown0(GetCurrentTab()->GetCurrentBuffer(), 1);
    GetCurrentTab()->GetCurrentBuffer()->pos = GetCurrentTab()->GetCurrentBuffer()->currentLine->len - 1;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
}
/* Run editor on the current buffer */

void editBf()
{
    const char *fn = GetCurrentTab()->GetCurrentBuffer()->filename.c_str();
    Str cmd;
    if (fn == NULL || GetCurrentTab()->GetCurrentBuffer()->pagerSource != NULL ||                                                       /* Behaving as a pager */
        (GetCurrentTab()->GetCurrentBuffer()->type.empty() && GetCurrentTab()->GetCurrentBuffer()->edit == NULL) ||                     /* Reading shell */
        GetCurrentTab()->GetCurrentBuffer()->real_scheme != SCM_LOCAL || GetCurrentTab()->GetCurrentBuffer()->currentURL.file == "-" || /* file is std input  */
        GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_FRAME)
    { /* Frame */
        disp_err_message("Can't edit other than local file", TRUE);
        return;
    }
    if (GetCurrentTab()->GetCurrentBuffer()->edit)
        cmd = unquote_mailcap(
            GetCurrentTab()->GetCurrentBuffer()->edit,
            GetCurrentTab()->GetCurrentBuffer()->real_type.c_str(),
            const_cast<char *>(fn),
            checkHeader(GetCurrentTab()->GetCurrentBuffer(), "Content-Type:"), NULL);
    else
        cmd = myEditor(Editor, shell_quote(fn),
                       cur_real_linenumber(GetCurrentTab()->GetCurrentBuffer()));
    fmTerm();
    system(cmd->ptr);
    fmInit();
    displayCurrentbuf(B_FORCE_REDRAW);
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
    saveBuffer(GetCurrentTab()->GetCurrentBuffer(), f, TRUE);
    fclose(f);
    fmTerm();
    system(myEditor(Editor, shell_quote(tmpf),
                    cur_real_linenumber(GetCurrentTab()->GetCurrentBuffer()))
               ->ptr);
    fmInit();
    unlink(tmpf);
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Set / unset mark */

void _mark()
{
    Line *l;
    if (!use_mark)
        return;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
    l->propBuf[GetCurrentTab()->GetCurrentBuffer()->pos] ^= PE_MARK;
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Go to next mark */

void nextMk()
{
    Line *l;
    int i;
    if (!use_mark)
        return;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    i = GetCurrentTab()->GetCurrentBuffer()->pos + 1;
    l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
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
                GetCurrentTab()->GetCurrentBuffer()->currentLine = l;
                GetCurrentTab()->GetCurrentBuffer()->pos = i;
                arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
                displayCurrentbuf(B_NORMAL);
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
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    i = GetCurrentTab()->GetCurrentBuffer()->pos - 1;
    l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
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
                GetCurrentTab()->GetCurrentBuffer()->currentLine = l;
                GetCurrentTab()->GetCurrentBuffer()->pos = i;
                arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
                displayCurrentbuf(B_NORMAL);
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
            displayCurrentbuf(B_NORMAL);
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
    for (l = GetCurrentTab()->GetCurrentBuffer()->firstLine; l != NULL; l = l->next)
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
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* view inline image */

void followI()
{
    Line *l;
    BufferPtr buf;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
    auto a = retrieveCurrentImg(GetCurrentTab()->GetCurrentBuffer());
    if (a == NULL)
        return;
    /* FIXME: gettextize? */
    message(Sprintf("loading %s", a->url)->ptr, 0, 0);
    refresh();
    buf = loadGeneralFile(const_cast<char *>(a->url.c_str()), GetCurrentTab()->GetCurrentBuffer()->BaseURL(), NULL, RG_NONE, NULL);
    if (buf == NULL)
    {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("Can't load %s", a->url)->ptr;
        disp_err_message(emsg, FALSE);
    }
    else
    {
        GetCurrentTab()->PushBufferCurrentPrev(buf);
    }
    displayCurrentbuf(B_NORMAL);
}
/* submit form */

void submitForm()
{
    _followForm(TRUE);
}
/* go to the top anchor */

void topA()
{
    auto &hl = GetCurrentTab()->GetCurrentBuffer()->hmarklist;
    BufferPoint *po;

    int hseq = 0;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    if (hl.empty())
        return;

    if (prec_num() > hl.size())
        hseq = hl.size() - 1;
    else if (prec_num() > 0)
        hseq = prec_num() - 1;

    const Anchor *an;
    do
    {
        if (hseq >= hl.size())
            return;
        po = &hl[hseq];
        an = GetCurrentTab()->GetCurrentBuffer()->href.RetrieveAnchor(po->line, po->pos);
        if (an == NULL)
            an = GetCurrentTab()->GetCurrentBuffer()->formitem.RetrieveAnchor(po->line, po->pos);
        hseq++;
    } while (an == NULL);
    gotoLine(GetCurrentTab()->GetCurrentBuffer(), po->line);
    GetCurrentTab()->GetCurrentBuffer()->pos = po->pos;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
}
/* go to the last anchor */

void lastA()
{
    auto &hl = GetCurrentTab()->GetCurrentBuffer()->hmarklist;
    BufferPoint *po;
    int hseq;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    if (hl.empty())
        return;
    if (prec_num() >= hl.size())
        hseq = 0;
    else if (prec_num() > 0)
        hseq = hl.size() - prec_num();
    else
        hseq = hl.size() - 1;

    const Anchor *an;
    do
    {
        if (hseq < 0)
            return;
        po = &hl[hseq];
        an = GetCurrentTab()->GetCurrentBuffer()->href.RetrieveAnchor(po->line, po->pos);
        if (an == NULL)
            an = GetCurrentTab()->GetCurrentBuffer()->formitem.RetrieveAnchor(po->line, po->pos);
        hseq--;
    } while (an == NULL);
    gotoLine(GetCurrentTab()->GetCurrentBuffer(), po->line);
    GetCurrentTab()->GetCurrentBuffer()->pos = po->pos;
    arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_NORMAL);
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
    // char *url;

    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    auto l = GetCurrentTab()->GetCurrentBuffer()->currentLine;

    auto a = retrieveCurrentImg(GetCurrentTab()->GetCurrentBuffer());
    if (a && a->image && a->image->map)
    {
        _followForm(FALSE);
        return;
    }

    int map = 0;
    int x = 0;
    int y = 0;
    if (a && a->image && a->image->ismap)
    {
        getMapXY(GetCurrentTab()->GetCurrentBuffer(), a, &x, &y);
        map = 1;
    }
    a = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
    if (a == NULL)
    {
        _followForm(FALSE);
        return;
    }

    if (a->url.size() && a->url[0] == '#')
    { /* index within this buffer */
        gotoLabel(a->url.c_str() + 1);
        return;
    }

    ParsedURL u;
    u.Parse2(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
    if (u.ToStr()->Cmp(GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()) == 0)
    {
        /* index within this buffer */
        if (u.label.size())
        {
            gotoLabel(u.label.c_str());
            return;
        }
    }

    if (handleMailto(a->url.c_str()))
    {
        return;
    }

    auto url = a->url;
    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

    if (check_target && open_tab_blank &&
        (a->target == "_new" || a->target == "_blank"))
    {
        auto tab = CreateTabSetCurrent();
        auto buf = tab->GetCurrentBuffer();
        loadLink(url.c_str(), a->target.c_str(), a->referer.c_str(), NULL);
        if (buf != GetCurrentTab()->GetCurrentBuffer())
            GetCurrentTab()->DeleteBuffer(buf);
        else
            deleteTab(GetCurrentTab());
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }
    else
    {
        loadLink(url.c_str(), a->target.c_str(), a->referer.c_str(), NULL);
        displayCurrentbuf(B_NORMAL);
    }
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
    int i;
    auto prec = prec_num() ? prec_num() : 1;
    for (i = 0; i < prec; i++)
    {
        auto buf = GetCurrentTab()->PrevBuffer(GetCurrentTab()->GetCurrentBuffer());
        if (!buf)
        {
            if (i == 0)
                return;
            break;
        }
        GetCurrentTab()->SetCurrentBuffer(buf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* go to the previous bufferr */

void prevBf()
{
    auto tab = GetCurrentTab();
    auto prec = prec_num() ? prec_num() : 1;
    for (int i = 0; i < prec; i++)
    {
        auto buf = tab->NextBuffer(tab->GetCurrentBuffer());
        if (!buf)
        {
            if (i == 0)
                return;
            break;
        }
        tab->SetCurrentBuffer(buf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* delete current buffer and back to the previous buffer */

void backBf()
{
    BufferPtr buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_FRAME];
    if (!checkBackBuffer(GetCurrentTab(), GetCurrentTab()->GetCurrentBuffer()))
    {
        if (close_tab_back && GetTabCount() >= 1)
        {
            deleteTab(GetCurrentTab());
            displayCurrentbuf(B_FORCE_REDRAW);
        }
        else
            /* FIXME: gettextize? */
            disp_message("Can't back...", TRUE);
        return;
    }
    auto tab = GetCurrentTab();
    tab->DeleteBuffer(tab->GetCurrentBuffer());
    if (buf)
    {
        if (buf->frameQ)
        {
            struct frameset *fs;
            long linenumber = buf->frameQ->linenumber;
            long top = buf->frameQ->top_linenumber;
            int pos = buf->frameQ->pos;
            int currentColumn = buf->frameQ->currentColumn;
            AnchorList &formitem = buf->frameQ->formitem;
            fs = popFrameTree(&(buf->frameQ));
            deleteFrameSet(buf->frameset);
            buf->frameset = fs;
            if (buf == GetCurrentTab()->GetCurrentBuffer())
            {
                rFrame();
                GetCurrentTab()->GetCurrentBuffer()->topLine = lineSkip(GetCurrentTab()->GetCurrentBuffer(),
                                                                        GetCurrentTab()->GetCurrentBuffer()->firstLine, top - 1,
                                                                        FALSE);
                gotoLine(GetCurrentTab()->GetCurrentBuffer(), linenumber);
                GetCurrentTab()->GetCurrentBuffer()->pos = pos;
                GetCurrentTab()->GetCurrentBuffer()->currentColumn = currentColumn;
                arrangeCursor(GetCurrentTab()->GetCurrentBuffer());
                formResetBuffer(GetCurrentTab()->GetCurrentBuffer(), formitem);
            }
        }
        else if (RenderFrame && buf == GetCurrentTab()->GetCurrentBuffer())
        {
            auto tab = GetCurrentTab();
            tab->DeleteBuffer(tab->GetCurrentBuffer());
        }
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

void deletePrevBuf()
{
    auto tab = GetCurrentTab();
    BufferPtr buf = tab->PrevBuffer(tab->GetCurrentBuffer());
    if (buf)
        tab->DeleteBuffer(buf);
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
    auto tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s&charset=%s",
                       (localCookie()->UrlEncode())->ptr,
                       (Strnew(BookmarkFile)->UrlEncode())->ptr,
                       (GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->UrlEncode())->ptr,

                       (wc_conv_strict(GetCurrentTab()->GetCurrentBuffer()->buffername.c_str(),
                                       InnerCharset,
                                       BookmarkCharset)
                            ->UrlEncode())
                           ->ptr,
                       wc_ces_to_charset(BookmarkCharset));
    auto request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    request->body = tmp->ptr;
    request->length = tmp->Size();
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
    BufferPtr buf;
    if ((buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_INFO]) != NULL)
    {
        GetCurrentTab()->SetCurrentBuffer(buf);
        displayCurrentbuf(B_NORMAL);
        return;
    }
    auto tab = GetCurrentTab();
    if ((buf = tab->GetCurrentBuffer()->linkBuffer[LB_INFO]) != NULL)
        tab->DeleteBuffer(buf);
    buf = page_info_panel(GetCurrentTab()->GetCurrentBuffer());
    cmd_loadBuffer(buf, BP_NORMAL, LB_INFO);
}
/* link menu */

void linkMn()
{
    LinkList *l = link_menu(GetCurrentTab()->GetCurrentBuffer());
    ParsedURL p_url;
    if (!l || !l->url)
        return;
    if (*(l->url) == '#')
    {
        gotoLabel(l->url + 1);
        return;
    }
    p_url.Parse2(l->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
    pushHashHist(URLHist, p_url.ToStr()->ptr);
    cmd_loadURL(l->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL(),
                GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->ptr, NULL);
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
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL)
    {
        if (opt != NULL && *opt != '\0')
        {
            char *v = get_param_option(opt);
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist("Set option: ", opt, TextHist);
        if (opt == NULL || *opt == '\0')
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    if (set_param_option(opt))
        sync_with_option();
    displayCurrentbuf(B_REDRAW_IMAGE);
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
    BufferPtr buf;
    buf = link_list_panel(GetCurrentTab()->GetCurrentBuffer());
    if (buf != NULL)
    {
#ifdef USE_M17N
        buf->document_charset = GetCurrentTab()->GetCurrentBuffer()->document_charset;
#endif
        cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
    }
}
/* cookie list */

void cooLst()
{
    BufferPtr buf;
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
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    do_download = TRUE;
    followA();
    do_download = FALSE;
}
/* download IMG link */

void svI()
{
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
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
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    file = searchKeyData();
    if (file == NULL || *file == '\0')
    {
        /* FIXME: gettextize? */
        qfile = inputLineHist("Save buffer to: ", NULL, IN_COMMAND, SaveHist);
        if (qfile == NULL || *qfile == '\0')
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file);
    if (*file == '|')
    {
        is_pipe = TRUE;
        f = popen(file + 1, "w");
    }
    else
    {
        if (qfile)
        {
            file = unescape_spaces(Strnew(qfile))->ptr;
            file = conv_to_system(file);
        }
        file = expandPath(file);
        if (checkOverWrite(file) < 0)
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
        f = fopen(file, "w");
        is_pipe = FALSE;
    }
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
        disp_err_message(emsg, TRUE);
        return;
    }
    saveBuffer(GetCurrentTab()->GetCurrentBuffer(), f, TRUE);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
    displayCurrentbuf(B_NORMAL);
}
/* save source */

void svSrc()
{
    char *file;
    if (GetCurrentTab()->GetCurrentBuffer()->sourcefile.empty())
        return;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    PermitSaveToPipe = TRUE;
    if (GetCurrentTab()->GetCurrentBuffer()->real_scheme == SCM_LOCAL)
        file = conv_from_system(guess_save_name(NULL,
                                                GetCurrentTab()->GetCurrentBuffer()->currentURL.real_file));
    else
        file = guess_save_name(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->currentURL.file);
    doFileCopy(GetCurrentTab()->GetCurrentBuffer()->sourcefile.c_str(), file);
    PermitSaveToPipe = FALSE;
    displayCurrentbuf(B_NORMAL);
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
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
        return;
    if (CurrentKey() == PrevKey() && s != NULL)
    {
        if (s->Size() - offset >= COLS)
            offset++;
        else if (s->Size() <= offset) /* bug ? */
            offset = 0;
    }
    else
    {
        offset = 0;
        s = currentURL();
        if (DecodeURL)
            s = Strnew(url_unquote_conv(s->ptr, WC_CES_NONE));
#ifdef USE_M17N
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->Size());
        bcopy((void *)pp, (void *)p, s->Size() * sizeof(Lineprop));
#endif
    }
    n = searchKeyNum();
    if (n > 1 && s->Size() > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
#ifdef USE_M17N
    while (offset < s->Size() && p[offset] & PC_WCHAR2)
        offset++;
#endif
    disp_message_nomouse(&s->ptr[offset], TRUE);
}
/* view HTML source */

void vwSrc()
{
    BufferPtr buf;
    if (GetCurrentTab()->GetCurrentBuffer()->type.empty() || GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_FRAME)
        return;
    if ((buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_SOURCE]) != NULL ||
        (buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_SOURCE]) != NULL)
    {
        GetCurrentTab()->SetCurrentBuffer(buf);
        displayCurrentbuf(B_NORMAL);
        return;
    }
    if (GetCurrentTab()->GetCurrentBuffer()->sourcefile.empty())
    {
        if (GetCurrentTab()->GetCurrentBuffer()->pagerSource &&
            GetCurrentTab()->GetCurrentBuffer()->type == "text/plain")
        {
            CharacterEncodingScheme old_charset;
            wc_bool old_fix_width_conv;

            Str tmpf = tmpfname(TMPF_SRC, NULL);
            auto f = fopen(tmpf->ptr, "w");
            if (f == NULL)
                return;

            old_charset = DisplayCharset;
            old_fix_width_conv = WcOption.fix_width_conv;
            DisplayCharset = (GetCurrentTab()->GetCurrentBuffer()->document_charset != WC_CES_US_ASCII)
                                 ? GetCurrentTab()->GetCurrentBuffer()->document_charset
                                 : WC_CES_NONE;
            WcOption.fix_width_conv = WC_FALSE;

            saveBufferBody(GetCurrentTab()->GetCurrentBuffer(), f, TRUE);
#ifdef USE_M17N
            DisplayCharset = old_charset;
            WcOption.fix_width_conv = old_fix_width_conv;
#endif
            fclose(f);
            GetCurrentTab()->GetCurrentBuffer()->sourcefile = tmpf->ptr;
        }
        else
        {
            return;
        }
    }
    buf = newBuffer(INIT_BUFFER_WIDTH);
    if (is_html_type(GetCurrentTab()->GetCurrentBuffer()->type.c_str()))
    {
        buf->type = "text/plain";
        if (is_html_type(GetCurrentTab()->GetCurrentBuffer()->real_type))
            buf->real_type = "text/plain";
        else
            buf->real_type = GetCurrentTab()->GetCurrentBuffer()->real_type;
        buf->buffername = Sprintf("source of %s", GetCurrentTab()->GetCurrentBuffer()->buffername)->ptr;
        buf->linkBuffer[LB_N_SOURCE] = GetCurrentTab()->GetCurrentBuffer();
        GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_SOURCE] = buf;
    }
    else if (GetCurrentTab()->GetCurrentBuffer()->type == "text/plain")
    {
        buf->type = "text/html";
        if (GetCurrentTab()->GetCurrentBuffer()->real_type == "text/plain")
            buf->real_type = "text/html";
        else
            buf->real_type = GetCurrentTab()->GetCurrentBuffer()->real_type;
        buf->buffername = Sprintf("HTML view of %s",
                                  GetCurrentTab()->GetCurrentBuffer()->buffername)
                              ->ptr;
        buf->linkBuffer[LB_SOURCE] = GetCurrentTab()->GetCurrentBuffer();
        GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_SOURCE] = buf;
    }
    else
    {
        return;
    }
    buf->currentURL = GetCurrentTab()->GetCurrentBuffer()->currentURL;
    buf->real_scheme = GetCurrentTab()->GetCurrentBuffer()->real_scheme;
    buf->filename = GetCurrentTab()->GetCurrentBuffer()->filename;
    buf->sourcefile = GetCurrentTab()->GetCurrentBuffer()->sourcefile;
    buf->header_source = GetCurrentTab()->GetCurrentBuffer()->header_source;
    buf->search_header = GetCurrentTab()->GetCurrentBuffer()->search_header;
#ifdef USE_M17N
    buf->document_charset = GetCurrentTab()->GetCurrentBuffer()->document_charset;
#endif
    buf->clone = GetCurrentTab()->GetCurrentBuffer()->clone;
    (*buf->clone)++;
    buf->need_reshape = TRUE;
    reshapeBuffer(buf);
    GetCurrentTab()->PushBufferCurrentPrev(buf);
    displayCurrentbuf(B_NORMAL);
}
/* reload */

void reload()
{
    BufferPtr buf;
    BufferPtr fbuf = NULL;
    CharacterEncodingScheme old_charset;

    Str url;
    FormList *request;
    int multipart;
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
    {
        if (!strcmp(GetCurrentTab()->GetCurrentBuffer()->buffername.c_str(), DOWNLOAD_LIST_TITLE))
        {
            ldDL();
            return;
        }
        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    if (GetCurrentTab()->GetCurrentBuffer()->currentURL.scheme == SCM_LOCAL &&
        GetCurrentTab()->GetCurrentBuffer()->currentURL.file == "-")
    {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't reload stdin", TRUE);
        return;
    }
    auto sbuf = GetCurrentTab()->GetCurrentBuffer()->Copy();
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_FRAME &&
        (fbuf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_FRAME]))
    {
        if (fmInitialized)
        {
            message("Rendering frame", 0, 0);
            refresh();
        }
        if (!(buf = renderFrame(fbuf, 1)))
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
        if (fbuf->linkBuffer[LB_FRAME])
        {
            if (buf->sourcefile.size() &&
                fbuf->linkBuffer[LB_FRAME]->sourcefile.size() &&
                buf->sourcefile == fbuf->linkBuffer[LB_FRAME]->sourcefile)
                fbuf->linkBuffer[LB_FRAME]->sourcefile.clear();
            auto tab = GetCurrentTab();
            tab->DeleteBuffer(fbuf->linkBuffer[LB_FRAME]);
        }
        fbuf->linkBuffer[LB_FRAME] = buf;
        buf->linkBuffer[LB_N_FRAME] = fbuf;
        GetCurrentTab()->PushBufferCurrentPrev(buf);
        GetCurrentTab()->SetCurrentBuffer(buf);
        if (GetCurrentTab()->GetCurrentBuffer()->firstLine)
        {
            COPY_BUFROOT(GetCurrentTab()->GetCurrentBuffer(), sbuf);
            restorePosition(GetCurrentTab()->GetCurrentBuffer(), sbuf);
        }
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }
    else if (GetCurrentTab()->GetCurrentBuffer()->frameset != NULL)
        fbuf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_FRAME];
    multipart = 0;
    if (GetCurrentTab()->GetCurrentBuffer()->form_submit)
    {
        request = GetCurrentTab()->GetCurrentBuffer()->form_submit->parent;
        if (request->method == FORM_METHOD_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
        {
            Str query;
            struct stat st;
            multipart = 1;
            query_from_followform(&query, GetCurrentTab()->GetCurrentBuffer()->form_submit, multipart);
            stat(request->body, &st);
            request->length = st.st_size;
        }
    }
    else
    {
        request = NULL;
    }
    url = GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr();
    /* FIXME: gettextize? */
    message("Reloading...", 0, 0);
    refresh();
#ifdef USE_M17N
    old_charset = DocumentCharset;
    if (GetCurrentTab()->GetCurrentBuffer()->document_charset != WC_CES_US_ASCII)
        DocumentCharset = GetCurrentTab()->GetCurrentBuffer()->document_charset;
#endif
    SearchHeader = GetCurrentTab()->GetCurrentBuffer()->search_header;
    DefaultType = Strnew(GetCurrentTab()->GetCurrentBuffer()->real_type)->ptr;
    buf = loadGeneralFile(url->ptr, NULL, NO_REFERER, RG_NOCACHE, request);
#ifdef USE_M17N
    DocumentCharset = old_charset;
#endif
    SearchHeader = FALSE;
    DefaultType = NULL;
    if (multipart)
        unlink(request->body);
    if (buf == NULL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    else
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    if (fbuf != NULL)
        GetCurrentTab()->DeleteBuffer(fbuf);
    repBuffer(GetCurrentTab()->GetCurrentBuffer(), buf);
    if ((buf->type.size()) && (sbuf->type.size()) &&
        ((buf->type == "text/plain" &&
          is_html_type(sbuf->type)) ||
         (is_html_type(buf->type) &&
          sbuf->type == "text/plain")))
    {
        vwSrc();
        if (GetCurrentTab()->GetCurrentBuffer() != buf)
            GetCurrentTab()->DeleteBuffer(buf);
    }
    GetCurrentTab()->GetCurrentBuffer()->search_header = sbuf->search_header;
    GetCurrentTab()->GetCurrentBuffer()->form_submit = sbuf->form_submit;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine)
    {
        COPY_BUFROOT(GetCurrentTab()->GetCurrentBuffer(), sbuf);
        restorePosition(GetCurrentTab()->GetCurrentBuffer(), sbuf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* reshape */

void reshape()
{
    GetCurrentTab()->GetCurrentBuffer()->need_reshape = TRUE;
    reshapeBuffer(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
}

void docCSet()
{
    char *cs;
    CharacterEncodingScheme charset;
    cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Document charset: ",
                      wc_ces_to_charset(GetCurrentTab()->GetCurrentBuffer()->document_charset));
    charset = wc_guess_charset_short(cs, WC_CES_NONE);
    if (charset == 0)
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    _docCSet(charset);
}

void defCSet()
{
    char *cs;
    CharacterEncodingScheme charset;
    cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Default document charset: ",
                      wc_ces_to_charset(DocumentCharset));
    charset = wc_guess_charset_short(cs, WC_CES_NONE);
    if (charset != 0)
        DocumentCharset = charset;
    displayCurrentbuf(B_NORMAL);
}

void chkURL()
{
    chkURLBuffer(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
}

void chkWORD()
{
    char *p;
    int spos, epos;
    p = getCurWord(GetCurrentTab()->GetCurrentBuffer(), &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->currentLine, spos, epos);
    displayCurrentbuf(B_FORCE_REDRAW);
}

void chkNMID()
{
    chkNMIDBuffer(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* render frame */

void rFrame()
{
    BufferPtr buf;
    if ((buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_FRAME]) != NULL)
    {
        GetCurrentTab()->SetCurrentBuffer(buf);
        displayCurrentbuf(B_NORMAL);
        return;
    }
    if (GetCurrentTab()->GetCurrentBuffer()->frameset == NULL)
    {
        if ((buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_FRAME]) != NULL)
        {
            GetCurrentTab()->SetCurrentBuffer(buf);
            displayCurrentbuf(B_NORMAL);
        }
        return;
    }
    if (fmInitialized)
    {
        message("Rendering frame", 0, 0);
        refresh();
    }
    buf = renderFrame(GetCurrentTab()->GetCurrentBuffer(), 0);
    if (buf == NULL)
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    buf->linkBuffer[LB_N_FRAME] = GetCurrentTab()->GetCurrentBuffer();
    GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_FRAME] = buf;
    GetCurrentTab()->PushBufferCurrentPrev(buf);
    if (fmInitialized && display_ok())
        displayCurrentbuf(B_FORCE_REDRAW);
}

void extbrz()
{
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't browse...", TRUE);
        return;
    }
    if (GetCurrentTab()->GetCurrentBuffer()->currentURL.scheme == SCM_LOCAL &&
        GetCurrentTab()->GetCurrentBuffer()->currentURL.file == "-")
    {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't browse stdin", TRUE);
        return;
    }
    invoke_browser(GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->ptr);
}

void linkbrz()
{
    ParsedURL pu;
    if (GetCurrentTab()->GetCurrentBuffer()->firstLine == NULL)
        return;
    auto a = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
    if (a == NULL)
        return;
    pu.Parse2(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
    invoke_browser(pu.ToStr()->ptr);
}
/* show current line number and number of lines in the entire document */

void curlno()
{
    Line *l = GetCurrentTab()->GetCurrentBuffer()->currentLine;
    Str tmp;
    int cur = 0, all = 0, col = 0, len = 0;
    if (l != NULL)
    {
        cur = l->real_linenumber;
        col = l->bwidth + GetCurrentTab()->GetCurrentBuffer()->currentColumn + GetCurrentTab()->GetCurrentBuffer()->cursorX + 1;
        while (l->next && l->next->bpos)
            l = l->next;
        if (l->width < 0)
            l->width = l->COLPOS(l->len);
        len = l->bwidth + l->width;
    }
    if (GetCurrentTab()->GetCurrentBuffer()->lastLine)
        all = GetCurrentTab()->GetCurrentBuffer()->lastLine->real_linenumber;
    if (GetCurrentTab()->GetCurrentBuffer()->pagerSource && !(GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_CLOSE))
        tmp = Sprintf("line %d col %d/%d", cur, col, len);
    else
        tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                      (int)((double)cur * 100.0 / (double)(all ? all : 1) + 0.5), col, len);
#ifdef USE_M17N
    tmp->Push("  ");
    tmp->Push(wc_ces_to_charset_desc(GetCurrentTab()->GetCurrentBuffer()->document_charset));
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
     * if (!(GetCurrentTab()->GetCurrentBuffer()->type && is_html_type(GetCurrentTab()->GetCurrentBuffer()->type)))
     * return;
     */
    GetCurrentTab()->GetCurrentBuffer()->image_flag = IMG_FLAG_AUTO;
    GetCurrentTab()->GetCurrentBuffer()->need_reshape = TRUE;
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void stopI()
{
    if (!activeImage)
        return;
    /*
     * if (!(GetCurrentTab()->GetCurrentBuffer()->type && is_html_type(GetCurrentTab()->GetCurrentBuffer()->type)))
     * return;
     */
    GetCurrentTab()->GetCurrentBuffer()->image_flag = IMG_FLAG_SKIP;
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void msToggle()
{
    if (use_mouse)
    {
        use_mouse = FALSE;
    }
    else
    {
        use_mouse = TRUE;
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

void mouse()
{
    auto btn = static_cast<MouseBtnAction>((unsigned char)getch() - 32);
    int x = (unsigned char)getch() - 33;
    if (x < 0)
        x += 0x100;
    int y = (unsigned char)getch() - 33;
    if (y < 0)
        y += 0x100;
    if (x < 0 || x >= COLS || y < 0 || y > (LINES - 1))
        return;
    process_mouse(btn, x, y);
}

void movMs()
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }
    if ((GetTabCount() > 1 || GetMouseActionMenuStr()) &&
        y < GetTabbarHeight())
        return;
    else if (x >= GetCurrentTab()->GetCurrentBuffer()->rootX &&
             y < (LINES - 1))
    {
        cursorXY(GetCurrentTab()->GetCurrentBuffer(),
                 x - GetCurrentTab()->GetCurrentBuffer()->rootX,
                 y - GetCurrentTab()->GetCurrentBuffer()->rootY);
    }
    displayCurrentbuf(B_NORMAL);
}
#ifdef KANJI_SYMBOLS
#define FRAME_WIDTH 2
#else
#define FRAME_WIDTH 1
#endif

void menuMs()
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }
    if ((GetTabCount() > 1 || GetMouseActionMenuStr()) && y < GetTabbarHeight())
    {
        x -= FRAME_WIDTH + 1;
    }
    else if (x >= GetCurrentTab()->GetCurrentBuffer()->rootX &&
             y < (LINES - 1))
    {
        cursorXY(GetCurrentTab()->GetCurrentBuffer(),
                 x - GetCurrentTab()->GetCurrentBuffer()->rootX,
                 y - GetCurrentTab()->GetCurrentBuffer()->rootY);
        displayCurrentbuf(B_NORMAL);
    }
    mainMenu(x, y);
}

void tabMs()
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }
    SelectTabByPosition(x, y);
    displayCurrentbuf(B_FORCE_REDRAW);
}

void closeTMs()
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }
    auto tab = posTab(x, y);
    if (!tab)
        return;
    deleteTab(tab);
    displayCurrentbuf(B_FORCE_REDRAW);
}

void dispVer()
{
    disp_message(Sprintf("w3m version %s", w3m_version)->ptr, TRUE);
}

void wrapToggle()
{
    if (WrapSearch)
    {
        WrapSearch = FALSE;
        /* FIXME: gettextize? */
        disp_message("Wrap search off", TRUE);
    }
    else
    {
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
    execdict(GetWord(GetCurrentTab()->GetCurrentBuffer()));
}

void execCmd()
{
    char *data, *p;
    int cmd;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0')
    {
        data = inputStrHist("command [; ...]: ", "", TextHist);
        if (data == NULL)
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    ExecuteCommand(data);
    displayCurrentbuf(B_NORMAL);
}

void setAlarm()
{
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    auto data = searchKeyData();
    if (data == NULL || *data == '\0')
    {
        data = inputStrHist("(Alarm)sec command: ", "", TextHist);
        if (data == NULL)
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }

    int sec = 0;
    Command cmd = nullptr;
    if (*data != '\0')
    {
        sec = atoi(getWord(&data));
        if (sec > 0)
            cmd = getFuncList(getWord(&data));
    }

    if (cmd)
    {
        data = getQWord(&data);
        setAlarmEvent(DefaultAlarm(), sec, AL_EXPLICIT, cmd, data);
        disp_message_nsec(Sprintf("%dsec %s %s", sec, "ID",
                                  data)
                              ->ptr,
                          FALSE, 1, FALSE, TRUE);
    }
    else
    {
        setAlarmEvent(DefaultAlarm(), 0, AL_UNSET, &nulcmd, NULL);
    }
    displayCurrentbuf(B_NORMAL);
}

void reinit()
{
    char *resource = searchKeyData();
    if (resource == NULL)
    {
        init_rc();
        sync_with_option();
#ifdef USE_COOKIE
        initCookie();
#endif
        displayCurrentbuf(B_REDRAW_IMAGE);
        return;
    }
    if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC"))
    {
        init_rc();
        sync_with_option();
        displayCurrentbuf(B_REDRAW_IMAGE);
        return;
    }
#ifdef USE_COOKIE
    if (!strcasecmp(resource, "COOKIE"))
    {
        initCookie();
        return;
    }
#endif
    if (!strcasecmp(resource, "KEYMAP"))
    {
        initKeymap(TRUE);
        return;
    }
    if (!strcasecmp(resource, "MAILCAP"))
    {
        initMailcap();
        return;
    }
#ifdef USE_MOUSE
    if (!strcasecmp(resource, "MOUSE"))
    {
        initMouseAction();
        displayCurrentbuf(B_REDRAW_IMAGE);
        return;
    }
#endif
#ifdef USE_MENU
    if (!strcasecmp(resource, "MENU"))
    {
        initMenu();
        return;
    }
#endif
    if (!strcasecmp(resource, "MIMETYPES"))
    {
        initMimeTypes();
        return;
    }
#ifdef USE_EXTERNAL_URI_LOADER
    if (!strcasecmp(resource, "URIMETHODS"))
    {
        initURIMethods();
        return;
    }
#endif
    disp_err_message(Sprintf("Don't know how to reinitialize '%s'", resource)->ptr, FALSE);
}

void defKey()
{
    char *data;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0')
    {
        data = inputStrHist("Key definition: ", "", TextHist);
        if (data == NULL || *data == '\0')
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    SetKeymap(allocStr(data, -1), -1, TRUE);
    displayCurrentbuf(B_NORMAL);
}

void newT()
{
    auto tab = CreateTabSetCurrent();
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void closeT()
{
    if (GetTabCount() <= 1)
        return;
    TabPtr tab;
    if (prec_num())
        tab = GetTabByIndex(prec_num() - 1);
    else
        tab = GetCurrentTab();
    if (tab)
        deleteTab(tab);
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void nextT()
{
    SelectRelativeTab(prec_num() ? prec_num() : 1);
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void prevT()
{
    SelectRelativeTab(-(prec_num() ? prec_num() : 1));
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void tabA()
{
    TabPtr tab;
    if (prec_num())
        tab = GetTabByIndex(prec_num() - 1);
    else
        tab = GetCurrentTab();
    followTab(prec_num() ? tab : NULL);
}

void tabURL()
{
    TabPtr tab;
    if (prec_num())
        tab = GetTabByIndex(prec_num() - 1);
    else
        tab = GetCurrentTab();
    tabURL0(prec_num() ? tab : NULL,
            "Goto URL on new tab: ", FALSE);
}

void tabrURL()
{
    TabPtr tab;
    if (prec_num())
        tab = GetTabByIndex(prec_num() - 1);
    else
        tab = GetCurrentTab();
    tabURL0(prec_num() ? tab : NULL,
            "Goto relative URL on new tab: ", TRUE);
}

void tabR()
{
    MoveTab(prec_num() ? prec_num() : 1);
    displayCurrentbuf(B_FORCE_REDRAW);
}

void tabL()
{
    MoveTab(-(prec_num() ? prec_num() : 1));
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* download panel */

void ldDL()
{
    BufferPtr buf;
    int replace = FALSE, new_tab = FALSE;
    auto tab = GetCurrentTab();
    if (tab->GetCurrentBuffer()->bufferprop & BP_INTERNAL &&
        tab->GetCurrentBuffer()->buffername == DOWNLOAD_LIST_TITLE)
        replace = TRUE;
    if (!FirstDL)
    {
        if (replace)
        {
            if (tab->GetCurrentBuffer() == tab->GetFirstBuffer() && tab->NextBuffer(tab->GetCurrentBuffer()))
            {
                DeleteCurrentTab();
            }
            else
                tab->DeleteBuffer(tab->GetCurrentBuffer());
            displayBuffer(tab->GetCurrentBuffer(), B_FORCE_REDRAW);
        }
        return;
    }

    auto nReload = checkDownloadList();

    buf = DownloadListBuffer();
    if (!buf)
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace)
    {
        COPY_BUFROOT(buf, GetCurrentTab()->GetCurrentBuffer());
        restorePosition(buf, GetCurrentTab()->GetCurrentBuffer());
    }
    if (!replace && open_tab_dl_list)
    {
        CreateTabSetCurrent();
        new_tab = TRUE;
    }
    GetCurrentTab()->PushBufferCurrentPrev(buf);
    if (replace || new_tab)
        deletePrevBuf();

    if (nReload)
        GetCurrentTab()->GetCurrentBuffer()->event = setAlarmEvent(GetCurrentTab()->GetCurrentBuffer()->event, 1, AL_IMPLICIT,
                                                                   &reload, NULL);

    displayCurrentbuf(B_FORCE_REDRAW);
}

void undoPos()
{
    BufferPos *b = GetCurrentTab()->GetCurrentBuffer()->undo;
    int i;
    if (!GetCurrentTab()->GetCurrentBuffer()->firstLine)
        return;
    if (!b || !b->prev)
        return;
    for (i = 0; i < (prec_num() ? prec_num() : 1) && b->prev; i++, b = b->prev)
        ;
    resetPos(b);
}

void redoPos()
{
    BufferPos *b = GetCurrentTab()->GetCurrentBuffer()->undo;
    int i;
    if (!GetCurrentTab()->GetCurrentBuffer()->firstLine)
        return;
    if (!b || !b->next)
        return;
    for (i = 0; i < (prec_num() ? prec_num() : 1) && b->next; i++, b = b->next)
        ;
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
