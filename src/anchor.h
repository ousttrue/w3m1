
#pragma once
#include "types.h"

Anchor *putAnchor(AnchorList &al, char *url, char *target,
                      char *referer,
                      char *title, unsigned char key, int line,
                      int pos);
Anchor *registerHref(BufferPtr buf, char *url, char *target,
                     char *referer, char *title, unsigned char key,
                     int line, int pos);
Anchor *registerName(BufferPtr buf, char *url, int line, int pos);
Anchor *registerImg(BufferPtr buf, char *url, char *title, int line,
                    int pos);

Anchor *retrieveCurrentAnchor(BufferPtr buf);
Anchor *retrieveCurrentImg(BufferPtr buf);
Anchor *retrieveCurrentForm(BufferPtr buf);
Anchor *searchAnchor(const AnchorList &al, char *str);
Anchor *searchURLLabel(BufferPtr buf, char *url);
void reAnchorWord(BufferPtr buf, Line *l, int spos, int epos);
char *reAnchor(BufferPtr buf, char *re);
char *reAnchorNews(BufferPtr buf, char *re);
char *reAnchorNewsheader(BufferPtr buf);
void addMultirowsForm(BufferPtr buf, AnchorList &al);
Anchor *closest_next_anchor(AnchorList &a, Anchor *an, int x, int y);
Anchor *closest_prev_anchor(AnchorList &a, Anchor *an, int x, int y);
void addMultirowsImg(BufferPtr buf, AnchorList &al);
void shiftAnchorPosition(AnchorList &al, HmarkerList *hl, const BufferPoint &bp, int shift);
char *getAnchorText(BufferPtr buf, AnchorList &al, Anchor *a);
