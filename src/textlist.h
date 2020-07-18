/* $Id: textlist.h,v 1.6 2003/01/20 15:30:22 ukai Exp $ */
#ifndef TEXTLIST_H
#define TEXTLIST_H
#include "Str.h"

/* General doubly linked list */

typedef struct _listitem
{
    void *ptr;
    struct _listitem *next;
    struct _listitem *prev;
} ListItem;

typedef struct _generallist
{
    ListItem *first;
    ListItem *last;
    short nitem;
} GeneralList;

extern ListItem *newListItem(void *s, ListItem *n, ListItem *p);
extern GeneralList *newGeneralList(void);
extern void pushValue(GeneralList *tl, void *s);
extern void *popValue(GeneralList *tl);
extern void *rpopValue(GeneralList *tl);
extern void delValue(GeneralList *tl, ListItem *it);
extern GeneralList *appendGeneralList(GeneralList *, GeneralList *);

/* Text list */

typedef struct _textlistitem
{
    char *ptr;
    struct _textlistitem *next;
    struct _textlistitem *prev;
} TextListItem;

typedef struct _textlist
{
    TextListItem *first;
    TextListItem *last;
    short nitem;
} TextList;

TextList *newTextList();
void pushText(TextList *tl, char *s);
char *popText(TextList *tl);
char *rpopText(TextList *tl);
void delText(TextList *tl, char *i);
TextList *appendTextList(TextList *tl, TextList *tl2);

/* Line text list */

typedef struct _TextLine
{
    Str line;
    short pos;
} TextLine;

typedef struct _textlinelistitem
{
    TextLine *ptr;
    struct _textlinelistitem *next;
    struct _textlinelistitem *prev;
} TextLineListItem;

typedef struct _textlinelist
{
    TextLineListItem *first;
    TextLineListItem *last;
    short nitem;
} TextLineList;

extern TextLine *newTextLine(Str line, int pos);
extern void appendTextLine(TextLineList *tl, Str line, int pos);
TextLineList *newTextLineList();
void pushTextLine(TextLineList* tl, TextLine* lbuf);
TextLine *popTextLine( TextLineList* tl);
TextLine *rpopTextLine(TextLineList *tl);
TextLineList* appendTextLineList(TextLineList * tl, TextLineList* tl2);

#endif /* not TEXTLIST_H */
