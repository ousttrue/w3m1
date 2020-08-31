#include <unistd.h>
#include <string_view_util.h>
#include "history.h"
#include "commands.h"
#include "command_dispatcher.h"
#include "frontend/terminal.h"
#include "frontend/screen.h"
#include "urimethod.h"
#include "indep.h"
#include "frontend/line.h"
#include "file.h"
#include "html/form.h"
#include "myctype.h"
#include "public.h"
#include "regex.h"

#include "command_dispatcher.h"
#include "frontend/menu.h"
#include "html/image.h"
#include "stream/url.h"
#include "frontend/display.h"
#include "frontend/tab.h"
#include "stream/cookie.h"
#include "stream/network.h"
#include "frontend/mouse.h"
#include "frontend/tabbar.h"
#include "mime/mimetypes.h"
#include "stream/local_cgi.h"
#include "frontend/anchor.h"
#include "loader.h"
#include "mime/mailcap.h"
#include "frontend/event.h"
#include "frontend/line.h"
#include "frontend/lineinput.h"

#include "download_list.h"
#include "rc.h"
#include "w3m.h"
#include <signal.h>

/* do nothing */
void nulcmd(w3mApp *w3m, const CommandContext &context)
{
}

void escmap(w3mApp *w3m, const CommandContext &context)
{
    char c = Terminal::getch();
    escKeyProc(c);
}

void escbmap(w3mApp *w3m, const CommandContext &context)
{
    char c = Terminal::getch();
    escbKeyProc(c);
}

void multimap(w3mApp *w3m, const CommandContext &context)
{
    char c = Terminal::getch();
    MultiKeyProc(c);
}

/* Move page forward */
void pgFore(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (w3mApp::Instance().vi_prec_num)
    {
        buf->NScroll(context.prec_num() * (buf->rect.lines - 1));
    }
    else
    {
        buf->NScroll(context.prec_num() * (buf->rect.lines - 1));
    }
}

/* Move page backward */
void pgBack(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (w3mApp::Instance().vi_prec_num)
    {
        buf->NScroll(-context.prec_num() * (buf->rect.lines - 1));
    }
    else
    {
        buf->NScroll(-context.prec_num() * (buf->rect.lines - 1));
    }
}

/* 1 line up */
void lup1(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->NScroll(context.prec_num());
}

/* 1 line down */
void ldown1(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->NScroll(-context.prec_num());
}

/* move cursor position to the center of screen */
void ctrCsrV(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    int offsety = buf->rect.lines / 2 - buf->rect.cursorY;
    buf->LineSkip(buf->TopLine(), -offsety, false);
    // buf->ArrangeLine();
}

void ctrCsrH(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    int offsetx = buf->rect.cursorX - buf->rect.cols / 2;
    buf->ColumnSkip(offsetx);
}

/* Redraw screen */
void rdrwSc(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    Screen::Instance().Clear();
    // buf->ArrangeCursor();
    // displayCurrentbuf(B_FORCE_REDRAW);
}

/* Search regular expression forward */
void srchfor(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->srch(forwardSearch, "Forward: ", context.data, context.prec_num());
}

void isrchfor(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->isrch(forwardSearch, "I-search: ", context.prec_num());
}

/* Search regular expression backward */
void srchbak(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->srch(backwardSearch, "Backward: ", context.data, context.prec_num());
}

void isrchbak(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->isrch(backwardSearch, "I-search backward: ", context.prec_num());
}

/* Search next matching */
void srchnxt(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->srch_nxtprv(false, context.prec_num());
}

/* Search previous matching */
void srchprv(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->srch_nxtprv(true, context.prec_num());
}

/* Shift screen left */
void shiftl(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    int column = buf->currentColumn;
    buf->ColumnSkip(context.prec_num() * (-buf->rect.cols + 1) + 1);
    shiftvisualpos(buf, buf->currentColumn - column);
}

/* Shift screen right */
void shiftr(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    int column = buf->currentColumn;
    buf->ColumnSkip(context.prec_num() * (buf->rect.cols - 1) - 1);
    shiftvisualpos(buf, buf->currentColumn - column);
}

void col1R(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    LinePtr l = buf->CurrentLine();
    if (l == NULL)
        return;
    for (int j = 0; j < context.prec_num(); j++)
    {
        int column = buf->currentColumn;
        buf->ColumnSkip(1);
        if (column == buf->currentColumn)
            break;
        shiftvisualpos(buf, 1);
    }
}

void col1L(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    LinePtr l = buf->CurrentLine();
    if (l == NULL)
        return;
    for (int j = 0; j < context.prec_num(); j++)
    {
        if (buf->currentColumn == 0)
            break;
        buf->ColumnSkip(-1);
        shiftvisualpos(buf, -1);
    }
}

void setEnv(w3mApp *w3m, const CommandContext &context)
{
    auto env = context.data;
    if (env.empty() || strchr(env.data(), '=') == NULL)
    {
        if (env.empty())
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist("Set environ: ", env, w3mApp::Instance().TextHist);
        if (env.empty())
        {
            return;
        }
    }

    char *value;
    if ((value = strchr(env.data(), '=')) != NULL && value > env.data())
    {
        auto var = allocStr(env.data(), value - env.data());
        value++;
        set_environ(var, value);
    }
}

void pipeBuf(w3mApp *w3m, const CommandContext &context)
{
    auto cmd = context.data;
    if (cmd.empty())
    {
        /* FIXME: gettextize? */
        cmd = inputLineHist("Pipe buffer to: ", "", IN_COMMAND, w3mApp::Instance().ShellHist);
    }
    if (cmd.size())
        cmd = conv_to_system(cmd.data());
    if (cmd.empty())
    {
        return;
    }

    auto tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    auto f = fopen(tmpf, "w");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_message(Sprintf("Can't save buffer to %s", cmd)->ptr, true);
        return;
    }
    saveBuffer(GetCurrentBuffer(), f, true);
    fclose(f);

    GetCurrentTab()->Push(URL::LocalPath(shell_quote(tmpf)));

    // auto newBuf = getpipe(myExtCommand(cmd, shell_quote(tmpf), true)->ptr);
    // if (newBuf == NULL)
    // {
    //     disp_message("Execution failed", true);
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
}

