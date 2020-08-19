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
#include "stream/url.h"
#include "frontend/display.h"
#include "frontend/tab.h"
#include "stream/cookie.h"
#include "frontend/terms.h"
#include "frontend/mouse.h"
#include "frontend/tabbar.h"
#include "mime/mimetypes.h"
#include "stream/local_cgi.h"
#include "html/anchor.h"
#include "stream/loader.h"
#include "mime/mailcap.h"
#include "frontend/event.h"
#include "frontend/line.h"
#include "frontend/lineinput.h"

#include "rc.h"
#include "w3m.h"
#include <signal.h>

/* do nothing */
void nulcmd(w3mApp *w3m)
{
}

void escmap(w3mApp *w3m)
{
    char c = getch();
    escKeyProc(c);
}

void escbmap(w3mApp *w3m)
{
    char c = getch();
    escbKeyProc(c);
}

void multimap(w3mApp *w3m)
{
    char c = getch();
    MultiKeyProc(c);
}

/* Move page forward */
void pgFore(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (w3mApp::Instance().vi_prec_num)
    {
        buf->NScroll(searchKeyNum() * (buf->rect.lines - 1));
        displayCurrentbuf(B_NORMAL);
    }
    else
    {
        buf->NScroll(prec_num() ? searchKeyNum() : searchKeyNum() * (buf->rect.lines - 1));
        displayCurrentbuf(prec_num() ? B_SCROLL : B_NORMAL);
    }
}

/* Move page backward */
void pgBack(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (w3mApp::Instance().vi_prec_num)
    {
        buf->NScroll(-searchKeyNum() * (buf->rect.lines - 1));
        displayCurrentbuf(B_NORMAL);
    }
    else
    {
        buf->NScroll(-(prec_num() ? searchKeyNum() : searchKeyNum() * (buf->rect.lines - 1)));
        displayCurrentbuf(prec_num() ? B_SCROLL : B_NORMAL);
    }
}

/* 1 line up */
void lup1(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    buf->NScroll(searchKeyNum());
    displayCurrentbuf(B_SCROLL);
}

/* 1 line down */
void ldown1(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    buf->NScroll(-searchKeyNum());
    displayCurrentbuf(B_SCROLL);
}

/* move cursor position to the center of screen */
void ctrCsrV(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;

    int offsety = buf->rect.lines / 2 - buf->rect.cursorY;
    if (offsety)
    {
        buf->LineSkip(buf->TopLine(), -offsety, FALSE);
        buf->ArrangeLine();
        displayCurrentbuf(B_NORMAL);
    }
}

void ctrCsrH(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;

    int offsetx = buf->rect.cursorX - buf->rect.cols / 2;
    if (offsetx)
    {
        buf->ColumnSkip(offsetx);
        buf->ArrangeCursor();
        displayCurrentbuf(B_NORMAL);
    }
}

/* Redraw screen */
void rdrwSc(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    clear();
    buf->ArrangeCursor();
    displayCurrentbuf(B_FORCE_REDRAW);
}

/* Search regular expression forward */
void srchfor(w3mApp *w3m)
{
    srch(forwardSearch, "Forward: ");
}

void isrchfor(w3mApp *w3m)
{
    isrch(forwardSearch, "I-search: ");
}

/* Search regular expression backward */
void srchbak(w3mApp *w3m)
{
    srch(backwardSearch, "Backward: ");
}

void isrchbak(w3mApp *w3m)
{
    isrch(backwardSearch, "I-search backward: ");
}

/* Search next matching */
void srchnxt(w3mApp *w3m)
{
    srch_nxtprv(0);
}

/* Search previous matching */
void srchprv(w3mApp *w3m)
{
    srch_nxtprv(1);
}

/* Shift screen left */
void shiftl(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    int column = buf->currentColumn;
    buf->ColumnSkip(searchKeyNum() * (-buf->rect.cols + 1) + 1);
    shiftvisualpos(buf, buf->currentColumn - column);
    displayCurrentbuf(B_NORMAL);
}

/* Shift screen right */
void shiftr(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    int column = buf->currentColumn;
    buf->ColumnSkip(searchKeyNum() * (buf->rect.cols - 1) - 1);
    shiftvisualpos(buf, buf->currentColumn - column);
    displayCurrentbuf(B_NORMAL);
}

void col1R(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    LinePtr l = buf->CurrentLine();
    int j, column, n = searchKeyNum();
    if (l == NULL)
        return;
    for (j = 0; j < n; j++)
    {
        column = buf->currentColumn;
        buf->ColumnSkip(1);
        if (column == buf->currentColumn)
            break;
        shiftvisualpos(buf, 1);
    }
    displayCurrentbuf(B_NORMAL);
}

void col1L(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    LinePtr l = buf->CurrentLine();
    int j, n = searchKeyNum();
    if (l == NULL)
        return;
    for (j = 0; j < n; j++)
    {
        if (buf->currentColumn == 0)
            break;
        buf->ColumnSkip(-1);
        shiftvisualpos(buf, -1);
    }
    displayCurrentbuf(B_NORMAL);
}

