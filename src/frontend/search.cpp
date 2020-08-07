#include "fm.h"
#include "regex.h"
#include "file.h"
#include "public.h"
#include "frontend/buffer.h"
#include "frontend/line.h"
#include "frontend/display.h"
#include "frontend/tabbar.h"
#include <signal.h>
#include <errno.h>
#include <unistd.h>


static void
set_mark(Line *l, int pos, int epos)
{
    for (; pos < epos && pos < l->size; pos++)
	l->propBuf[pos] |= PE_MARK;
}

#ifdef USE_MIGEMO
/* Migemo: romaji --> kana+kanji in regexp */
static FILE *migemor = NULL, *migemow = NULL;
static int migemo_running;
static int migemo_pid = 0;

void
init_migemo()
{
    migemo_active = migemo_running = use_migemo;
    if (migemor != NULL)
	fclose(migemor);
    if (migemow != NULL)
	fclose(migemow);
    migemor = migemow = NULL;
    if (migemo_pid)
	kill(migemo_pid, SIGKILL);
    migemo_pid = 0;
}

static int
open_migemo(char *migemo_command)
{
    migemo_pid = open_pipe_rw(&migemor, &migemow);
    if (migemo_pid < 0)
	goto err0;
    if (migemo_pid == 0) {
	/* child */
	setup_child(FALSE, 2, -1);
	myExec(migemo_command);
	/* XXX: ifdef __EMX__, use start /f ? */
    }
    return 1;
  err0:
    migemo_pid = 0;
    migemo_active = migemo_running = 0;
    return 0;
}

static char *
migemostr(char *str)
{
    Str tmp = NULL;
    if (migemor == NULL || migemow == NULL)
	if (open_migemo(migemo_command) == 0)
	    return str;
    fprintf(migemow, "%s\n", conv_to_system(str));
  again:
    if (fflush(migemow) != 0) {
	switch (errno) {
	case EINTR:
	    goto again;
	default:
	    goto err;
	}
    }
    tmp = Str_conv_from_system(Strfgets(migemor));
    Strchop(tmp);
    if (tmp->length == 0)
	goto err;
    return conv_search_string(tmp->ptr, SystemCharset);
  err:
    /* XXX: backend migemo is not working? */
    init_migemo();
    migemo_active = migemo_running = 0;
    return str;
}
#endif				/* USE_MIGEMO */

#ifdef USE_M17N
/* normalize search string */
char *
conv_search_string(char *str, CharacterEncodingScheme f_ces)
{
    if (SearchConv && !WcOption.pre_conv &&
	GetCurrentTab()->GetCurrentBuffer()->document_charset != f_ces)
	str = wtf_conv_fit(str, GetCurrentTab()->GetCurrentBuffer()->document_charset);
    return str;
}
#endif

int
forwardSearch(BufferPtr buf, char *str)
{
    char *p, *first, *last;
    Line *l, *begin;
    int wrapped = FALSE;
    int pos;

#ifdef USE_MIGEMO
    if (migemo_active > 0) {
	if (((p = regexCompile(migemostr(str), IgnoreCase)) != NULL)
	    && ((p = regexCompile(str, IgnoreCase)) != NULL)) {
	    message(p, 0, 0);
	    return SR_NOTFOUND;
	}
    }
    else
#endif
    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
	message(p, 0, 0);
	return SR_NOTFOUND;
    }
    l = buf->CurrentLine();
    if (l == NULL) {
	return SR_NOTFOUND;
    }
    pos = buf->pos;
    if (l->bpos) {
	pos += l->bpos;
	while (l->bpos && buf->PrevLine(l))
	    l = buf->PrevLine(l);
    }
    begin = l;
#ifdef USE_M17N
    while (pos < l->size && l->propBuf[pos] & PC_WCHAR2)
	pos++;