/* Execute shell command and read output ac pipe. */
void pipesh(w3mApp *w3m, const CommandContext &context)
{
    auto cmd = context.data;
    if (cmd.empty())
    {
        cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, w3mApp::Instance().ShellHist);
    }
    if (cmd.size())
        cmd = conv_to_system(cmd.data());
    if (cmd.empty())
    {
        return;
    }
    auto newBuf = getpipe(cmd.data());
    // if (buf == NULL)
    // {
    //     disp_message("Execution failed", true);
    //     return;
    // }
    // else
    // {
    //     buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    //     if (buf->type.empty())
    //         buf->type = "text/plain";
    //     GetCurrentTab()->Push(buf);
    // }
}

/* Execute shell command and load entire output to buffer */
void readsh(w3mApp *w3m, const CommandContext &context)
{
    auto cmd = context.data;
    if (cmd.empty())
    {
        cmd = inputLineHist("(read shell)!", "", IN_COMMAND, w3mApp::Instance().ShellHist);
    }
    if (cmd.size())
        cmd = conv_to_system(cmd.data());
    if (cmd.empty())
    {
        return;
    }

    auto newBuf = getshell(cmd.data());
    if (!newBuf)
    {
        /* FIXME: gettextize? */
        disp_message("Execution failed", true);
        return;
    }

    // buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    // if (buf->type.empty())
    //     buf->type = "text/plain";
    // GetCurrentTab()->Push(buf);
}

/* Execute shell command */
void execsh(w3mApp *w3m, const CommandContext &context)
{
    auto cmd = context.data;
    if (cmd.empty())
    {
        cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, w3mApp::Instance().ShellHist);
    }
    if (cmd.size())
        cmd = conv_to_system(cmd.data());
    if (cmd.size())
    {
        fmTerm();
        printf("\n");
        system(cmd.c_str());
        /* FIXME: gettextize? */
        printf("\n[Hit any key]");
        fflush(stdout);
        fmInit();
        Terminal::getch();
    }
}

/* Load file */
void ldfile(w3mApp *w3m, const CommandContext &context)
{
    auto fn = context.data;
    if (fn.empty())
    {
        /* FIXME: gettextize? */
        fn = inputFilenameHist("(Load)Filename? ", "", w3mApp::Instance().LoadHist);
    }
    if (fn.size())
        fn = conv_to_system(fn.data());
    if (fn.empty())
    {
        return;
    }

    cmd_loadfile(fn.data());
}

/* Load help file */
void ldhelp(w3mApp *w3m, const CommandContext &context)
{
    std::string_view lang = Network::Instance().AcceptLang;
    auto n = strcspn(lang.data(), ";, \t");
    auto tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
                       UrlEncode(Strnew_m_charp(w3mApp::w3m_version))->ptr,
                       UrlEncode(Strnew_m_charp(lang.substr(n)))->ptr);
    cmd_loadURL(tmp->ptr, NULL, HttpReferrerPolicy::NoReferer, NULL);
}

void movL(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    for (int i = 0; i < context.prec_num(); i++)
        buf->CursorLeft(buf->rect.cols / 2);
}

void movL1(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    for (int i = 0; i < context.prec_num(); ++i)
        buf->CursorLeft(1);
}

void movD(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    for (int i = 0; i < context.prec_num(); i++)
        buf->CursorDown((buf->rect.lines + 1) / 2);
}

void movD1(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    for (int i = 0; i < context.prec_num(); i++)
        buf->CursorDown(1);
}

void movU(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    for (int i = 0; i < context.prec_num(); i++)
        buf->CursorUp((buf->rect.lines + 1) / 2);
}

void movU1(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    for (int i = 0; i < context.prec_num(); i++)
        buf->CursorUp(1);
}

void movR(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    for (int i = 0; i < context.prec_num(); i++)
        buf->CursorRight(buf->rect.cols / 2);
}

void movR1(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    for (int i = 0; i < context.prec_num(); i++)
        buf->CursorRight(1);
}

void movLW(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->MoveLeftWord(context.prec_num());
}

void movRW(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->MoveRightWord(context.prec_num());
}

/* Quit */
void quitfm(w3mApp *w3m, const CommandContext &context)
{
    w3m->Quit();
}

/* Question and Quit */
void qquitfm(w3mApp *w3m, const CommandContext &context)
{
    w3m->Quit(true);
}

/* Select buffer */
void selBuf(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    int ok = false;
    do
    {
        char cmd;
        // auto buf = tab->SelectBuffer(GetCurrentBuffer(), &cmd);
        BufferPtr buf;
        switch (cmd)
        {
        case 'B':
            ok = true;
            break;

        case '\n':
        case ' ':
            // tab->SetCurrent(tab->GetBufferIndex(buf));
            ok = true;
            break;

        case 'D':
            // tab->DeleteBuffer(buf);
            break;

        case 'q':
            qquitfm(w3m, context);
            break;

        case 'Q':
            quitfm(w3m, context);
            break;
        }
    } while (!ok);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
void susp(w3mApp *w3m, const CommandContext &context)
{
#ifndef SIGSTOP
    char *shell;
#endif /* not SIGSTOP */
    Screen::Instance().Move((Terminal::lines() - 1), 0);
    Screen::Instance().CtrlToEolWithBGColor();
    Screen::Instance().Refresh();
    Terminal::flush();

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
    // displayCurrentbuf(B_FORCE_REDRAW);
}

void goLine(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();

    if (context.prec_num())
        buf->_goLine("^", context.prec_num());
    else if (context.data.size())
        buf->_goLine(context.data, context.prec_num());
    else
        /* FIXME: gettextize? */
        buf->_goLine(inputStr("Goto line: ", ""), context.prec_num());
}

void goLineF(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->_goLine("^", context.prec_num());
}

void goLineL(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->_goLine("$", context.prec_num());
}

/* Go to the beginning of the line */
void linbeg(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    while (buf->PrevLine(buf->CurrentLine()) && buf->CurrentLine()->bpos)
        buf->CursorUp0(1);
    buf->pos = 0;
    buf->ArrangeCursor();
}

/* Go to the bottom of the line */
void linend(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    while (buf->NextLine(buf->CurrentLine()) && buf->NextLine(buf->CurrentLine())->bpos)
        buf->CursorDown0(1);
    buf->pos = buf->CurrentLine()->len() - 1;
    buf->ArrangeCursor();
}

/* Run editor on the current buffer */
void editBf(w3mApp *w3m, const CommandContext &context)
{
    const char *fn = GetCurrentBuffer()->filename.c_str();
    Str cmd;
    if (fn == NULL ||
        (GetCurrentBuffer()->type.empty() && GetCurrentBuffer()->edit.empty()) ||           /* Reading shell */
        GetCurrentBuffer()->real_scheme != SCM_LOCAL || GetCurrentBuffer()->url.path == "-" /* file is std input  */
    )
    { /* Frame */
        disp_err_message("Can't edit other than local file", true);
        return;
    }
    if (GetCurrentBuffer()->edit.size())
    {
        // TODO:
        // cmd = unquote_mailcap(
        //     GetCurrentBuffer()->edit.c_str(),
        //     GetCurrentBuffer()->real_type.c_str(),
        //     const_cast<char *>(fn),
        //     checkHeader(GetCurrentBuffer(), "Content-Type:"), NULL);
    }
    else
    {
        cmd = myEditor(
            w3mApp::Instance().Editor.c_str(),
            shell_quote(fn),
            cur_real_linenumber(GetCurrentBuffer()));
    }
    fmTerm();
    system(cmd->ptr);
    fmInit();
    // displayCurrentbuf(B_FORCE_REDRAW);
    reload(w3m, context);
}

/* Run editor on the current screen */
void editScr(w3mApp *w3m, const CommandContext &context)
{
    char *tmpf;
    FILE *f;
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    f = fopen(tmpf, "w");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_err_message(Sprintf("Can't open %s", tmpf)->ptr, true);
        return;
    }
    saveBuffer(GetCurrentBuffer(), f, true);
    fclose(f);
    fmTerm();
    system(myEditor(w3mApp::Instance().Editor.c_str(), shell_quote(tmpf),
                    cur_real_linenumber(GetCurrentBuffer()))
               ->ptr);
    fmInit();
    unlink(tmpf);
    // displayCurrentbuf(B_FORCE_REDRAW);
}