void setEnv(w3mApp *w3m)
{
    char *env;
    char *var, *value;
    ClearCurrentKeyData();
    env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL)
    {
        if (env != NULL && *env != '\0')
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist("Set environ: ", env, w3mApp::Instance().TextHist);
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

void pipeBuf(w3mApp *w3m)
{
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    auto cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0')
    {
        /* FIXME: gettextize? */
        cmd = inputLineHist("Pipe buffer to: ", "", IN_COMMAND, w3mApp::Instance().ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0')
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }

    auto tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    auto f = fopen(tmpf, "w");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_message(Sprintf("Can't save buffer to %s", cmd)->ptr, TRUE);
        return;
    }
    saveBuffer(GetCurrentTab()->GetCurrentBuffer(), f, TRUE);
    fclose(f);

    GetCurrentTab()->Push(URL::Parse(shell_quote(tmpf)));

    // auto newBuf = getpipe(myExtCommand(cmd, shell_quote(tmpf), TRUE)->ptr);
    // if (newBuf == NULL)
    // {
    //     disp_message("Execution failed", TRUE);
    //     return;
    // }
    // else
    // {
    //     newBuf->filename = cmd;
    //     newBuf->buffername = Sprintf("%s %s", PIPEBUFFERNAME,
    //                                  conv_from_system(cmd))
    //                              ->ptr;
    //     newBuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    //     if (newBuf->type.empty())
    //         newBuf->type = "text/plain";
    //     newBuf->currentURL.path = "-";
    //     GetCurrentTab()->Push(newBuf);
    // }
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Execute shell command and read output ac pipe. */

void pipesh(w3mApp *w3m)
{
    BufferPtr buf;
    char *cmd;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0')
    {
        cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, w3mApp::Instance().ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0')
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    buf = getpipe(cmd);
    // if (buf == NULL)
    // {
    //     disp_message("Execution failed", TRUE);
    //     return;
    // }
    // else
    // {
    //     buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    //     if (buf->type.empty())
    //         buf->type = "text/plain";
    //     GetCurrentTab()->Push(buf);
    // }
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Execute shell command and load entire output to buffer */

void readsh(w3mApp *w3m)
{
    BufferPtr buf;
    char *cmd;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0')
    {
        cmd = inputLineHist("(read shell)!", "", IN_COMMAND, w3mApp::Instance().ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0')
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }

    crmode();

    auto success = TrapJmp([&]() {
        buf = getshell(cmd);
        return true;
    });

    term_raw();

    if (!success)
    {
        /* FIXME: gettextize? */
        disp_message("Execution failed", TRUE);
        return;
    }

    // buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    // if (buf->type.empty())
    //     buf->type = "text/plain";
    // GetCurrentTab()->Push(buf);
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Execute shell command */

void execsh(w3mApp *w3m)
{
    char *cmd;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0')
    {
        cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, w3mApp::Instance().ShellHist);
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

void ldfile(w3mApp *w3m)
{
    char *fn;
    fn = searchKeyData();
    if (fn == NULL || *fn == '\0')
    {
        /* FIXME: gettextize? */
        fn = inputFilenameHist("(Load)Filename? ", NULL, w3mApp::Instance().LoadHist);
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

void ldhelp(w3mApp *w3m)
{
    std::string_view lang = w3mApp::Instance().AcceptLang;
    auto n = strcspn(lang.data(), ";, \t");
    auto tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
                  UrlEncode(Strnew_m_charp(w3mApp::w3m_version))->ptr,
                  UrlEncode(Strnew_m_charp(lang.substr(n)))->ptr);
    cmd_loadURL(tmp->ptr, NULL, HttpReferrerPolicy::NoReferer, NULL);
}

void movL(w3mApp *w3m)
{
    _movL(GetCurrentTab()->GetCurrentBuffer()->rect.cols / 2);
}

void movL1(w3mApp *w3m)
{
    _movL(1);
}

void movD(w3mApp *w3m)
{
    _movD((GetCurrentTab()->GetCurrentBuffer()->rect.lines + 1) / 2);
}

void movD1(w3mApp *w3m)
{
    _movD(1);
}

void movU(w3mApp *w3m)
{
    _movU((GetCurrentTab()->GetCurrentBuffer()->rect.lines + 1) / 2);
}

void movU1(w3mApp *w3m)
{
    _movU(1);
}

void movR(w3mApp *w3m)
{
    _movR(GetCurrentTab()->GetCurrentBuffer()->rect.cols / 2);
}

void movR1(w3mApp *w3m)
{
    _movR(1);
}

void movLW(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    int n = searchKeyNum();
    if (!buf->MoveLeftWord(n))
    {
        return;
    }
    displayCurrentbuf(B_NORMAL);
}

void movRW(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    int n = searchKeyNum();
    if (!buf->MoveRightWord(n))
    {
        return;
    }
    displayCurrentbuf(B_NORMAL);
}

/* Quit */
void quitfm(w3mApp *w3m)
{
    w3m->_quitfm(FALSE);
}

/* Question and Quit */
void qquitfm(w3mApp *w3m)
{
    w3m->_quitfm(w3mApp::Instance().confirm_on_quit);
}

/* Select buffer */
void selBuf(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    int ok = FALSE;
    do
    {
        char cmd;
        // auto buf = tab->SelectBuffer(tab->GetCurrentBuffer(), &cmd);
        BufferPtr buf;
        switch (cmd)
        {
        case 'B':
            ok = TRUE;
            break;

        case '\n':
        case ' ':
            // tab->SetCurrent(tab->GetBufferIndex(buf));
            ok = TRUE;
            break;

        case 'D':
            // tab->DeleteBuffer(buf);
            break;

        case 'q':
            qquitfm(w3m);
            break;

        case 'Q':
            quitfm(w3m);
            break;
        }
    } while (!ok);

    displayCurrentbuf(B_FORCE_REDRAW);
}
/* Suspend (on BSD), or run interactive shell (on SysV) */

void susp(w3mApp *w3m)
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

void goLine(w3mApp *w3m)
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

void goLineF(w3mApp *w3m)
{
    _goLine("^");
}

void goLineL(w3mApp *w3m)
{
    _goLine("$");
}

/* Go to the beginning of the line */
void linbeg(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    while (buf->PrevLine(buf->CurrentLine()) && buf->CurrentLine()->bpos)
        buf->CursorUp0(1);
    buf->pos = 0;
    buf->ArrangeCursor();
    displayCurrentbuf(B_NORMAL);
}

/* Go to the bottom of the line */
void linend(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    while (buf->NextLine(buf->CurrentLine()) && buf->NextLine(buf->CurrentLine())->bpos)
        buf->CursorDown0(1);
    buf->pos = buf->CurrentLine()->len() - 1;
    buf->ArrangeCursor();
    displayCurrentbuf(B_NORMAL);
}

/* Run editor on the current buffer */
void editBf(w3mApp *w3m)
{
    const char *fn = GetCurrentTab()->GetCurrentBuffer()->filename.c_str();
    Str cmd;
    if (fn == NULL || GetCurrentTab()->GetCurrentBuffer()->pagerSource != NULL ||                                                       /* Behaving as a pager */
        (GetCurrentTab()->GetCurrentBuffer()->type.empty() && GetCurrentTab()->GetCurrentBuffer()->edit.empty()) ||                     /* Reading shell */
        GetCurrentTab()->GetCurrentBuffer()->real_scheme != SCM_LOCAL || GetCurrentTab()->GetCurrentBuffer()->currentURL.path == "-" || /* file is std input  */
        GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_FRAME)
    { /* Frame */
        disp_err_message("Can't edit other than local file", TRUE);
        return;
    }
    if (GetCurrentTab()->GetCurrentBuffer()->edit.size())
    {
        // TODO:
        // cmd = unquote_mailcap(
        //     GetCurrentTab()->GetCurrentBuffer()->edit.c_str(),
        //     GetCurrentTab()->GetCurrentBuffer()->real_type.c_str(),
        //     const_cast<char *>(fn),
        //     checkHeader(GetCurrentTab()->GetCurrentBuffer(), "Content-Type:"), NULL);
    }
    else
    {
        cmd = myEditor(
            w3mApp::Instance().Editor.c_str(), 
            shell_quote(fn),
            cur_real_linenumber(GetCurrentTab()->GetCurrentBuffer()));
    }
    fmTerm();
    system(cmd->ptr);
    fmInit();
    displayCurrentbuf(B_FORCE_REDRAW);
    reload(w3m);
}

/* Run editor on the current screen */
void editScr(w3mApp *w3m)
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
    system(myEditor(w3mApp::Instance().Editor.c_str(), shell_quote(tmpf),
                    cur_real_linenumber(GetCurrentTab()->GetCurrentBuffer()))
               ->ptr);
    fmInit();
    unlink(tmpf);
    displayCurrentbuf(B_FORCE_REDRAW);
}

/* Set / unset mark */
void _mark(w3mApp *w3m)
{
    LinePtr l;
    if (!w3mApp::Instance().use_mark)
        return;
    if (GetCurrentTab()->GetCurrentBuffer()->LineCount() == 0)
        return;
    l = GetCurrentTab()->GetCurrentBuffer()->CurrentLine();
    l->propBuf()[GetCurrentTab()->GetCurrentBuffer()->pos] ^= PE_MARK;
    displayCurrentbuf(B_FORCE_REDRAW);
}

/* Go to next mark */
void nextMk(w3mApp *w3m)
{
    LinePtr l;
    int i;
    if (!w3mApp::Instance().use_mark)
        return;
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    i = buf->pos + 1;
    l = buf->CurrentLine();
    if (i >= l->len())
    {
        i = 0;
        l = buf->NextLine(l);
    }
    while (l != NULL)
    {
        for (; i < l->len(); i++)
        {
            if (l->propBuf()[i] & PE_MARK)
            {
                buf->SetCurrentLine(l);
                buf->pos = i;
                buf->ArrangeCursor();
                displayCurrentbuf(B_NORMAL);
                return;
            }
        }
        l = buf->NextLine(l);
        i = 0;
    }
    /* FIXME: gettextize? */
    disp_message("No mark exist after here", TRUE);
}
/* Go to previous mark */

void prevMk(w3mApp *w3m)
{
    LinePtr l;
    int i;
    if (!w3mApp::Instance().use_mark)
        return;
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    i = buf->pos - 1;
    l = buf->CurrentLine();
    if (i < 0)
    {
        l = buf->PrevLine(l);
        if (l != NULL)
            i = l->len() - 1;
    }
    while (l != NULL)
    {
        for (; i >= 0; i--)
        {
            if (l->propBuf()[i] & PE_MARK)
            {
                buf->SetCurrentLine(l);
                buf->pos = i;
                buf->ArrangeCursor();
                displayCurrentbuf(B_NORMAL);
                return;
            }
        }
        l = buf->PrevLine(l);
        if (l != NULL)
            i = l->len() - 1;
    }
    /* FIXME: gettextize? */
    disp_message("No mark exist before here", TRUE);
}
/* Mark place to which the regular expression matches */

void reMark(w3mApp *w3m)
{
    if (!w3mApp::Instance().use_mark)
        return;

    auto str = searchKeyData();
    if (str == NULL || *str == '\0')
    {
        str = inputStrHist("(Mark)Regexp: ", MarkString(), w3mApp::Instance().TextHist);
        if (str == NULL || *str == '\0')
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    str = conv_search_string(str, w3mApp::Instance().DisplayCharset);
    if ((str = regexCompile(str, 1)) != NULL)
    {
        disp_message(str, TRUE);
        return;
    }
    SetMarkString(str);

    GetCurrentTab()->GetCurrentBuffer()->EachLine([&](auto l) {
        char *p, *p1, *p2;
        p = l->lineBuf();
        for (;;)
        {
            if (regexMatch(p, &l->lineBuf()[l->len()] - p, p == l->lineBuf()) == 1)
            {
                matchedPosition(&p1, &p2);
                l->propBuf()[p1 - l->lineBuf()] |= PE_MARK;
                p = p2;
            }
            else
                break;
        }
    });
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* view inline image */

void followI(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    auto a = buf->img.RetrieveAnchor(buf->CurrentPoint());
    if (a == NULL)
        return;

    auto l = buf->CurrentLine();
    /* FIXME: gettextize? */
    message(Sprintf("loading %s", a->url)->ptr, 0, 0);
    refresh();
    GetCurrentTab()->Push(URL::Parse(a->url));
    // auto newBuf = loadGeneralFile(URL::Parse(a->url), buf->BaseURL());
    // if (newBuf == NULL)
    // {
    //     /* FIXME: gettextize? */
    //     char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    //     disp_err_message(emsg, FALSE);
    // }
    // else
    // {
    //     GetCurrentTab()->Push(newBuf);
    // }
    displayCurrentbuf(B_NORMAL);
}
/* submit form */

void submitForm(w3mApp *w3m)
{
    _followForm(TRUE);
}
/* go to the top anchor */

void topA(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;

    auto &hl = buf->hmarklist;
    if (hl.empty())
        return;

    int hseq = 0;
    if (prec_num() > hl.size())
        hseq = hl.size() - 1;
    else if (prec_num() > 0)
        hseq = prec_num() - 1;

    BufferPoint *po;
    const Anchor *an;
    do
    {
        if (hseq >= hl.size())
            return;
        auto &po = hl[hseq];
        an = buf->href.RetrieveAnchor(po);
        if (an == NULL)
            an = buf->formitem.RetrieveAnchor(po);
        hseq++;
    } while (an == NULL);

    buf->Goto(*po);
    displayCurrentbuf(B_NORMAL);
}
/* go to the last anchor */

void lastA(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;

    auto &hl = buf->hmarklist;
    BufferPoint *po;
    int hseq;
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
        auto &po = hl[hseq];
        an = buf->href.RetrieveAnchor(po);
        if (an == NULL)
            an = buf->formitem.RetrieveAnchor(po);
        hseq--;
    } while (an == NULL);

    buf->Goto(*po);
    displayCurrentbuf(B_NORMAL);
}
/* go to the next anchor */

void nextA(w3mApp *w3m)
{
    _nextA(FALSE);
}
/* go to the previous anchor */

void prevA(w3mApp *w3m)
{
    _prevA(FALSE);
}
/* go to the next visited anchor */

void nextVA(w3mApp *w3m)
{
    _nextA(TRUE);
}
/* go to the previous visited anchor */

void prevVA(w3mApp *w3m)
{
    _prevA(TRUE);
}

static std::tuple<bool, int, int> isMap(const BufferPtr &buf, const Anchor *a)
{
    int x = 0;
    int y = 0;
    if (a && a->image && a->image->ismap)
    {
        getMapXY(buf, a, &x, &y);
        return {true, x, y};
    }

    return {};
}

/* follow HREF link */
void followA(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    auto l = buf->CurrentLine();

    bool map = false;
    int x = -1;
    int y = -1;

    {
        auto a = buf->img.RetrieveAnchor(buf->CurrentPoint());
        if (a && a->image && a->image->map)
        {
            _followForm(FALSE);
            return;
        }

        std::tie(map, x, y) = isMap(buf, a);
    }

    auto a = buf->href.RetrieveAnchor(buf->CurrentPoint());
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

    auto u = URL::Parse(a->url).Resolve(buf->BaseURL());
    if (u.ToStr()->Cmp(buf->currentURL.ToStr()) == 0)
    {
        /* index within this buffer */
        if (u.fragment.size())
        {
            gotoLabel(u.fragment.c_str());
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

    // if (check_target() && open_tab_blank &&
    //     (a->target == "_new" || a->target == "_blank"))
    // {
    //     auto tab = CreateTabSetCurrent();
    //     auto buf = tab->GetCurrentBuffer();
    //     loadLink(url.c_str(), a->target.c_str(), a->referer, NULL);
    //     // if (buf != buf)
    //     //     GetCurrentTab()->DeleteBuffer(buf);
    //     // else
    //     //     deleteTab(GetCurrentTab());
    //     displayCurrentbuf(B_FORCE_REDRAW);
    //     return;
    // }
    // else
    // {
    //     loadLink(url.c_str(), a->target.c_str(), a->referer, NULL);
    //     displayCurrentbuf(B_NORMAL);
    // }

    tab->Push(URL::Parse(url));
    displayCurrentbuf(B_NORMAL);
}

/* go to the next left anchor */
void nextL(w3mApp *w3m)
{
    nextX(-1, 0);
}

/* go to the next left-up anchor */

void nextLU(w3mApp *w3m)
{
    nextX(-1, -1);
}
/* go to the next right anchor */

void nextR(w3mApp *w3m)
{
    nextX(1, 0);
}
/* go to the next right-down anchor */

void nextRD(w3mApp *w3m)
{
    nextX(1, 1);
}
/* go to the next downward anchor */

void nextD(w3mApp *w3m)
{
    nextY(1);
}
/* go to the next upward anchor */

void nextU(w3mApp *w3m)
{
    nextY(-1);
}

/* go to the next bufferr */
void nextBf(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    int i = 0;
    auto prec = prec_num() ? prec_num() : 1;
    for (; i < prec; i++)
    {
        if (!tab->Forward())
        {
            break;
        }
    }
    if (i)
    {
        displayCurrentbuf(B_FORCE_REDRAW);
    }
}

/* go to the previous bufferr */
void prevBf(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto prec = prec_num() ? prec_num() : 1;
    int i = 0;
    for (; i < prec; i++)
    {
        if (!tab->Back())
        {
            break;
        }
    }
    if (i)
    {
        displayCurrentbuf(B_FORCE_REDRAW);
    }
}

/* delete current buffer and back to the previous buffer */
void backBf(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    tab->Back(true);
    // auto buf = tab->GetCurrentBuffer()->linkBuffer[LB_N_FRAME];
    // if (!tab->CheckBackBuffer())
    // {
    //     if (close_tab_back && GetTabCount() >= 1)
    //     {
    //         deleteTab(GetCurrentTab());
    //         displayCurrentbuf(B_FORCE_REDRAW);
    //     }
    //     else
    //         /* FIXME: gettextize? */
    //         disp_message("Can't back...", TRUE);
    //     return;
    // }
    // tab->DeleteBuffer(tab->GetCurrentBuffer());
    // if (buf)
    // {
    //     if (buf->frameQ)
    //     {
    //         struct frameset *fs;
    //         long linenumber = buf->frameQ->linenumber;
    //         long top = buf->frameQ->top_linenumber;
    //         int pos = buf->frameQ->pos;
    //         int currentColumn = buf->frameQ->currentColumn;
    //         AnchorList &formitem = buf->frameQ->formitem;
    //         fs = popFrameTree(&(buf->frameQ));
    //         deleteFrameSet(buf->frameset);
    //         buf->frameset = fs;
    //         if (buf == GetCurrentTab()->GetCurrentBuffer())
    //         {
    //             rFrame(w3m);
    //             GetCurrentTab()->GetCurrentBuffer()->LineSkip(
    //                 GetCurrentTab()->GetCurrentBuffer()->FirstLine(), top - 1,
    //                 FALSE);
    //             GetCurrentTab()->GetCurrentBuffer()->Goto({linenumber, pos});
    //             // GetCurrentTab()->GetCurrentBuffer()->pos = pos;
    //             // GetCurrentTab()->GetCurrentBuffer()->ArrangeCursor();
    //             GetCurrentTab()->GetCurrentBuffer()->currentColumn = currentColumn;
    //             formResetBuffer(GetCurrentTab()->GetCurrentBuffer(), formitem);
    //         }
    //     }
    //     else if (w3mApp::Instance().RenderFrame && buf == GetCurrentTab()->GetCurrentBuffer())
    //     {
    //         auto tab = GetCurrentTab();
    //         tab->DeleteBuffer(tab->GetCurrentBuffer());
    //     }
    // }
    displayCurrentbuf(B_FORCE_REDRAW);
}

// for local CGI replace
void deletePrevBuf(w3mApp *w3m)
{
    GetCurrentTab()->DeleteBack();
}

void goURL(w3mApp *w3m)
{
    goURL0("Goto URL: ", FALSE);
}

void gorURL(w3mApp *w3m)
{
    goURL0("Goto relative URL: ", TRUE);
}
/* load bookmark */

void ldBmark(w3mApp *w3m)
{
    cmd_loadURL(w3mApp::Instance().BookmarkFile, NULL, HttpReferrerPolicy::NoReferer, NULL);
}
/* Add current to bookmark */

void adBmark(w3mApp *w3m)
{
    auto tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s&charset=%s",
                       UrlEncode((localCookie()))->ptr,
                       UrlEncode((Strnew(w3mApp::Instance().BookmarkFile)))->ptr,
                       UrlEncode((GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()))->ptr,

                       UrlEncode((wc_conv_strict(GetCurrentTab()->GetCurrentBuffer()->buffername.c_str(),
                                                 w3mApp::Instance().InnerCharset,
                                                 w3mApp::Instance().BookmarkCharset)))
                           ->ptr,
                       wc_ces_to_charset(w3mApp::Instance().BookmarkCharset));
    auto request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    request->body = tmp->ptr;
    request->length = tmp->Size();
    cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, HttpReferrerPolicy::NoReferer,
                request);
}
/* option setting */

void ldOpt(w3mApp *w3m)
{
    // cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}
/* error message list */

void msgs(w3mApp *w3m)
{
    // cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}
/* page info */

void pginfo(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    // BufferPtr buf;
    // if ((buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_INFO]) != NULL)
    // {
    //     GetCurrentTab()->SetCurrentBuffer(buf);
    //     displayCurrentbuf(B_NORMAL);
    //     return;
    // }

    // if ((buf = tab->GetCurrentBuffer()->linkBuffer[LB_INFO]) != NULL)
    //     tab->DeleteBuffer(buf);
    auto newBuf = page_info_panel(GetCurrentTab()->GetCurrentBuffer());
    // cmd_loadBuffer(newBuf, BP_NORMAL, LB_INFO);
    // tab->Push(newBuf);
}
/* link menu */

void linkMn(w3mApp *w3m)
{
    auto l = link_menu(GetCurrentTab()->GetCurrentBuffer());
    if (!l || l->url().empty())
        return;

    if (l->url()[0] == '#')
    {
        gotoLabel(l->url().substr(1));
        return;
    }

    auto p_url = URL::Parse(l->url()).Resolve(GetCurrentTab()->GetCurrentBuffer()->BaseURL());
    pushHashHist(w3mApp::Instance().URLHist, p_url.ToStr()->ptr);
    cmd_loadURL(l->url(), GetCurrentTab()->GetCurrentBuffer()->BaseURL(), HttpReferrerPolicy::StrictOriginWhenCrossOrigin, NULL);
}
/* accesskey */

void accessKey(w3mApp *w3m)
{
    anchorMn(accesskey_menu, TRUE);
}
/* set an option */

void setOpt(w3mApp *w3m)
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
        opt = inputStrHist("Set option: ", opt, w3mApp::Instance().TextHist);
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

void listMn(w3mApp *w3m)
{
    anchorMn(list_menu, TRUE);
}

void movlistMn(w3mApp *w3m)
{
    anchorMn(list_menu, FALSE);
}
/* link,anchor,image list */

void linkLst(w3mApp *w3m)
{
    BufferPtr buf;
    buf = link_list_panel(GetCurrentTab()->GetCurrentBuffer());
    if (buf != NULL)
    {
#ifdef USE_M17N
        buf->document_charset = GetCurrentTab()->GetCurrentBuffer()->document_charset;
#endif
        // cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
    }
}
/* cookie list */

void cooLst(w3mApp *w3m)
{
    BufferPtr buf;
    buf = cookie_list_panel();
    if (buf != NULL)
    {
        // cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
    }
}
/* History page */

void ldHist(w3mApp *w3m)
{
    // cmd_loadBuffer(historyBuffer(w3mApp::Instance().URLHist), BP_NO_URL, LB_NOLINK);
}
/* download HREF link */

void svA(w3mApp *w3m)
{
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    w3mApp::Instance().do_download = TRUE;
    followA(w3m);
    w3mApp::Instance().do_download = FALSE;
}
/* download IMG link */

void svI(w3mApp *w3m)
{
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    w3mApp::Instance().do_download = TRUE;
    followI(w3m);
    w3mApp::Instance().do_download = FALSE;
}
/* save buffer */

void svBuf(w3mApp *w3m)
{
    char *qfile = NULL, *file;
    FILE *f;
    int is_pipe;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    file = searchKeyData();
    if (file == NULL || *file == '\0')
    {
        /* FIXME: gettextize? */
        qfile = inputLineHist("Save buffer to: ", NULL, IN_COMMAND, w3mApp::Instance().SaveHist);
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

void svSrc(w3mApp *w3m)
{
    char *file = nullptr;
    if (GetCurrentTab()->GetCurrentBuffer()->sourcefile.empty())
        return;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    w3mApp::Instance().PermitSaveToPipe = TRUE;
    // if (GetCurrentTab()->GetCurrentBuffer()->real_scheme == SCM_LOCAL)
    //     file = conv_from_system(guess_save_name(NULL,
    //                                             GetCurrentTab()->GetCurrentBuffer()->currentURL.real_file));
    // else
    //     file = guess_save_name(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->currentURL.path);
    doFileCopy(GetCurrentTab()->GetCurrentBuffer()->sourcefile.c_str(), file);
    w3mApp::Instance().PermitSaveToPipe = FALSE;
    displayCurrentbuf(B_NORMAL);
}

/* peek URL */
void peekURL(w3mApp *w3m)
{
    _peekURL(0);
}

/* peek URL of image */
void peekIMG(w3mApp *w3m)
{
    _peekURL(1);
}

void curURL(w3mApp *w3m)
{
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
        return;

    static Str s = NULL;
    static Lineprop *p = NULL;
    static int offset = 0;
    if (CurrentKey() == PrevKey() && s)
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
        if (w3mApp::Instance().DecodeURL)
            s = Strnew(url_unquote_conv(s->ptr, WC_CES_NONE));

        Lineprop *pp;
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->Size());
        bcopy((void *)pp, (void *)p, s->Size() * sizeof(Lineprop));
    }

    int n = searchKeyNum();
    if (n > 1 && s->Size() > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
    while (offset < s->Size() && p[offset] & PC_WCHAR2)
        offset++;
    disp_message_nomouse(&s->ptr[offset], TRUE);
}

/* view HTML source */
void vwSrc(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->type.empty() || buf->bufferprop & BP_FRAME)
    {
        return;
    }

    // {
    //     BufferPtr link;
    //     if ((link = buf->linkBuffer[LB_SOURCE]) ||
    //         (link = buf->linkBuffer[LB_N_SOURCE]))
    //     {
    //         GetCurrentTab()->SetCurrentBuffer(link);
    //         displayCurrentbuf(B_NORMAL);
    //         return;
    //     }
    // }

    if (buf->sourcefile.empty())
    {
        if (buf->pagerSource && buf->type == "text/plain")
        {
            // pager
            CharacterEncodingScheme old_charset;
            bool old_fix_width_conv;

            Str tmpf = tmpfname(TMPF_SRC, NULL);
            auto f = fopen(tmpf->ptr, "w");
            if (f == NULL)
                return;

            old_charset = w3mApp::Instance().DisplayCharset;
            old_fix_width_conv = WcOption.fix_width_conv;
            w3mApp::Instance().DisplayCharset = (buf->document_charset != WC_CES_US_ASCII)
                                                    ? buf->document_charset
                                                    : WC_CES_NONE;
            WcOption.fix_width_conv = false;

            saveBufferBody(buf, f, TRUE);

            w3mApp::Instance().DisplayCharset = old_charset;
            WcOption.fix_width_conv = old_fix_width_conv;

            fclose(f);
            buf->sourcefile = tmpf->ptr;
        }
        else
        {
            return;
        }
    }

    {
        auto newBuf = newBuffer(buf->currentURL);
        if (is_html_type(buf->type.c_str()))
        {
            newBuf->type = "text/plain";
            if (is_html_type(buf->real_type))
                newBuf->real_type = "text/plain";
            else
                newBuf->real_type = buf->real_type;
            newBuf->buffername = Sprintf("source of %s", buf->buffername)->ptr;
            newBuf->linkBuffer[LB_N_SOURCE] = buf;
            buf->linkBuffer[LB_SOURCE] = newBuf;
        }
        else if (buf->type == "text/plain")
        {
            newBuf->type = "text/html";
            if (buf->real_type == "text/plain")
                newBuf->real_type = "text/html";
            else
                newBuf->real_type = buf->real_type;
            newBuf->buffername = Sprintf("HTML view of %s",
                                         buf->buffername)
                                     ->ptr;
            newBuf->linkBuffer[LB_SOURCE] = buf;
            buf->linkBuffer[LB_N_SOURCE] = newBuf;
        }
        else
        {
            return;
        }
        newBuf->currentURL = buf->currentURL;
        newBuf->real_scheme = buf->real_scheme;
        newBuf->filename = buf->filename;
        newBuf->sourcefile = buf->sourcefile;
        newBuf->header_source = buf->header_source;
        newBuf->search_header = buf->search_header;
        newBuf->document_charset = buf->document_charset;
        newBuf->clone = buf->clone;
        (*newBuf->clone)++;
        newBuf->need_reshape = TRUE;
        // buf->Reshape();
        GetCurrentTab()->Push(URL::Parse("w3m://htmlsource"));
        displayCurrentbuf(B_NORMAL);
    }
}

/* reload */
void reload(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->bufferprop & BP_INTERNAL)
    {
        if (buf->buffername == w3mApp::DOWNLOAD_LIST_TITLE)
        {
            ldDL(w3m);
            return;
        }

        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", TRUE);
        return;
    }

    if (buf->currentURL.IsStdin())
    {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't reload stdin", TRUE);
        return;
    }

    BufferPtr fbuf = NULL;
    auto sbuf = buf->Copy();
    if (buf->bufferprop & BP_FRAME &&
        (fbuf = buf->linkBuffer[LB_N_FRAME]))
    {
        // frame
        if (w3mApp::Instance().fmInitialized)
        {
            message("Rendering frame", 0, 0);
            refresh();
        }

        BufferPtr renderBuf;
        if (!(renderBuf = renderFrame(fbuf, 1)))
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
        if (fbuf->linkBuffer[LB_FRAME])
        {
            if (renderBuf->sourcefile.size() &&
                fbuf->linkBuffer[LB_FRAME]->sourcefile.size() &&
                renderBuf->sourcefile == fbuf->linkBuffer[LB_FRAME]->sourcefile)
                fbuf->linkBuffer[LB_FRAME]->sourcefile.clear();
            auto tab = GetCurrentTab();
            // tab->DeleteBuffer(fbuf->linkBuffer[LB_FRAME]);
        }
        fbuf->linkBuffer[LB_FRAME] = renderBuf;
        renderBuf->linkBuffer[LB_N_FRAME] = fbuf;
        // GetCurrentTab()->Push(renderBuf);
        if (GetCurrentTab()->GetCurrentBuffer()->LineCount())
        {
            GetCurrentTab()->GetCurrentBuffer()->rect = sbuf->rect;
            GetCurrentTab()->GetCurrentBuffer()->restorePosition(sbuf);
        }
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }

    // int multipart;
    if (GetCurrentTab()->GetCurrentBuffer()->frameset != NULL)
    {
        fbuf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_FRAME];
    }

    //
    // form
    //
    FormList *request;
    int multipart = 0;
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

    //
    // reload
    //
    /* FIXME: gettextize? */
    message("Reloading...", 0, 0);
    refresh();

    auto old_charset = w3m->DocumentCharset;
    if (buf->document_charset != WC_CES_US_ASCII)
        w3m->DocumentCharset = buf->document_charset;

    w3m->SearchHeader = buf->search_header;
    w3m->DefaultType = Strnew(buf->real_type)->ptr;

    {
        auto newBuf = loadGeneralFile(buf->currentURL.NoCache(), NULL, HttpReferrerPolicy::NoReferer, request);

        w3m->DocumentCharset = old_charset;

        w3m->SearchHeader = FALSE;
        w3m->DefaultType.clear();
        if (multipart)
            unlink(request->body);

        if (newBuf == NULL)
        {
            /* FIXME: gettextize? */
            disp_err_message("Can't reload...", TRUE);
            return;
        }

        // if (fbuf != NULL)
        //     GetCurrentTab()->DeleteBuffer(fbuf);

        // tab->Push(newBuf);
        tab->DeleteBack();

        if ((newBuf->type.size()) && (sbuf->type.size()) &&
            ((newBuf->type == "text/plain" &&
              is_html_type(sbuf->type)) ||
             (is_html_type(newBuf->type) &&
              sbuf->type == "text/plain")))
        {
            vwSrc(w3m);
            // if (GetCurrentTab()->GetCurrentBuffer() != newBuf)
            //     GetCurrentTab()->DeleteBuffer(newBuf);
        }
        GetCurrentTab()->GetCurrentBuffer()->search_header = sbuf->search_header;
        GetCurrentTab()->GetCurrentBuffer()->form_submit = sbuf->form_submit;
        if (GetCurrentTab()->GetCurrentBuffer()->LineCount())
        {
            GetCurrentTab()->GetCurrentBuffer()->rect = sbuf->rect;
            GetCurrentTab()->GetCurrentBuffer()->restorePosition(sbuf);
        }
        displayCurrentbuf(B_FORCE_REDRAW);
    }
}
/* reshape */

void reshape(w3mApp *w3m)
{
    GetCurrentTab()->GetCurrentBuffer()->need_reshape = TRUE;
    // GetCurrentTab()->GetCurrentBuffer()->Reshape();
    displayCurrentbuf(B_FORCE_REDRAW);
}

void docCSet(w3mApp *w3m)
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

void defCSet(w3mApp *w3m)
{
    char *cs;
    CharacterEncodingScheme charset;
    cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Default document charset: ",
                      wc_ces_to_charset(w3mApp::Instance().DocumentCharset));
    charset = wc_guess_charset_short(cs, WC_CES_NONE);
    if (charset != 0)
        w3mApp::Instance().DocumentCharset = charset;
    displayCurrentbuf(B_NORMAL);
}

void chkURL(w3mApp *w3m)
{
    chkURLBuffer(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
}

void chkWORD(w3mApp *w3m)
{
    char *p;
    int spos, epos;
    p = getCurWord(GetCurrentTab()->GetCurrentBuffer(), &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(GetCurrentTab()->GetCurrentBuffer(), GetCurrentTab()->GetCurrentBuffer()->CurrentLine(), spos, epos);
    displayCurrentbuf(B_FORCE_REDRAW);
}

void chkNMID(w3mApp *w3m)
{
    chkNMIDBuffer(GetCurrentTab()->GetCurrentBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* render frame */

void rFrame(w3mApp *w3m)
{
    BufferPtr buf;
    // if ((buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_FRAME]) != NULL)
    // {
    //     GetCurrentTab()->SetCurrentBuffer(buf);
    //     displayCurrentbuf(B_NORMAL);
    //     return;
    // }
    // if (GetCurrentTab()->GetCurrentBuffer()->frameset == NULL)
    // {
    //     if ((buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_FRAME]) != NULL)
    //     {
    //         GetCurrentTab()->SetCurrentBuffer(buf);
    //         displayCurrentbuf(B_NORMAL);
    //     }
    //     return;
    // }
    if (w3mApp::Instance().fmInitialized)
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
    // GetCurrentTab()->Push(buf);
    if (w3mApp::Instance().fmInitialized && display_ok())
        displayCurrentbuf(B_FORCE_REDRAW);
}

void extbrz(w3mApp *w3m)
{
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't browse...", TRUE);
        return;
    }
    if (GetCurrentTab()->GetCurrentBuffer()->currentURL.IsStdin())
    {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't browse stdin", TRUE);
        return;
    }
    invoke_browser(GetCurrentTab()->GetCurrentBuffer()->currentURL.ToStr()->ptr);
}

void linkbrz(w3mApp *w3m)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;

    auto a = buf->href.RetrieveAnchor(buf->CurrentPoint());
    if (a == NULL)
        return;

    auto pu = URL::Parse(a->url).Resolve(GetCurrentTab()->GetCurrentBuffer()->BaseURL());
    invoke_browser(pu.ToStr()->ptr);
}

/* show current line number and number of lines in the entire document */
void curlno(w3mApp *w3m)
{
    int cur = 0, all = 0, col = 0, len = 0;
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    LinePtr l = buf->CurrentLine();
    if (l)
    {
        cur = l->real_linenumber;
        col = l->bwidth + buf->currentColumn + buf->rect.cursorX + 1;
        while (buf->NextLine(l) && buf->NextLine(l)->bpos)
            l = buf->NextLine(l);
        l->CalcWidth();
        len = l->bend();
    }

    if (buf->LastLine())
        all = buf->LastLine()->real_linenumber;

    Str tmp;
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE))
        tmp = Sprintf("line %d col %d/%d", cur, col, len);
    else
        tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                      (int)((double)cur * 100.0 / (double)(all ? all : 1) + 0.5), col, len);

    tmp->Push("  ");
    tmp->Push(wc_ces_to_charset_desc(buf->document_charset));

    disp_message(tmp->ptr, FALSE);
}

void dispI(w3mApp *w3m)
{
    if (!w3mApp::Instance().displayImage)
        initImage();
    if (!w3mApp::Instance().activeImage)
        return;
    w3mApp::Instance().displayImage = TRUE;
    /*
     * if (!(GetCurrentTab()->GetCurrentBuffer()->type && is_html_type(GetCurrentTab()->GetCurrentBuffer()->type)))
     * return;
     */
    GetCurrentTab()->GetCurrentBuffer()->image_flag = IMG_FLAG_AUTO;
    GetCurrentTab()->GetCurrentBuffer()->need_reshape = TRUE;
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void stopI(w3mApp *w3m)
{
    if (!w3mApp::Instance().activeImage)
        return;
    /*
     * if (!(GetCurrentTab()->GetCurrentBuffer()->type && is_html_type(GetCurrentTab()->GetCurrentBuffer()->type)))
     * return;
     */
    GetCurrentTab()->GetCurrentBuffer()->image_flag = IMG_FLAG_SKIP;
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void msToggle(w3mApp *w3m)
{
    if (w3mApp::Instance().use_mouse)
    {
        w3mApp::Instance().use_mouse = FALSE;
    }
    else
    {
        w3mApp::Instance().use_mouse = TRUE;
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

void mouse(w3mApp *w3m)
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

void movMs(w3mApp *w3m)
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }

    if ((GetTabCount() > 1 || GetMouseActionMenuStr()) && y < GetTabbarHeight())
    {
        // mouse on tab
        return;
    }

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (x >= buf->rect.rootX && y < (LINES - 1))
    {
        GetCurrentTab()->GetCurrentBuffer()->CursorXY(x, y);
    }
    displayCurrentbuf(B_NORMAL);
}

#ifdef KANJI_SYMBOLS
#define FRAME_WIDTH 2
#else
#define FRAME_WIDTH 1
#endif
void menuMs(w3mApp *w3m)
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if ((GetTabCount() > 1 || GetMouseActionMenuStr()) && y < GetTabbarHeight())
    {
        x -= FRAME_WIDTH + 1;
    }
    else if (x >= buf->rect.rootX && y < (LINES - 1))
    {
        GetCurrentTab()->GetCurrentBuffer()->CursorXY(x, y);
        displayCurrentbuf(B_NORMAL);
    }
    mainMenu(x, y);
}

// mouse 位置の tab をアクティブにする
void tabMs(w3mApp *w3m)
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }

    TabPtr tab = GetTabByPosition(x, y);
    if (!tab)
        return;
    SetCurrentTab(tab);

    displayCurrentbuf(B_FORCE_REDRAW);
}

void closeTMs(w3mApp *w3m)
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }
    auto tab = GetTabByPosition(x, y);
    if (!tab)
        return;
    deleteTab(tab);
    displayCurrentbuf(B_FORCE_REDRAW);
}

void dispVer(w3mApp *w3m)
{
    disp_message(Sprintf("w3m version %s", w3mApp::w3m_version)->ptr, TRUE);
}

void wrapToggle(w3mApp *w3m)
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

void dictword(w3mApp *w3m)
{
    execdict(inputStr("(dictionary)!", ""));
}

void dictwordat(w3mApp *w3m)
{
    execdict(GetWord(GetCurrentTab()->GetCurrentBuffer()));
}

void execCmd(w3mApp *w3m)
{
    char *data, *p;
    int cmd;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0')
    {
        data = inputStrHist("command [; ...]: ", "", w3mApp::Instance().TextHist);
        if (data == NULL)
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    ExecuteCommand(data);
    displayCurrentbuf(B_NORMAL);
}

void setAlarm(w3mApp *w3m)
{
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    auto data = searchKeyData();
    if (data == NULL || *data == '\0')
    {
        data = inputStrHist("(Alarm)sec command: ", "", w3mApp::Instance().TextHist);
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

void reinit(w3mApp *w3m)
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

void defKey(w3mApp *w3m)
{
    char *data;
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0')
    {
        data = inputStrHist("Key definition: ", "", w3mApp::Instance().TextHist);
        if (data == NULL || *data == '\0')
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    SetKeymap(allocStr(data, -1), -1, TRUE);
    displayCurrentbuf(B_NORMAL);
}

void newT(w3mApp *w3m)
{
    auto tab = CreateTabSetCurrent();
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void closeT(w3mApp *w3m)
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

void nextT(w3mApp *w3m)
{
    SelectRelativeTab(prec_num() ? prec_num() : 1);
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void prevT(w3mApp *w3m)
{
    SelectRelativeTab(-(prec_num() ? prec_num() : 1));
    displayCurrentbuf(B_REDRAW_IMAGE);
}

void tabA(w3mApp *w3m)
{
    TabPtr tab;
    if (prec_num())
        tab = GetTabByIndex(prec_num() - 1);
    else
        tab = GetCurrentTab();
    followTab(prec_num() ? tab : NULL);
}

void tabURL(w3mApp *w3m)
{
    TabPtr tab;
    if (prec_num())
        tab = GetTabByIndex(prec_num() - 1);
    else
        tab = GetCurrentTab();
    tabURL0(prec_num() ? tab : NULL,
            "Goto URL on new tab: ", FALSE);
}

void tabrURL(w3mApp *w3m)
{
    TabPtr tab;
    if (prec_num())
        tab = GetTabByIndex(prec_num() - 1);
    else
        tab = GetCurrentTab();
    tabURL0(prec_num() ? tab : NULL,
            "Goto relative URL on new tab: ", TRUE);
}

void tabR(w3mApp *w3m)
{
    MoveTab(prec_num() ? prec_num() : 1);
    displayCurrentbuf(B_FORCE_REDRAW);
}

void tabL(w3mApp *w3m)
{
    MoveTab(-(prec_num() ? prec_num() : 1));
    displayCurrentbuf(B_FORCE_REDRAW);
}
/* download panel */

void ldDL(w3mApp *w3m)
{
    int replace = FALSE, new_tab = FALSE;
    auto tab = GetCurrentTab();
    if (tab->GetCurrentBuffer()->bufferprop & BP_INTERNAL &&
        tab->GetCurrentBuffer()->buffername == w3mApp::Instance().DOWNLOAD_LIST_TITLE)
        replace = TRUE;

    // TOOD:
    // if (!FirstDL)
    // {
    //     if (replace)
    //     {
    //         if (tab->GetCurrentBuffer() == tab->GetFirstBuffer() && tab->NextBuffer(tab->GetCurrentBuffer()))
    //         {
    //             DeleteCurrentTab();
    //         }
    //         else
    //             tab->DeleteBuffer(tab->GetCurrentBuffer());
    //         displayBuffer(tab->GetCurrentBuffer(), B_FORCE_REDRAW);
    //     }
    //     return;
    // }

    auto nReload = checkDownloadList();

    auto newBuf = DownloadListBuffer(w3m);
    if (!newBuf)
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    newBuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace)
    {
        newBuf->rect = GetCurrentTab()->GetCurrentBuffer()->rect;
        newBuf->restorePosition(GetCurrentTab()->GetCurrentBuffer());
    }
    if (!replace && w3mApp::Instance().open_tab_dl_list)
    {
        CreateTabSetCurrent();
        new_tab = TRUE;
    }
    // GetCurrentTab()->Push(newBuf);
    if (replace || new_tab)
        deletePrevBuf(w3m);

    if (nReload)
        GetCurrentTab()->GetCurrentBuffer()->event = setAlarmEvent(GetCurrentTab()->GetCurrentBuffer()->event, 1, AL_IMPLICIT,
                                                                   &reload, NULL);

    displayCurrentbuf(B_FORCE_REDRAW);
}

void undoPos(w3mApp *w3m)
{
    BufferPos *b = GetCurrentTab()->GetCurrentBuffer()->undo;
    int i;
    if (GetCurrentTab()->GetCurrentBuffer()->LineCount() == 0)
        return;
    if (!b || !b->prev)
        return;
    for (i = 0; i < (prec_num() ? prec_num() : 1) && b->prev; i++, b = b->prev)
        ;
    resetPos(b);
}

void redoPos(w3mApp *w3m)
{
    BufferPos *b = GetCurrentTab()->GetCurrentBuffer()->undo;
    int i;
    if (GetCurrentTab()->GetCurrentBuffer()->LineCount() == 0)
        return;
    if (!b || !b->next)
        return;
    for (i = 0; i < (prec_num() ? prec_num() : 1) && b->next; i++, b = b->next)
        ;
    resetPos(b);
}

void mainMn(w3mApp *w3m)
{
    PopupMenu();
}

void selMn(w3mApp *w3m)
{
    PopupBufferMenu();
}

void tabMn(w3mApp *w3m)
{
    PopupTabMenu();
}