#endif
    if (pos < l->size && regexMatch(&l->lineBuf[pos], l->size - pos, 0) == 1) {
	matchedPosition(&first, &last);
	pos = first - l->lineBuf;
	while (pos >= l->len && buf->NextLine(l) && buf->NextLine(l)->bpos) {
	    pos -= l->len;
	    l = buf->NextLine(l);
	}
	buf->pos = pos;
	if (l != buf->CurrentLine())
	    buf->GotoLine(l->linenumber);
	buf->ArrangeCursor();
	set_mark(l, pos, pos + last - first);
	return SR_FOUND;
    }
    for (l = buf->NextLine(l);; l = buf->NextLine(l)) {
	if (l == NULL) {
	    if (buf->pagerSource) {
		l = getNextPage(buf, 1);
		if (l == NULL) {
		    if (WrapSearch && !wrapped) {
			l = buf->FirstLine();
			wrapped = TRUE;
		    }
		    else {
			break;
		    }
		}
	    }
	    else if (WrapSearch) {
		l = buf->FirstLine();
		wrapped = TRUE;
	    }
	    else {
		break;
	    }
	}
	if (l->bpos)
	    continue;
	if (regexMatch(l->lineBuf, l->size, 1) == 1) {
	    matchedPosition(&first, &last);
	    pos = first - l->lineBuf;
	    while (pos >= l->len && buf->NextLine(l) && buf->NextLine(l)->bpos) {
		pos -= l->len;
		l = buf->NextLine(l);
	    }
	    buf->pos = pos;
	    buf->SetCurrentLine(l);
	    buf->GotoLine(l->linenumber);
	    buf->ArrangeCursor();
	    set_mark(l, pos, pos + last - first);
	    return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
	}
	if (wrapped && l == begin)	/* no match */
	    break;
    }
    return SR_NOTFOUND;
}

int
backwardSearch(BufferPtr buf, char *str)
{
    char *p, *q, *found, *found_last, *first, *last;
    Line *l, *begin;
    int wrapped = FALSE;
    int pos;

#ifdef USE_MIGEMO
    if (migemo_active > 0) {
	if (((p = regexCompile(migemostr(str), IgnoreCase)) != NULL)
	    && ((p = regexCompile(str, IgnoreCase)) != NULL)) {
	    message(p, 0, 0);
	    return SR_NOTFOUND;
	}
    }
    else
#endif
    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
	message(p, 0, 0);
	return SR_NOTFOUND;
    }
    l = buf->CurrentLine();
    if (l == NULL) {
	return SR_NOTFOUND;
    }
    pos = buf->pos;
    if (l->bpos) {
	pos += l->bpos;
	while (l->bpos && buf->PrevLine(l))
	    l = buf->PrevLine(l);
    }
    begin = l;
    if (pos > 0) {
	pos--;
#ifdef USE_M17N
	while (pos > 0 && l->propBuf[pos] & PC_WCHAR2)
	    pos--;
#endif
	p = &l->lineBuf[pos];
	found = NULL;
	found_last = NULL;
	q = l->lineBuf;
	while (regexMatch(q, &l->lineBuf[l->size] - q, q == l->lineBuf) == 1) {
	    matchedPosition(&first, &last);
	    if (first <= p) {
		found = first;
		found_last = last;
	    }
	    if (q - l->lineBuf >= l->size)
		break;
	    q++;
#ifdef USE_M17N
	    while (q - l->lineBuf < l->size
		   && l->propBuf[q - l->lineBuf] & PC_WCHAR2)
		q++;
#endif
	    if (q > p)
		break;
	}
	if (found) {
	    pos = found - l->lineBuf;
	    while (pos >= l->len && buf->NextLine(l) && buf->NextLine(l)->bpos) {
		pos -= l->len;
		l = buf->NextLine(l);
	    }
	    buf->pos = pos;
	    if (l != buf->CurrentLine())
		buf->GotoLine(l->linenumber);
	    buf->ArrangeCursor();
	    set_mark(l, pos, pos + found_last - found);
	    return SR_FOUND;
	}
    }
    for (l = buf->PrevLine(l);; l = buf->PrevLine(l)) {
	if (l == NULL) {
	    if (WrapSearch) {
		l = buf->LastLine();
		wrapped = TRUE;
	    }
	    else {
		break;
	    }
	}
	found = NULL;
	found_last = NULL;
	q = l->lineBuf;
	while (regexMatch(q, &l->lineBuf[l->size] - q, q == l->lineBuf) == 1) {
	    matchedPosition(&first, &last);
	    found = first;
	    found_last = last;
	    if (q - l->lineBuf >= l->size)
		break;
	    q++;
#ifdef USE_M17N
	    while (q - l->lineBuf < l->size
		   && l->propBuf[q - l->lineBuf] & PC_WCHAR2)
		q++;
#endif
	}
	if (found) {
	    pos = found - l->lineBuf;
	    while (pos >= l->len && buf->NextLine(l) && buf->NextLine(l)->bpos) {
		pos -= l->len;
		l = buf->NextLine(l);
	    }
	    buf->pos = pos;
	    buf->GotoLine(l->linenumber);
	    buf->ArrangeCursor();
	    set_mark(l, pos, pos + found_last - found);
	    return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
	}
	if (wrapped && l == begin)	/* no match */
	    break;
    }
    return SR_NOTFOUND;
}