/* Set / unset mark */
void _mark(w3mApp *w3m, const CommandContext &context)
{
    if (!w3mApp::Instance().use_mark)
        return;

    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    auto l = buf->CurrentLine();
    l->propBuf()[buf->pos] ^= PE_MARK;
    // displayCurrentbuf(B_FORCE_REDRAW);
}

/* Go to next mark */
void nextMk(w3mApp *w3m, const CommandContext &context)
{
    LinePtr l;
    int i;
    if (!w3mApp::Instance().use_mark)
        return;
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
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
                return;
            }
        }
        l = buf->NextLine(l);
        i = 0;
    }
    /* FIXME: gettextize? */
    disp_message("No mark exist after here", true);
}
/* Go to previous mark */

void prevMk(w3mApp *w3m, const CommandContext &context)
{
    LinePtr l;
    int i;
    if (!w3mApp::Instance().use_mark)
        return;
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
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
                return;
            }
        }
        l = buf->PrevLine(l);
        if (l != NULL)
            i = l->len() - 1;
    }
    /* FIXME: gettextize? */
    disp_message("No mark exist before here", true);
}
/* Mark place to which the regular expression matches */

void reMark(w3mApp *w3m, const CommandContext &context)
{
    if (!w3mApp::Instance().use_mark)
        return;

    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    auto str = context.data;
    if (str.empty())
    {
        str = inputStrHist("(Mark)Regexp: ", MarkString(), w3mApp::Instance().TextHist);
        if (str.empty())
        {
            return;
        }
    }
    str = buf->conv_search_string(str, w3mApp::Instance().DisplayCharset);
    if ((str = regexCompile(str.data(), 1)).size())
    {
        disp_message(str.data(), true);
        return;
    }
    SetMarkString(str.data());

    for (int i = 0; i < buf->m_document->LineCount(); ++i)
    {
        auto l = buf->m_document->GetLine(i);
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
    }
}

/* view inline image */
void followI(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    auto a = buf->m_document->img.RetrieveAnchor(buf->CurrentPoint());
    if (a == NULL)
        return;

    auto l = buf->CurrentLine();
    /* FIXME: gettextize? */
    message(Sprintf("loading %s", a->url)->ptr, 0, 0);
    Screen::Instance().Refresh();
    Terminal::flush();

    GetCurrentTab()->Push(URL::Parse(a->url, &buf->url));
    // auto newBuf = loadGeneralFile(URL::Parse(a->url), &buf->currentURL);
    // if (newBuf == NULL)
    // {
    //     /* FIXME: gettextize? */
    //     char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    //     disp_err_message(emsg, false);
    // }
    // else
    // {
    //     GetCurrentTab()->Push(newBuf);
    // }
}

/* submit form */
void submitForm(w3mApp *w3m, const CommandContext &context)
{
    _followForm(true);
}
/* go to the top anchor */

void topA(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;

    auto &hl = buf->m_document->hmarklist;
    if (hl.empty())
        return;

    int hseq = 0;
    if (context.prec_num() > hl.size())
        hseq = hl.size() - 1;
    else if (context.prec_num() > 0)
        hseq = context.prec_num() - 1;

    BufferPoint po = {};
    AnchorPtr an;
    do
    {
        if (hseq >= hl.size())
            return;
        po = hl[hseq];
        an = buf->m_document->href.RetrieveAnchor(po);
        if (an == NULL)
            an = buf->m_document->formitem.RetrieveAnchor(po);
        hseq++;
    } while (an == NULL);

    buf->Goto(po);
}

/* go to the last anchor */
void lastA(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;

    auto &hl = buf->m_document->hmarklist;
    int hseq;
    if (hl.empty())
        return;
    if (context.prec_num() >= hl.size())
        hseq = 0;
    else if (context.prec_num() > 0)
        hseq = hl.size() - context.prec_num();
    else
        hseq = hl.size() - 1;

    BufferPoint po = {};
    AnchorPtr an;
    do
    {
        if (hseq < 0)
            return;
        po = hl[hseq];
        an = buf->m_document->href.RetrieveAnchor(po);
        if (an == NULL)
            an = buf->m_document->formitem.RetrieveAnchor(po);
        hseq--;
    } while (an == NULL);

    buf->Goto(po);
}

/* go to the next anchor */
void nextA(w3mApp *w3m, const CommandContext &context)
{
    _nextA(false, context.prec_num());
}

