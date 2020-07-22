
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "rc.h"
#include "etc.h"
#include "display.h"

#ifdef USE_HISTORY
Buffer *
historyBuffer(Hist *hist)
{
    Str src = Strnew();
    HistItem *item;
    char *p, *q;

    /* FIXME: gettextize? */
    src->Push("<html>\n<head><title>History Page</title></head>\n");
    src->Push("<body>\n<h1>History Page</h1>\n<hr>\n");
    src->Push("<ol>\n");
    if (hist && hist->list) {
	for (item = hist->list->last; item; item = item->prev) {
	    q = html_quote((char *)item->ptr);
	    if (DecodeURL)
		p = html_quote(url_unquote_conv((char *)item->ptr, 0));
	    else
		p = q;
	    src->Push("<li><a href=\"");
	    src->Push(q);
	    src->Push("\">");
	    src->Push(p);
	    src->Push("</a>\n");
	}
    }
    src->Push("</ol>\n</body>\n</html>");
    return loadHTMLString(src);
}

void
loadHistory(Hist *hist)
{
    FILE *f;
    Str line;

    if (hist == NULL)
	return;
    if ((f = fopen(rcFile(HISTORY_FILE), "rt")) == NULL)
	return;

    while (!feof(f)) {
	line = Strfgets(f);
    line->Strip();
	if (line->Size() == 0)
	    continue;
	pushHist(hist, url_quote(line->ptr));
    }
    fclose(f);
}

void
saveHistory(Hist *hist, size_t size)
{
    FILE *f;
    HistItem *item;
    char *tmpf;

    if (hist == NULL || hist->list == NULL)
	return;
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    if ((f = fopen(tmpf, "w")) == NULL) {
	/* FIXME: gettextize? */
	disp_err_message("Can't open history", FALSE);
	return;
    }
    for (item = hist->list->first; item && hist->list->nitem > size;
	 item = item->next)
	size++;
    for (; item; item = item->next)
	fprintf(f, "%s\n", (char *)item->ptr);
    if (fclose(f) == EOF) {
	/* FIXME: gettextize? */
	disp_err_message("Can't save history", FALSE);
	return;
    }
    rename(tmpf, rcFile(HISTORY_FILE));
}
#endif				/* USE_HISTORY */

Hist *
newHist()
{
    Hist *hist;

    hist = New(Hist);
    hist->list = (HistList *)newGeneralList();
    hist->current = NULL;
    hist->hash = NULL;
    return hist;
}

Hist *
copyHist(Hist *hist)
{
    Hist *newHistory;
    HistItem *item;

    if (hist == NULL)
	return NULL;
    newHistory = newHist();
    for (item = hist->list->first; item; item = item->next)
	pushHist(newHistory, (char *)item->ptr);
    return newHistory;
}

HistItem *
unshiftHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = (HistItem *)newListItem((void *)allocStr(ptr, -1),
				   (ListItem *)hist->list->first, NULL);
    if (hist->list->first)
	hist->list->first->prev = item;
    else
	hist->list->last = item;
    hist->list->first = item;
    hist->list->nitem++;
    return item;
}

HistItem *
pushHist(Hist *hist, const char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = (HistItem *)newListItem((void *)allocStr(ptr, -1),
				   NULL, (ListItem *)hist->list->last);
    if (hist->list->last)
	hist->list->last->next = item;
    else
	hist->list->first = item;
    hist->list->last = item;
    hist->list->nitem++;
    return item;
}

/* Don't mix pushHashHist() and pushHist()/unshiftHist(). */

HistItem *
pushHashHist(Hist *hist, const char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = getHashHist(hist, ptr);
    if (item) {
	if (item->next)
	    item->next->prev = item->prev;
	else			/* item == hist->list->last */
	    hist->list->last = item->prev;
	if (item->prev)
	    item->prev->next = item->next;
	else			/* item == hist->list->first */
	    hist->list->first = item->next;
	hist->list->nitem--;
    }
    item = pushHist(hist, ptr);
    putHash_sv(hist->hash, (char*)ptr, (void *)item);
    return item;
}

HistItem *
getHashHist(Hist *hist, const char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->hash == NULL) {
	hist->hash = newHash_sv(HIST_HASH_SIZE);
	for (item = hist->list->first; item; item = item->next)
	    putHash_sv(hist->hash, (char *)item->ptr, (void *)item);
    }
    return (HistItem *)getHash_sv(hist->hash, (char*)ptr, NULL);
}

char *
lastHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->list->last) {
	hist->current = hist->list->last;
	return (char *)hist->current->ptr;
    }
    return NULL;
}

char *
nextHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->current && hist->current->next) {
	hist->current = hist->current->next;
	return (char *)hist->current->ptr;
    }
    return NULL;
}

char *
prevHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->current && hist->current->prev) {
	hist->current = hist->current->prev;
	return (char *)hist->current->ptr;
    }
    return NULL;
}
