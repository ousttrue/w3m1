/* $Id: textlist.h,v 1.6 2003/01/20 15:30:22 ukai Exp $ */
#ifndef TEXTLIST_H
#define TEXTLIST_H
#include <wc.h>

/* General doubly linked list */

struct ListItem
{
    void *ptr;
    ListItem *next;
    ListItem *prev;
};

struct GeneralList
{
    ListItem *first;
    ListItem *last;
    short nitem;
};

extern ListItem *newListItem(void *s, ListItem *n, ListItem *p);
extern GeneralList *newGeneralList(void);
extern void pushValue(GeneralList *tl, void *s);
extern void *popValue(GeneralList *tl);
extern void *rpopValue(GeneralList *tl);
extern void delValue(GeneralList *tl, ListItem *it);
extern GeneralList *appendGeneralList(GeneralList *, GeneralList *);

/* Text list */

struct TextListItem
{
    char *ptr;
    TextListItem *next;
    TextListItem *prev;
};

struct TextList
{
    TextListItem *first;
    TextListItem *last;
    short nitem;
};

TextList *newTextList();
void pushText(TextList *tl, const char *s);
char *popText(TextList *tl);
char *rpopText(TextList *tl);
void delText(TextList *tl, char *i);
TextList *appendTextList(TextList *tl, TextList *tl2);

/* Line text list */

struct TextLine
{
    Str line;
    short pos;
};

struct TextLineListItem
{
    TextLine *ptr;
    TextLineListItem *next;
    TextLineListItem *prev;
};

struct TextLineList
{
    TextLineListItem *first;
    TextLineListItem *last;
    short nitem;
};

extern TextLine *newTextLine(Str line, int pos);
extern void appendTextLine(TextLineList *tl, Str line, int pos);
TextLineList *newTextLineList();
void pushTextLine(TextLineList *tl, TextLine *lbuf);
TextLine *popTextLine(TextLineList *tl);
TextLine *rpopTextLine(TextLineList *tl);
TextLineList *appendTextLineList(TextLineList *tl, TextLineList *tl2);

#endif /* not TEXTLIST_H */
