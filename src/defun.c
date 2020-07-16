#include "fm.h"
#include "myctype.h"
#include "public.h"

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
