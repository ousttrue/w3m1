#include "fm.h"
#include "myctype.h"
#include "public.h"
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