/* go to the previous anchor */
void prevA(w3mApp *w3m, const CommandContext &context)
{
    _prevA(false, context.prec_num());
}

/* go to the next visited anchor */
void nextVA(w3mApp *w3m, const CommandContext &context)
{
    _nextA(true, context.prec_num());
}

/* go to the previous visited anchor */
void prevVA(w3mApp *w3m, const CommandContext &context)
{
    _prevA(true, context.prec_num());
}

static std::tuple<bool, int, int> isMap(const BufferPtr &buf, const AnchorPtr a)
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
void followA(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    auto l = buf->CurrentLine();

    bool map = false;
    int x = -1;
    int y = -1;

    {
        auto a = buf->m_document->img.RetrieveAnchor(buf->CurrentPoint());
        if (a && a->image && a->image->map)
        {
            _followForm(false);
            return;
        }

        std::tie(map, x, y) = isMap(buf, a);
    }

    auto a = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (a == NULL)
    {
        _followForm(false);
        return;
    }

    if (a->url.size() && a->url[0] == '#')
    { /* index within this buffer */
        gotoLabel(a->url.c_str() + 1);
        return;
    }

    auto u = URL::Parse(a->url, &buf->url);
    if (u.ToStr()->Cmp(buf->url.ToStr()) == 0)
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
    //     auto buf = GetCurrentBuffer();
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

    tab->Push(URL::Parse(url, &buf->url));
}

/* go to the next left anchor */
void nextL(w3mApp *w3m, const CommandContext &context)
{
    nextX(-1, 0, context.prec_num());
}

/* go to the next left-up anchor */

void nextLU(w3mApp *w3m, const CommandContext &context)
{
    nextX(-1, -1, context.prec_num());
}
/* go to the next right anchor */

void nextR(w3mApp *w3m, const CommandContext &context)
{
    nextX(1, 0, context.prec_num());
}
/* go to the next right-down anchor */

void nextRD(w3mApp *w3m, const CommandContext &context)
{
    nextX(1, 1, context.prec_num());
}
/* go to the next downward anchor */

void nextD(w3mApp *w3m, const CommandContext &context)
{
    nextY(1, context.prec_num());
}
/* go to the next upward anchor */

void nextU(w3mApp *w3m, const CommandContext &context)
{
    nextY(-1, context.prec_num());
}

/* go to the next bufferr */
void nextBf(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    int i = 0;
    auto prec = context.prec_num() ? context.prec_num() : 1;
    for (; i < prec; i++)
    {
        if (!tab->Forward())
        {
            break;
        }
    }
}

/* go to the previous bufferr */
void prevBf(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto prec = context.prec_num() ? context.prec_num() : 1;
    int i = 0;
    for (; i < prec; i++)
    {
        if (!tab->Back())
        {
            break;
        }
    }
}

/* delete current buffer and back to the previous buffer */
void backBf(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    tab->Back(true);
    // auto buf = GetCurrentBuffer()->linkBuffer[LB_N_FRAME];
    // if (!tab->CheckBackBuffer())
    // {
    //     if (close_tab_back && GetTabCount() >= 1)
    //     {
    //         deleteTab(GetCurrentTab());
    //         displayCurrentbuf(B_FORCE_REDRAW);
    //     }
    //     else
    //         /* FIXME: gettextize? */
    //         disp_message("Can't back...", true);
    //     return;
    // }
    // tab->DeleteBuffer(GetCurrentBuffer());
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
    //         if (buf == GetCurrentBuffer())
    //         {
    //             rFrame(w3m);
    //             GetCurrentBuffer()->LineSkip(
    //                 GetCurrentBuffer()->FirstLine(), top - 1,
    //                 false);
    //             GetCurrentBuffer()->Goto({linenumber, pos});
    //             // GetCurrentBuffer()->pos = pos;
    //             // GetCurrentBuffer()->ArrangeCursor();
    //             GetCurrentBuffer()->currentColumn = currentColumn;
    //             formResetBuffer(GetCurrentBuffer(), formitem);
    //         }
    //     }
    //     else if (w3mApp::Instance().RenderFrame && buf == GetCurrentBuffer())
    //     {
    //         auto tab = GetCurrentTab();
    //         tab->DeleteBuffer(GetCurrentBuffer());
    //     }
    // }
    // displayCurrentbuf(B_FORCE_REDRAW);
}

// for local CGI replace
void deletePrevBuf(w3mApp *w3m, const CommandContext &context)
{
    GetCurrentTab()->DeleteBack();
}

void goURL(w3mApp *w3m, const CommandContext &context)
{
    goURL0(context.data, "Goto URL: ", false);
}

void gorURL(w3mApp *w3m, const CommandContext &context)
{
    goURL0(context.data, "Goto relative URL: ", true);
}

/* load bookmark */
void ldBmark(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    tab->Push(URL::LocalPath(w3m->BookmarkFile));
}

/* Add current to bookmark */
void adBmark(w3mApp *w3m, const CommandContext &context)
{
    auto tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s&charset=%s",
                       UrlEncode((localCookie()))->ptr,
                       UrlEncode((Strnew(w3mApp::Instance().BookmarkFile)))->ptr,
                       UrlEncode((GetCurrentBuffer()->url.ToStr()))->ptr,

                       UrlEncode((wc_conv_strict(GetCurrentBuffer()->buffername.c_str(),
                                                 w3mApp::Instance().InnerCharset,
                                                 w3mApp::Instance().BookmarkCharset)))
                           ->ptr,
                       wc_ces_to_charset(w3mApp::Instance().BookmarkCharset));
    auto request = Form::Create("", "post");
    request->body = tmp->ptr;
    request->length = tmp->Size();
    cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, HttpReferrerPolicy::NoReferer,
                request);
}
/* option setting */

