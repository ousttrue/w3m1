#include <memory>
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

Hist *newHist();
Hist *copyHist(Hist *hist);
HistItem *unshiftHist(Hist *hist, char *ptr);
HistItem *pushHist(Hist *hist, const char *ptr);
HistItem *pushHashHist(Hist *hist, const char *ptr);
HistItem *getHashHist(Hist *hist, const char *ptr);
char *lastHist(Hist *hist);
char *nextHist(Hist *hist);
char *prevHist(Hist *hist);

std::shared_ptr<struct Buffer> historyBuffer(Hist *hist);
void loadHistory(Hist *hist);
void saveHistory(Hist *hist, size_t size);
void ldHist(void);
