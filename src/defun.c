#include "fm.h"
#include "myctype.h"
#include "public.h"
#include "regex.h"
#include <signal.h>

DEFUN(nulcmd, NOTHING NULL @@@, "Do nothing")
{				/* do nothing */
}

#ifdef __EMX__
DEFUN(pcmap, PCMAP, "pcmap")
{
    w3mFuncList[(int)PcKeymap[(int)getch()]].func();
}
#else				/* not __EMX__ */
void
pcmap(void)
{
}
#endif

DEFUN(escmap, ESCMAP, "ESC map")
{
    char c;
    c = getch();
    if (IS_ASCII(c))
	escKeyProc((int)c, K_ESC, EscKeymap);
}

DEFUN(escbmap, ESCBMAP, "ESC [ map")
{
    char c;
    c = getch();
    if (IS_DIGIT(c)) {
	escdmap(c);
	return;
    }
    if (IS_ASCII(c))
	escKeyProc((int)c, K_ESCB, EscBKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap")
{
    char c;
    c = getch();
    if (IS_ASCII(c)) {
	CurrentKey = K_MULTI | (CurrentKey << 16) | c;
	escKeyProc((int)c, 0, NULL);
    }
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Move to next page")
{
    if (vi_prec_num)
	nscroll(searchKeyNum() * (Currentbuf->LINES - 1), B_NORMAL);
    else
	nscroll(prec_num() ? searchKeyNum() : searchKeyNum()
		* (Currentbuf->LINES - 1), prec_num() ? B_SCROLL : B_NORMAL);
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Move to previous page")
{
    if (vi_prec_num)
	nscroll(-searchKeyNum() * (Currentbuf->LINES - 1), B_NORMAL);
    else
	nscroll(-(prec_num ? searchKeyNum() : searchKeyNum()
		  * (Currentbuf->LINES - 1)), prec_num ? B_SCROLL : B_NORMAL);
}

/* 1 line up */
DEFUN(lup1, UP, "Scroll up one line")
{
    nscroll(searchKeyNum(), B_SCROLL);
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll down one line")
{
    nscroll(-searchKeyNum(), B_SCROLL);
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Move to the center column")
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
	return;
    offsety = Currentbuf->LINES / 2 - Currentbuf->cursorY;
    if (offsety != 0) {
#if 0
	Currentbuf->currentLine = lineSkip(Currentbuf,
					   Currentbuf->currentLine, offsety,
					   FALSE);
#endif
	Currentbuf->topLine =
	    lineSkip(Currentbuf, Currentbuf->topLine, -offsety, FALSE);
	arrangeLine(Currentbuf);
	displayBuffer(Currentbuf, B_NORMAL);
    }
}

DEFUN(ctrCsrH, CENTER_H, "Move to the center line")
{
    int offsetx;
    if (Currentbuf->firstLine == NULL)
	return;
    offsetx = Currentbuf->cursorX - Currentbuf->COLS / 2;
    if (offsetx != 0) {
	columnSkip(Currentbuf, offsetx);
	arrangeCursor(Currentbuf);
	displayBuffer(Currentbuf, B_NORMAL);
    }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Redraw screen")
{
    clear();
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Search regular expression forward */
DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward")
{
    srch(forwardSearch, "Forward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward")
{
    isrch(forwardSearch, "I-search: ");
}

/* Search regular expression backward */
DEFUN(srchbak, SEARCH_BACK, "Search backward")
{
    srch(backwardSearch, "Backward: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward")
{
    isrch(backwardSearch, "I-search backward: ");
}

/* Search next matching */
DEFUN(srchnxt, SEARCH_NEXT, "Search next regexp")
{
    srch_nxtprv(0);
}

/* Search previous matching */
DEFUN(srchprv, SEARCH_PREV, "Search previous regexp")
{
    srch_nxtprv(1);
}

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left")
{
    int column;

    if (Currentbuf->firstLine == NULL)
	return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (-Currentbuf->COLS + 1) + 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    int column;

    if (Currentbuf->firstLine == NULL)
	return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (Currentbuf->COLS - 1) - 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1R, RIGHT, "Shift screen one column right")
{
    Buffer *buf = Currentbuf;
    Line *l = buf->currentLine;
    int j, column, n = searchKeyNum();

    if (l == NULL)
	return;
    for (j = 0; j < n; j++) {
	column = buf->currentColumn;
	columnSkip(Currentbuf, 1);
	if (column == buf->currentColumn)
	    break;
	shiftvisualpos(Currentbuf, 1);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1L, LEFT, "Shift screen one column")
{
    Buffer *buf = Currentbuf;
    Line *l = buf->currentLine;
    int j, n = searchKeyNum();

    if (l == NULL)
	return;
    for (j = 0; j < n; j++) {
	if (buf->currentColumn == 0)
	    break;
	columnSkip(Currentbuf, -1);
	shiftvisualpos(Currentbuf, -1);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(setEnv, SETENV, "Set environment variable")
{
    char *env;
    char *var, *value;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
	if (env != NULL && *env != '\0')
	    env = Sprintf("%s=", env)->ptr;
	env = inputStrHist("Set environ: ", env, TextHist);
	if (env == NULL || *env == '\0') {
	    displayBuffer(Currentbuf, B_NORMAL);
	    return;
	}
    }
    if ((value = strchr(env, '=')) != NULL && value > env) {
	var = allocStr(env, value - env);
	value++;
	set_environ(var, value);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(pipeBuf, PIPE_BUF, "Send rendered document to pipe")
{
    Buffer *buf;
    char *cmd, *tmpf;
    FILE *f;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
	/* FIXME: gettextize? */
	cmd = inputLineHist("Pipe buffer to: ", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
	cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    f = fopen(tmpf, "w");
    if (f == NULL) {
	/* FIXME: gettextize? */
	disp_message(Sprintf("Can't save buffer to %s", cmd)->ptr, TRUE);
	return;
    }
    saveBuffer(Currentbuf, f, TRUE);
    fclose(f);
    buf = getpipe(myExtCommand(cmd, shell_quote(tmpf), TRUE)->ptr);
    if (buf == NULL) {
	disp_message("Execution failed", TRUE);
	return;
    }
    else {
	buf->filename = cmd;
	buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME,
				  conv_from_system(cmd))->ptr;
	buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	if (buf->type == NULL)
	    buf->type = "text/plain";
	buf->currentURL.file = "-";
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command and read output ac pipe. */
DEFUN(pipesh, PIPE_SHELL, "Execute shell command and browse")
{
    Buffer *buf;
    char *cmd;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
	cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
	cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    buf = getpipe(cmd);
    if (buf == NULL) {
	disp_message("Execution failed", TRUE);
	return;
    }
    else {
	buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	if (buf->type == NULL)
	    buf->type = "text/plain";
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command and load entire output to buffer */
DEFUN(readsh, READ_SHELL, "Execute shell command and load")
{
    Buffer *buf;
    char *cmd;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
	cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
	cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    auto prevtrap = mySignal(SIGINT, intTrap);
    crmode();
    buf = getshell(cmd);
    mySignal(SIGINT, prevtrap);
    term_raw();
    if (buf == NULL) {
	/* FIXME: gettextize? */
	disp_message("Execution failed", TRUE);
	return;
    }
    else {
	buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	if (buf->type == NULL)
	    buf->type = "text/plain";
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command")
{
    char *cmd;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
	cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
	cmd = conv_to_system(cmd);
    if (cmd != NULL && *cmd != '\0') {
	fmTerm();
	printf("\n");
	system(cmd);
	/* FIXME: gettextize? */
	printf("\n[Hit any key]");
	fflush(stdout);
	fmInit();
	getch();
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Load file */
DEFUN(ldfile, LOAD, "Load local file")
{
    char *fn;

    fn = searchKeyData();
    if (fn == NULL || *fn == '\0') {
	/* FIXME: gettextize? */
	fn = inputFilenameHist("(Load)Filename? ", NULL, LoadHist);
    }
    if (fn != NULL)
	fn = conv_to_system(fn);
    if (fn == NULL || *fn == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    cmd_loadfile(fn);
}

/* Load help file */
DEFUN(ldhelp, HELP, "View help")
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

DEFUN(movL, MOVE_LEFT,
      "Move cursor left (a half screen shift at the left edge)")
{
    _movL(Currentbuf->COLS / 2);
}

DEFUN(movL1, MOVE_LEFT1, "Move cursor left (1 columns shift at the left edge)")
{
    _movL(1);
}

DEFUN(movD, MOVE_DOWN,
      "Move cursor down (a half screen scroll at the end of screen)")
{
    _movD((Currentbuf->LINES + 1) / 2);
}

DEFUN(movD1, MOVE_DOWN1,
      "Move cursor down (1 line scroll at the end of screen)")
{
    _movD(1);
}

DEFUN(movU, MOVE_UP,
      "Move cursor up (a half screen scroll at the top of screen)")
{
    _movU((Currentbuf->LINES + 1) / 2);
}

DEFUN(movU1, MOVE_UP1, "Move cursor up (1 line scrol at the top of screen)")
{
    _movU(1);
}

DEFUN(movR, MOVE_RIGHT,
      "Move cursor right (a half screen shift at the right edge)")
{
    _movR(Currentbuf->COLS / 2);
}

DEFUN(movR1, MOVE_RIGHT1,
      "Move cursor right (1 columns shift at the right edge)")
{
    _movR(1);
}

DEFUN(movLW, PREV_WORD, "Move to previous word")
{
    char *lb;
    Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
	return;

    for (i = 0; i < n; i++) {
	pline = Currentbuf->currentLine;
	ppos = Currentbuf->pos;

	if (prev_nonnull_line(Currentbuf->currentLine) < 0)
	    goto end;

	while (1) {
	    l = Currentbuf->currentLine;
	    lb = l->lineBuf;
	    while (Currentbuf->pos > 0) {
		int tmp = Currentbuf->pos;
		prevChar(tmp, l);
		if (is_wordchar(getChar(&lb[tmp])))
		    break;
		Currentbuf->pos = tmp;
	    }
	    if (Currentbuf->pos > 0)
		break;
	    if (prev_nonnull_line(Currentbuf->currentLine->prev) < 0) {
		Currentbuf->currentLine = pline;
		Currentbuf->pos = ppos;
		goto end;
	    }
	    Currentbuf->pos = Currentbuf->currentLine->len;
	}

	l = Currentbuf->currentLine;
	lb = l->lineBuf;
	while (Currentbuf->pos > 0) {
	    int tmp = Currentbuf->pos;
	    prevChar(tmp, l);
	    if (!is_wordchar(getChar(&lb[tmp])))
		break;
	    Currentbuf->pos = tmp;
	}
    }
  end:
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movRW, NEXT_WORD, "Move to next word")
{
    char *lb;
    Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
	return;

    for (i = 0; i < n; i++) {
	pline = Currentbuf->currentLine;
	ppos = Currentbuf->pos;

	if (next_nonnull_line(Currentbuf->currentLine) < 0)
	    goto end;

	l = Currentbuf->currentLine;
	lb = l->lineBuf;
	while (Currentbuf->pos < l->len &&
	       is_wordchar(getChar(&lb[Currentbuf->pos])))
	    nextChar(&Currentbuf->pos, l);

	while (1) {
	    while (Currentbuf->pos < l->len &&
		   !is_wordchar(getChar(&lb[Currentbuf->pos])))
		nextChar(&Currentbuf->pos, l);
	    if (Currentbuf->pos < l->len)
		break;
	    if (next_nonnull_line(Currentbuf->currentLine->next) < 0) {
		Currentbuf->currentLine = pline;
		Currentbuf->pos = ppos;
		goto end;
	    }
	    Currentbuf->pos = 0;
	    l = Currentbuf->currentLine;
	    lb = l->lineBuf;
	}
    }
  end:
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Quit */
DEFUN(quitfm, ABORT EXIT, "Quit w3m without confirmation")
{
    _quitfm(FALSE);
}

/* Question and Quit */
DEFUN(qquitfm, QUIT, "Quit w3m")
{
    _quitfm(confirm_on_quit);
}

/* Select buffer */
DEFUN(selBuf, SELECT, "Go to buffer selection panel")
{
    Buffer *buf;
    int ok;
    char cmd;

    ok = FALSE;
    do {
	buf = selectBuffer(Firstbuf, Currentbuf, &cmd);
	switch (cmd) {
	case 'B':
	    ok = TRUE;
	    break;
	case '\n':
	case ' ':
	    Currentbuf = buf;
	    ok = TRUE;
	    break;
	case 'D':
	    delBuffer(buf);
	    if (Firstbuf == NULL) {
		/* No more buffer */
		Firstbuf = nullBuffer();
		Currentbuf = Firstbuf;
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

    for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
	if (buf == Currentbuf)
	    continue;
#ifdef USE_IMAGE
	deleteImage(buf);
#endif
	if (clear_buffer)
	    tmpClearBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
DEFUN(susp, INTERRUPT SUSPEND, "Stop loading document")
{
#ifndef SIGSTOP
    char *shell;
#endif				/* not SIGSTOP */
    move(LASTLINE, 0);
    clrtoeolx();
    refresh();
    fmTerm();
#ifndef SIGSTOP
    shell = getenv("SHELL");
    if (shell == NULL)
	shell = "/bin/sh";
    system(shell);
#else				/* SIGSTOP */
    kill((pid_t) 0, SIGSTOP);
#endif				/* SIGSTOP */
    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(goLine, GOTO_LINE, "Go to specified line")
{

    char *str = searchKeyData();
    if (prec_num)
	_goLine("^");
    else if (str)
	_goLine(str);
    else
	/* FIXME: gettextize? */
	_goLine(inputStr("Goto line: ", ""));
}

DEFUN(goLineF, BEGIN, "Go to the first line")
{
    _goLine("^");
}

DEFUN(goLineL, END, "Go to the last line")
{
    _goLine("$");
}

/* Go to the beginning of the line */
DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of line")
{
    if (Currentbuf->firstLine == NULL)
	return;
    while (Currentbuf->currentLine->prev && Currentbuf->currentLine->bpos)
	cursorUp0(Currentbuf, 1);
    Currentbuf->pos = 0;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}


/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of line")
{
    if (Currentbuf->firstLine == NULL)
	return;
    while (Currentbuf->currentLine->next
	   && Currentbuf->currentLine->next->bpos)
	cursorDown0(Currentbuf, 1);
    Currentbuf->pos = Currentbuf->currentLine->len - 1;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit current document")
{
    char *fn = Currentbuf->filename;
    Str cmd;

    if (fn == NULL || Currentbuf->pagerSource != NULL ||	/* Behaving as a pager */
	(Currentbuf->type == NULL && Currentbuf->edit == NULL) ||	/* Reading shell */
	Currentbuf->real_scheme != SCM_LOCAL || !strcmp(Currentbuf->currentURL.file, "-") ||	/* file is std input  */
	Currentbuf->bufferprop & BP_FRAME) {	/* Frame */
	disp_err_message("Can't edit other than local file", TRUE);
	return;
    }
    if (Currentbuf->edit)
	cmd = unquote_mailcap(Currentbuf->edit, Currentbuf->real_type, fn,
			      checkHeader(Currentbuf, "Content-Type:"), NULL);
    else
	cmd = myEditor(Editor, shell_quote(fn),
		       cur_real_linenumber(Currentbuf));
    fmTerm();
    system(cmd->ptr);
    fmInit();

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    reload();
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit currently rendered document")
{
    char *tmpf;
    FILE *f;

    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    f = fopen(tmpf, "w");
    if (f == NULL) {
	/* FIXME: gettextize? */
	disp_err_message(Sprintf("Can't open %s", tmpf)->ptr, TRUE);
	return;
    }
    saveBuffer(Currentbuf, f, TRUE);
    fclose(f);
    fmTerm();
    system(myEditor(Editor, shell_quote(tmpf),
		    cur_real_linenumber(Currentbuf))->ptr);
    fmInit();
    unlink(tmpf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Set / unset mark */
DEFUN(_mark, MARK, "Set/unset mark")
{
    Line *l;
    if (!use_mark)
	return;
    if (Currentbuf->firstLine == NULL)
	return;
    l = Currentbuf->currentLine;
    l->propBuf[Currentbuf->pos] ^= PE_MARK;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Go to next mark */
DEFUN(nextMk, NEXT_MARK, "Move to next word")
{
    Line *l;
    int i;

    if (!use_mark)
	return;
    if (Currentbuf->firstLine == NULL)
	return;
    i = Currentbuf->pos + 1;
    l = Currentbuf->currentLine;
    if (i >= l->len) {
	i = 0;
	l = l->next;
    }
    while (l != NULL) {
	for (; i < l->len; i++) {
	    if (l->propBuf[i] & PE_MARK) {
		Currentbuf->currentLine = l;
		Currentbuf->pos = i;
		arrangeCursor(Currentbuf);
		displayBuffer(Currentbuf, B_NORMAL);
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
DEFUN(prevMk, PREV_MARK, "Move to previous mark")
{
    Line *l;
    int i;

    if (!use_mark)
	return;
    if (Currentbuf->firstLine == NULL)
	return;
    i = Currentbuf->pos - 1;
    l = Currentbuf->currentLine;
    if (i < 0) {
	l = l->prev;
	if (l != NULL)
	    i = l->len - 1;
    }
    while (l != NULL) {
	for (; i >= 0; i--) {
	    if (l->propBuf[i] & PE_MARK) {
		Currentbuf->currentLine = l;
		Currentbuf->pos = i;
		arrangeCursor(Currentbuf);
		displayBuffer(Currentbuf, B_NORMAL);
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
DEFUN(reMark, REG_MARK, "Set mark using regexp")
{
    Line *l;
    char *str;
    char *p, *p1, *p2;

    if (!use_mark)
	return;
    str = searchKeyData();
    if (str == NULL || *str == '\0') {
	str = inputStrHist("(Mark)Regexp: ", MarkString(), TextHist);
	if (str == NULL || *str == '\0') {
	    displayBuffer(Currentbuf, B_NORMAL);
	    return;
	}
    }
    str = conv_search_string(str, DisplayCharset);
    if ((str = regexCompile(str, 1)) != NULL) {
	disp_message(str, TRUE);
	return;
    }
    SetMarkString(str);
    for (l = Currentbuf->firstLine; l != NULL; l = l->next) {
	p = l->lineBuf;
	for (;;) {
	    if (regexMatch(p, &l->lineBuf[l->len] - p, p == l->lineBuf) == 1) {
		matchedPosition(&p1, &p2);
		l->propBuf[p1 - l->lineBuf] |= PE_MARK;
		p = p2;
	    }
	    else
		break;
	}
    }

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "View image")
{
    Line *l;
    Anchor *a;
    Buffer *buf;

    if (Currentbuf->firstLine == NULL)
	return;
    l = Currentbuf->currentLine;

    a = retrieveCurrentImg(Currentbuf);
    if (a == NULL)
	return;
    /* FIXME: gettextize? */
    message(Sprintf("loading %s", a->url)->ptr, 0, 0);
    refresh();
    buf = loadGeneralFile(a->url, baseURL(Currentbuf), NULL, 0, NULL);
    if (buf == NULL) {
	/* FIXME: gettextize? */
	char *emsg = Sprintf("Can't load %s", a->url)->ptr;
	disp_err_message(emsg, FALSE);
    }
    else if (buf != NO_BUFFER) {
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form")
{
    _followForm(TRUE);
}

/* go to the top anchor */
DEFUN(topA, LINK_BEGIN, "Go to the first link")
{
    HmarkerList *hl = Currentbuf->hmarklist;
    BufferPoint *po;
    Anchor *an;
    int hseq = 0;

    if (Currentbuf->firstLine == NULL)
	return;
    if (!hl || hl->nmark == 0)
	return;

    if (prec_num() > hl->nmark)
	hseq = hl->nmark - 1;
    else if (prec_num() > 0)
	hseq = prec_num() - 1;
    do {
	if (hseq >= hl->nmark)
	    return;
	po = hl->marks + hseq;
	an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
	if (an == NULL)
	    an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
	hseq++;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Go to the last link")
{
    HmarkerList *hl = Currentbuf->hmarklist;
    BufferPoint *po;
    Anchor *an;
    int hseq;

    if (Currentbuf->firstLine == NULL)
	return;
    if (!hl || hl->nmark == 0)
	return;

    if (prec_num() >= hl->nmark)
	hseq = 0;
    else if (prec_num() > 0)
	hseq = hl->nmark - prec_num();
    else
	hseq = hl->nmark - 1;
    do {
	if (hseq < 0)
	    return;
	po = hl->marks + hseq;
	an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
	if (an == NULL)
	    an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
	hseq--;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next anchor */
DEFUN(nextA, NEXT_LINK, "Move to next link")
{
    _nextA(FALSE);
}

/* go to the previous anchor */
DEFUN(prevA, PREV_LINK, "Move to previous link")
{
    _prevA(FALSE);
}

/* go to the next visited anchor */
DEFUN(nextVA, NEXT_VISITED, "Move to next visited link")
{
    _nextA(TRUE);
}

/* go to the previous visited anchor */
DEFUN(prevVA, PREV_VISITED, "Move to previous visited link")
{
    _prevA(TRUE);
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Go to current link")
{
    Line *l;
    Anchor *a;
    ParsedURL u;
#ifdef USE_IMAGE
    int x = 0, y = 0, map = 0;
#endif
    char *url;

    if (Currentbuf->firstLine == NULL)
	return;
    l = Currentbuf->currentLine;

#ifdef USE_IMAGE
    a = retrieveCurrentImg(Currentbuf);
    if (a && a->image && a->image->map) {
	_followForm(FALSE);
	return;
    }
    if (a && a->image && a->image->ismap) {
	getMapXY(Currentbuf, a, &x, &y);
	map = 1;
    }
#else
    a = retrieveCurrentMap(Currentbuf);
    if (a) {
	_followForm(FALSE);
	return;
    }
#endif
    a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL) {
	_followForm(FALSE);
	return;
    }
    if (*a->url == '#') {	/* index within this buffer */
	gotoLabel(a->url + 1);
	return;
    }
    parseURL2(a->url, &u, baseURL(Currentbuf));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&Currentbuf->currentURL)) == 0) {
	/* index within this buffer */
	if (u.label) {
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
#endif				/* USE_NNTP */
    url = a->url;
#ifdef USE_IMAGE
    if (map)
	url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;
#endif

    if (check_target && open_tab_blank && a->target &&
	(!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
	Buffer *buf;

	_newT();
	buf = Currentbuf;
	loadLink(url, a->target, a->referer, NULL);
	if (buf != Currentbuf)
	    delBuffer(buf);
	else
	    deleteTab(CurrentTab);
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
    loadLink(url, a->target, a->referer, NULL);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next left anchor */
DEFUN(nextL, NEXT_LEFT, "Move to next left link")
{
    nextX(-1, 0);
}

/* go to the next left-up anchor */
DEFUN(nextLU, NEXT_LEFT_UP, "Move to next left (or upward) link")
{
    nextX(-1, -1);
}

/* go to the next right anchor */
DEFUN(nextR, NEXT_RIGHT, "Move to next right link")
{
    nextX(1, 0);
}

/* go to the next right-down anchor */
DEFUN(nextRD, NEXT_RIGHT_DOWN, "Move to next right (or downward) link")
{
    nextX(1, 1);
}

/* go to the next downward anchor */
DEFUN(nextD, NEXT_DOWN, "Move to next downward link")
{
    nextY(1);
}

/* go to the next upward anchor */
DEFUN(nextU, NEXT_UP, "Move to next upward link")
{
    nextY(-1);
}

/* go to the next bufferr */
DEFUN(nextBf, NEXT, "Move to next buffer")
{
    Buffer *buf;
    int i;

    for (i = 0; i < PREC_NUM(); i++) {
	buf = prevBuffer(Firstbuf, Currentbuf);
	if (!buf) {
	    if (i == 0)
		return;
	    break;
	}
	Currentbuf = buf;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Move to previous buffer")
{
    Buffer *buf;
    int i;

    for (i = 0; i < PREC_NUM(); i++) {
	buf = Currentbuf->nextBuffer;
	if (!buf) {
	    if (i == 0)
		return;
	    break;
	}
	Currentbuf = buf;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* delete current buffer and back to the previous buffer */
DEFUN(backBf, BACK, "Back to previous buffer")
{
    Buffer *buf = Currentbuf->linkBuffer[LB_N_FRAME];

    if (!checkBackBuffer(Currentbuf)) {
	if (close_tab_back && nTab >= 1) {
	    deleteTab(CurrentTab);
	    displayBuffer(Currentbuf, B_FORCE_REDRAW);
	}
	else
	    /* FIXME: gettextize? */
	    disp_message("Can't back...", TRUE);
	return;
    }

    delBuffer(Currentbuf);

    if (buf) {
	if (buf->frameQ) {
	    struct frameset *fs;
	    long linenumber = buf->frameQ->linenumber;
	    long top = buf->frameQ->top_linenumber;
	    int pos = buf->frameQ->pos;
	    int currentColumn = buf->frameQ->currentColumn;
	    AnchorList *formitem = buf->frameQ->formitem;

	    fs = popFrameTree(&(buf->frameQ));
	    deleteFrameSet(buf->frameset);
	    buf->frameset = fs;

	    if (buf == Currentbuf) {
		rFrame();
		Currentbuf->topLine = lineSkip(Currentbuf,
					       Currentbuf->firstLine, top - 1,
					       FALSE);
		gotoLine(Currentbuf, linenumber);
		Currentbuf->pos = pos;
		Currentbuf->currentColumn = currentColumn;
		arrangeCursor(Currentbuf);
		formResetBuffer(Currentbuf, formitem);
	    }
	}
	else if (RenderFrame && buf == Currentbuf) {
	    delBuffer(Currentbuf);
	}
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF,
      "Delete previous buffer (mainly for local-CGI)")
{
    Buffer *buf = Currentbuf->nextBuffer;
    if (buf)
	delBuffer(buf);
}
