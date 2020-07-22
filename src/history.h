/* $Id: history.h,v 1.5 2002/01/26 17:24:01 ukai Exp $ */
#ifndef HISTORY_H
#define HISTORY_H

#include "textlist.h"
#include "hash.h"

#define HIST_HASH_SIZE 127

typedef ListItem HistItem;

typedef GeneralList HistList;

struct Hist
{
    HistList *list;
    HistItem *current;
    Hash_sv *hash;
};

extern Hist *newHist();
extern Hist *copyHist(Hist *hist);
extern HistItem *unshiftHist(Hist *hist, char *ptr);
extern HistItem *pushHist(Hist *hist, const char *ptr);
extern HistItem *pushHashHist(Hist *hist, const char *ptr);
extern HistItem *getHashHist(Hist *hist, const char *ptr);
extern char *lastHist(Hist *hist);
extern char *nextHist(Hist *hist);
extern char *prevHist(Hist *hist);

#endif /* HISTORY_H */