void ldOpt(w3mApp *w3m, const CommandContext &context)
{
    // cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}
/* error message list */

void msgs(w3mApp *w3m, const CommandContext &context)
{
    // cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}
/* page info */

void pginfo(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    // BufferPtr buf;
    // if ((buf = GetCurrentBuffer()->linkBuffer[LB_N_INFO]) != NULL)
    // {
    //     GetCurrentTab()->SetCurrentBuffer(buf);
    //     displayCurrentbuf(B_NORMAL);
    //     return;
    // }

    // if ((buf = GetCurrentBuffer()->linkBuffer[LB_INFO]) != NULL)
    //     tab->DeleteBuffer(buf);
    auto newBuf = page_info_panel(GetCurrentBuffer());
    // cmd_loadBuffer(newBuf, BP_NORMAL, LB_INFO);
    // tab->Push(newBuf);
}
/* link menu */

void linkMn(w3mApp *w3m, const CommandContext &context)
{
    auto l = link_menu(GetCurrentBuffer());
    if (!l || l->url().empty())
        return;

    if (l->url()[0] == '#')
    {
        gotoLabel(l->url().substr(1));
        return;
    }

    auto p_url = URL::Parse(l->url(), &GetCurrentBuffer()->url);
    pushHashHist(w3mApp::Instance().URLHist, p_url.ToStr()->ptr);
    cmd_loadURL(l->url(), &GetCurrentBuffer()->url, HttpReferrerPolicy::StrictOriginWhenCrossOrigin, NULL);
}
/* accesskey */

void accessKey(w3mApp *w3m, const CommandContext &context)
{
    anchorMn(accesskey_menu, true);
}
/* set an option */

void setOpt(w3mApp *w3m, const CommandContext &context)
{
    auto opt = context.data;
    if (opt.empty() || strchr(opt.data(), '=') == NULL)
    {
        if (opt.size())
        {
            auto v = get_param_option(opt.data());
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist("Set option: ", opt, w3mApp::Instance().TextHist);
        if (opt.empty())
        {
            // displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    if (set_param_option(opt.data()))
        sync_with_option();
    // displayCurrentbuf(B_REDRAW_IMAGE);
}

/* list menu */
void listMn(w3mApp *w3m, const CommandContext &context)
{
    anchorMn(list_menu, true);
}

void movlistMn(w3mApp *w3m, const CommandContext &context)
{
    anchorMn(list_menu, false);
}

/* link,anchor,image list */
void linkLst(w3mApp *w3m, const CommandContext &context)
{
    auto buf = GetCurrentBuffer();
    auto newBuf = link_list_panel(GetCurrentBuffer());
    if (newBuf != NULL)
    {
        newBuf->m_document->document_charset = buf->m_document->document_charset;
        // cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
    }
}

/* cookie list */
void cooLst(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    tab->Push(URL::Parse("w3m://cookielist"));
}

/* History page */
void ldHist(w3mApp *w3m, const CommandContext &context)
{
    // cmd_loadBuffer(historyBuffer(w3mApp::Instance().URLHist), BP_NO_URL, LB_NOLINK);
}

/* download HREF link */
void svA(w3mApp *w3m, const CommandContext &context)
{
    w3mApp::Instance().do_download = true;
    followA(w3m, context);
    w3mApp::Instance().do_download = false;
}

/* download IMG link */
void svI(w3mApp *w3m, const CommandContext &context)
{
    w3mApp::Instance().do_download = true;
    followI(w3m, context);
    w3mApp::Instance().do_download = false;
}

/* save buffer */
void svBuf(w3mApp *w3m, const CommandContext &context)
{
    char *qfile = NULL;
    int is_pipe;
    auto file = context.data;
    if (file.empty())
    {
        /* FIXME: gettextize? */
        qfile = inputLineHist("Save buffer to: ", "", IN_COMMAND, w3mApp::Instance().SaveHist);
        if (qfile == NULL || *qfile == '\0')
        {
            // displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file.c_str());

    FILE *f = nullptr;
    if (file.size() && file[0] == '|')
    {
        is_pipe = true;
        f = popen(file.c_str() + 1, "w");
    }
    else
    {
        if (qfile)
        {
            file = unescape_spaces(Strnew(qfile))->ptr;
            file = conv_to_system(file.data());
        }
        file = expandPath(file.data());
        if (checkOverWrite(file.data()) < 0)
        {
            // displayCurrentbuf(B_NORMAL);
            return;
        }
        f = fopen(file.data(), "w");
        is_pipe = false;
    }
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        char *emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
        disp_err_message(emsg, true);
        return;
    }
    saveBuffer(GetCurrentBuffer(), f, true);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
    // displayCurrentbuf(B_NORMAL);
}

/* save source */
void svSrc(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->sourcefile.empty())
        return;

    w3mApp::Instance().PermitSaveToPipe = true;
    const char *file = nullptr;
    // if (GetCurrentBuffer()->real_scheme == SCM_LOCAL)
    //     file = conv_from_system(guess_save_name(NULL,
    //                                             GetCurrentBuffer()->currentURL.real_file));
    // else
    //     file = guess_save_name(GetCurrentBuffer(), GetCurrentBuffer()->currentURL.path);
    doFileCopy(GetCurrentBuffer()->sourcefile.c_str(), file);
    w3mApp::Instance().PermitSaveToPipe = false;
    // displayCurrentbuf(B_NORMAL);
}

/* peek URL */
void peekURL(w3mApp *w3m, const CommandContext &context)
{
    _peekURL(0, context.prec_num());
}

/* peek URL of image */
void peekIMG(w3mApp *w3m, const CommandContext &context)
{
    _peekURL(1, context.prec_num());
}

void curURL(w3mApp *w3m, const CommandContext &context)
{
    if (GetCurrentBuffer()->bufferprop & BP_INTERNAL)
        return;

    // static Str s = NULL;
    // static Lineprop *p = NULL;
    // static int offset = 0;
    // if (CurrentKey() == PrevKey() && s)
    // {
    //     if (s->Size() - offset >= Terminal::columns())
    //         offset++;
    //     else if (s->Size() <= offset) /* bug ? */
    //         offset = 0;
    // }
    // else

    auto offset = 0;
    auto s = currentURL();
    if (w3mApp::Instance().DecodeURL)
        s = Strnew(url_unquote_conv(s->ptr, WC_CES_NONE));

    auto propstr = PropertiedString::create(s);
    int n = context.prec_num();
    if (n > 1 && s->Size() > (n - 1) * (Terminal::columns() - 1))
        offset = (n - 1) * (Terminal::columns() - 1);
    while (offset < s->Size() && propstr.propBuf()[offset] & PC_WCHAR2)
        offset++;
    disp_message_nomouse(&s->ptr[offset], true);
}

/* view HTML source */
void vwSrc(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();

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
        // if (buf->pagerSource && buf->type == "text/plain")
        // {
        //     // pager
        //     CharacterEncodingScheme old_charset;
        //     bool old_fix_width_conv;

        //     Str tmpf = tmpfname(TMPF_SRC, NULL);
        //     auto f = fopen(tmpf->ptr, "w");
        //     if (f == NULL)
        //         return;

        //     old_charset = w3mApp::Instance().DisplayCharset;
        //     old_fix_width_conv = WcOption.fix_width_conv;
        //     w3mApp::Instance().DisplayCharset = (buf->m_document->document_charset != WC_CES_US_ASCII)
        //                                             ? buf->m_document->document_charset
        //                                             : WC_CES_NONE;
        //     WcOption.fix_width_conv = false;

        //     saveBufferBody(buf, f, true);

        //     w3mApp::Instance().DisplayCharset = old_charset;
        //     WcOption.fix_width_conv = old_fix_width_conv;

        //     fclose(f);
        //     buf->sourcefile = tmpf->ptr;
        // }
        // else
        {
            return;
        }
    }

    {
        auto newBuf = Buffer::Create(buf->url);
        if (is_html_type(buf->type.c_str()))
        {
            newBuf->type = "text/plain";
            if (is_html_type(buf->real_type))
                newBuf->real_type = "text/plain";
            else
                newBuf->real_type = buf->real_type;
            newBuf->buffername = Sprintf("source of %s", buf->buffername)->ptr;
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
        }
        else
        {
            return;
        }
        newBuf->url = buf->url;
        newBuf->real_scheme = buf->real_scheme;
        newBuf->filename = buf->filename;
        newBuf->sourcefile = buf->sourcefile;
        newBuf->header_source = buf->header_source;
        newBuf->search_header = buf->search_header;
        newBuf->m_document->document_charset = buf->m_document->document_charset;
        newBuf->need_reshape = true;
        // buf->Reshape();
        GetCurrentTab()->Push(URL::Parse("w3m://htmlsource", &buf->url));
    }
}

/* reload */
void reload(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->bufferprop & BP_INTERNAL)
    {
        if (buf->buffername == w3mApp::DOWNLOAD_LIST_TITLE)
        {
            ldDL(w3m, context);
            return;
        }

        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", true);
        return;
    }

    if (buf->url.IsStdin())
    {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't reload stdin", true);
        return;
    }

    BufferPtr fbuf = NULL;
    auto sbuf = buf->Copy();

    //
    // form
    //
    FormPtr request;
    auto multipart = true;
    if (GetCurrentBuffer()->form_submit)
    {
        request = GetCurrentBuffer()->form_submit->parent.lock();
        if (request->method == FORM_METHOD_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
        {
            auto form = GetCurrentBuffer()->form_submit->parent.lock();
            auto query = form->Query(GetCurrentBuffer()->form_submit, multipart);
            struct stat st;
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
    Screen::Instance().Refresh();
    Terminal::flush();

    auto old_charset = w3m->DocumentCharset;
    if (buf->m_document->document_charset != WC_CES_US_ASCII)
        w3m->DocumentCharset = buf->m_document->document_charset;

    w3m->SearchHeader = buf->search_header;
    w3m->DefaultType = Strnew(buf->real_type)->ptr;

    {
        auto newBuf = loadGeneralFile(buf->url.NoCache(), NULL, HttpReferrerPolicy::NoReferer, request);

        w3m->DocumentCharset = old_charset;

        w3m->SearchHeader = false;
        w3m->DefaultType.clear();
        if (multipart)
            unlink(request->body);

        if (newBuf == NULL)
        {
            /* FIXME: gettextize? */
            disp_err_message("Can't reload...", true);
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
            vwSrc(w3m, context);
            // if (GetCurrentBuffer() != newBuf)
            //     GetCurrentTab()->DeleteBuffer(newBuf);
        }
        GetCurrentBuffer()->search_header = sbuf->search_header;
        GetCurrentBuffer()->form_submit = sbuf->form_submit;
        if (GetCurrentBuffer()->m_document->LineCount())
        {
            GetCurrentBuffer()->rect = sbuf->rect;
            GetCurrentBuffer()->restorePosition(sbuf);
        }
        // displayCurrentbuf(B_FORCE_REDRAW);
    }
}
/* reshape */

void reshape(w3mApp *w3m, const CommandContext &context)
{
    GetCurrentBuffer()->need_reshape = true;
    // GetCurrentBuffer()->Reshape();
    // displayCurrentbuf(B_FORCE_REDRAW);
}

void docCSet(w3mApp *w3m, const CommandContext &context)
{
    auto buf = GetCurrentBuffer();
    auto cs = context.data;
    if (cs.empty())
        /* FIXME: gettextize? */
        cs = inputStr("Document charset: ",
                      wc_ces_to_charset(buf->m_document->document_charset));
    auto charset = wc_guess_charset_short(cs.data(), WC_CES_NONE);
    if (charset == 0)
    {
        // displayCurrentbuf(B_NORMAL);
        return;
    }
    _docCSet(charset);
}

void defCSet(w3mApp *w3m, const CommandContext &context)
{
    auto cs = context.data;
    if (cs.empty())
        /* FIXME: gettextize? */
        cs = inputStr("Default document charset: ",
                      wc_ces_to_charset(w3mApp::Instance().DocumentCharset));
    auto charset = wc_guess_charset_short(cs.data(), WC_CES_NONE);
    if (charset != 0)
        w3mApp::Instance().DocumentCharset = charset;
    // displayCurrentbuf(B_NORMAL);
}

void chkURL(w3mApp *w3m, const CommandContext &context)
{
    chkURLBuffer(GetCurrentBuffer());
    // displayCurrentbuf(B_FORCE_REDRAW);
}

void chkWORD(w3mApp *w3m, const CommandContext &context)
{
    char *p;
    int spos, epos;
    p = getCurWord(GetCurrentBuffer(), &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(GetCurrentBuffer(), GetCurrentBuffer()->CurrentLine(), spos, epos);
    // displayCurrentbuf(B_FORCE_REDRAW);
}

void extbrz(w3mApp *w3m, const CommandContext &context)
{
    if (GetCurrentBuffer()->bufferprop & BP_INTERNAL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't browse...", true);
        return;
    }
    if (GetCurrentBuffer()->url.IsStdin())
    {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't browse stdin", true);
        return;
    }
    invoke_browser(GetCurrentBuffer()->url.ToStr()->ptr, context.data, context.prec_num());
}

void linkbrz(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;

    auto a = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (a == NULL)
        return;

    auto pu = URL::Parse(a->url, &GetCurrentBuffer()->url);
    invoke_browser(pu.ToStr()->ptr, context.data, context.prec_num());
}

/* show current line number and number of lines in the entire document */
void curlno(w3mApp *w3m, const CommandContext &context)
{
    int cur = 0, all = 0, col = 0, len = 0;
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
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

    if (buf->m_document->LastLine())
        all = buf->m_document->LastLine()->real_linenumber;

    Str tmp;
    // if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE))
    //     tmp = Sprintf("line %d col %d/%d", cur, col, len);
    // else
        tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                      (int)((double)cur * 100.0 / (double)(all ? all : 1) + 0.5), col, len);

    tmp->Push("  ");
    tmp->Push(wc_ces_to_charset_desc(buf->m_document->document_charset));

    disp_message(tmp->ptr, false);
}

void dispI(w3mApp *w3m, const CommandContext &context)
{
    ImageManager::Instance().displayImage = true;
    ImageManager::Instance().initImage();
    if (!ImageManager::Instance().activeImage)
        return;
    /*
     * if (!(GetCurrentBuffer()->type && is_html_type(GetCurrentBuffer()->type)))
     * return;
     */
    GetCurrentBuffer()->image_flag = IMG_FLAG_AUTO;
    GetCurrentBuffer()->need_reshape = true;
    // displayCurrentbuf(B_REDRAW_IMAGE);
}

void stopI(w3mApp *w3m, const CommandContext &context)
{
    if (!ImageManager::Instance().activeImage)
        return;
    /*
     * if (!(GetCurrentBuffer()->type && is_html_type(GetCurrentBuffer()->type)))
     * return;
     */
    GetCurrentBuffer()->image_flag = IMG_FLAG_SKIP;
    // displayCurrentbuf(B_REDRAW_IMAGE);
}

void msToggle(w3mApp *w3m, const CommandContext &context)
{
    if (w3mApp::Instance().use_mouse)
    {
        w3mApp::Instance().use_mouse = false;
    }
    else
    {
        w3mApp::Instance().use_mouse = true;
    }
    // displayCurrentbuf(B_FORCE_REDRAW);
}

void mouse(w3mApp *w3m, const CommandContext &context)
{
    auto btn = static_cast<MouseBtnAction>((unsigned char)Terminal::getch() - 32);
    int x = (unsigned char)Terminal::getch() - 33;
    if (x < 0)
        x += 0x100;
    int y = (unsigned char)Terminal::getch() - 33;
    if (y < 0)
        y += 0x100;
    if (x < 0 || x >= Terminal::columns() || y < 0 || y > (Terminal::lines() - 1))
        return;
    process_mouse(btn, x, y);
}

void movMs(w3mApp *w3m, const CommandContext &context)
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }

    if ((GetTabCount() > 1 || GetMouseActionMenuStr().size()) && y < GetTabbarHeight())
    {
        // mouse on tab
        return;
    }

    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (x >= buf->rect.rootX && y < (Terminal::lines() - 1))
    {
        GetCurrentBuffer()->CursorXY(x, y);
    }
    // displayCurrentbuf(B_NORMAL);
}

#ifdef KANJI_SYMBOLS
#define FRAME_WIDTH 2
#else
#define FRAME_WIDTH 1
#endif
void menuMs(w3mApp *w3m, const CommandContext &context)
{
    int x, y;
    if (!TryGetMouseActionPosition(&x, &y))
    {
        return;
    }

    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if ((GetTabCount() > 1 || GetMouseActionMenuStr().size()) && y < GetTabbarHeight())
    {
        x -= FRAME_WIDTH + 1;
    }
    else if (x >= buf->rect.rootX && y < (Terminal::lines() - 1))
    {
        GetCurrentBuffer()->CursorXY(x, y);
        // displayCurrentbuf(B_NORMAL);
    }
    mainMenu(x, y);
}

// mouse 位置の tab をアクティブにする
void tabMs(w3mApp *w3m, const CommandContext &context)
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
}

void closeTMs(w3mApp *w3m, const CommandContext &context)
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
}

void dispVer(w3mApp *w3m, const CommandContext &context)
{
    disp_message(Sprintf("w3m version %s", w3mApp::w3m_version)->ptr, true);
}

void wrapToggle(w3mApp *w3m, const CommandContext &context)
{
    if (w3m->WrapSearch)
    {
        w3m->WrapSearch = false;
        /* FIXME: gettextize? */
        disp_message("Wrap search off", true);
    }
    else
    {
        w3m->WrapSearch = true;
        /* FIXME: gettextize? */
        disp_message("Wrap search on", true);
    }
}

void dictword(w3mApp *w3m, const CommandContext &context)
{
    execdict(inputStr("(dictionary)!", ""));
}

void dictwordat(w3mApp *w3m, const CommandContext &context)
{
    execdict(GetWord(GetCurrentBuffer()));
}

void execCmd(w3mApp *w3m, const CommandContext &context)
{
    auto data = context.data;
    if (data.empty())
    {
        data = inputStrHist("command [; ...]: ", "", w3mApp::Instance().TextHist);
        if (data.empty())
        {
            return;
        }
    }
    CommandDispatcher::Instance().ExecuteCommand(data);
    // displayCurrentbuf(B_NORMAL);
}

void setAlarm(w3mApp *w3m, const CommandContext &context)
{
    std::string_view data = context.data;
    if (data.empty())
    {
        data = inputStrHist("(Alarm)sec command: ", "", w3mApp::Instance().TextHist);
        if (data.empty())
        {
            // displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    assert(data.size());

    std::string _sec;
    std::tie(data, _sec) = getWord(data);
    int sec = atoi(_sec.c_str());
    Command cmd = nullptr;
    if (sec > 0)
    {
        std::string word;
        std::tie(data, word) = getWord(data);
        cmd = getFuncList(word);
    }

    if (cmd)
    {
        std::string word;
        std::tie(data, word) = getQWord(data);
        setAlarmEvent(DefaultAlarm(), sec, AL_EXPLICIT, cmd, word.data());
        disp_message_nsec(Sprintf("%dsec %s %s", sec, "ID",
                                  word.c_str())
                              ->ptr,
                          false, 1, false, true);
    }
    else
    {
        setAlarmEvent(DefaultAlarm(), 0, AL_UNSET, &nulcmd, NULL);
    }
    // displayCurrentbuf(B_NORMAL);
}

void reinit(w3mApp *w3m, const CommandContext &context)
{
    std::string_view resource = context.data;
    if (resource.empty())
    {
        init_rc();
        sync_with_option();
        initCookie();
        // displayCurrentbuf(B_REDRAW_IMAGE);
        return;
    }
    if (svu::ic_eq(resource, "CONFIG") || svu::ic_eq(resource, "RC"))
    {
        init_rc();
        sync_with_option();
        // displayCurrentbuf(B_REDRAW_IMAGE);
        return;
    }

    if (svu::ic_eq(resource, "COOKIE"))
    {
        initCookie();
        return;
    }

    if (svu::ic_eq(resource, "KEYMAP"))
    {
        initKeymap(true);
        return;
    }

    if (svu::ic_eq(resource, "MAILCAP"))
    {
        initMailcap();
        return;
    }

    if (svu::ic_eq(resource, "MOUSE"))
    {
        initMouseAction();
        // displayCurrentbuf(B_REDRAW_IMAGE);
        return;
    }

    if (svu::ic_eq(resource, "MENU"))
    {
        initMenu();
        return;
    }

    if (svu::ic_eq(resource, "MIMETYPES"))
    {
        initMimeTypes();
        return;
    }

    if (svu::ic_eq(resource, "URIMETHODS"))
    {
        initURIMethods();
        return;
    }

    disp_err_message(Sprintf("Don't know how to reinitialize '%s'", resource)->ptr, false);
}

void defKey(w3mApp *w3m, const CommandContext &context)
{
    std::string_view data = context.data;
    if (data.empty())
    {
        data = inputStrHist("Key definition: ", "", w3mApp::Instance().TextHist);
        if (data.empty())
        {
            // displayCurrentbuf(B_NORMAL);
            return;
        }
    }
    SetKeymap(allocStr(data.data(), -1), -1, true);
    // displayCurrentbuf(B_NORMAL);
}

void newT(w3mApp *w3m, const CommandContext &context)
{
    auto tab = CreateTabSetCurrent();
}

void closeT(w3mApp *w3m, const CommandContext &context)
{
    if (GetTabCount() <= 1)
        return;
    TabPtr tab;
    if (context.prec_num())
        tab = GetTabByIndex(context.prec_num() - 1);
    else
        tab = GetCurrentTab();
    if (tab)
        deleteTab(tab);
}

void nextT(w3mApp *w3m, const CommandContext &context)
{
    SelectRelativeTab(context.prec_num() ? context.prec_num() : 1);
}

void prevT(w3mApp *w3m, const CommandContext &context)
{
    SelectRelativeTab(-(context.prec_num() ? context.prec_num() : 1));
}

void tabA(w3mApp *w3m, const CommandContext &context)
{
    TabPtr tab;
    if (context.prec_num())
        tab = GetTabByIndex(context.prec_num() - 1);
    else
        tab = GetCurrentTab();
    // followTab(context.prec_num() ? tab : NULL, context);
}

void tabURL(w3mApp *w3m, const CommandContext &context)
{
    TabPtr tab;
    if (context.prec_num())
        tab = GetTabByIndex(context.prec_num() - 1);
    else
        tab = GetCurrentTab();
    tabURL0(context.prec_num() ? tab : NULL,
            context.data,
            "Goto URL on new tab: ", false);
}

void tabrURL(w3mApp *w3m, const CommandContext &context)
{
    TabPtr tab;
    if (context.prec_num())
        tab = GetTabByIndex(context.prec_num() - 1);
    else
        tab = GetCurrentTab();
    tabURL0(context.prec_num() ? tab : NULL,
            context.data,
            "Goto relative URL on new tab: ", true);
}

void tabR(w3mApp *w3m, const CommandContext &context)
{
    MoveTab(context.prec_num() ? context.prec_num() : 1);
}

void tabL(w3mApp *w3m, const CommandContext &context)
{
    MoveTab(-(context.prec_num() ? context.prec_num() : 1));
}

/* download panel */
void ldDL(w3mApp *w3m, const CommandContext &context)
{
    bool replace = false;
    auto tab = GetCurrentTab();
    if (GetCurrentBuffer()->bufferprop & BP_INTERNAL &&
        GetCurrentBuffer()->buffername == w3mApp::Instance().DOWNLOAD_LIST_TITLE)
        replace = true;

    // TOOD:
    // if (!FirstDL)
    // {
    //     if (replace)
    //     {
    //         if (GetCurrentBuffer() == tab->GetFirstBuffer() && tab->NextBuffer(GetCurrentBuffer()))
    //         {
    //             DeleteCurrentTab();
    //         }
    //         else
    //             tab->DeleteBuffer(GetCurrentBuffer());
    //         displayBuffer(GetCurrentBuffer(), B_FORCE_REDRAW);
    //     }
    //     return;
    // }

    auto nReload = checkDownloadList();

    auto newBuf = DownloadListBuffer(w3m, context);
    if (!newBuf)
    {
        return;
    }
    newBuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace)
    {
        newBuf->rect = GetCurrentBuffer()->rect;
        newBuf->restorePosition(GetCurrentBuffer());
    }

    bool new_tab = false;
    if (!replace && w3mApp::Instance().open_tab_dl_list)
    {
        CreateTabSetCurrent();
        new_tab = true;
    }
    // GetCurrentTab()->Push(newBuf);
    if (replace || new_tab)
        deletePrevBuf(w3m, context);

    if (nReload)
        GetCurrentBuffer()->m_document->event = setAlarmEvent(GetCurrentBuffer()->m_document->event, 1, AL_IMPLICIT,
                                                              &reload, NULL);
}

void undoPos(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->undoPos(context.prec_num());
}

void redoPos(w3mApp *w3m, const CommandContext &context)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    buf->redoPos();
}

void mainMn(w3mApp *w3m, const CommandContext &context)
{
    PopupMenu(context.data);
}

void selMn(w3mApp *w3m, const CommandContext &context)
{
    PopupBufferMenu();
}

void tabMn(w3mApp *w3m, const CommandContext &context)
{
    PopupTabMenu();
}
