#pragma once
#include "types.h"

void deleteImage(BufferPtr buf);
void getAllImage(BufferPtr buf);
void loadImage(BufferPtr buf, int flag);

BufferPtr nullBuffer(void);
void clearBuffer(BufferPtr buf);
BufferPtr namedBuffer(BufferPtr first, char *name);
BufferPtr deleteBuffer(BufferPtr first, BufferPtr delbuf);
BufferPtr replaceBuffer(BufferPtr first, BufferPtr delbuf, BufferPtr newbuf);
BufferPtr nthBuffer(BufferPtr firstbuf, int n);
void gotoRealLine(BufferPtr buf, int n);
void gotoLine(BufferPtr buf, int n);
BufferPtr selectBuffer(BufferPtr firstbuf, BufferPtr currentbuf, char *selectchar);
void reshapeBuffer(BufferPtr buf);

BufferPtr prevBuffer(BufferPtr first, BufferPtr buf);
HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
void set_buffer_environ(BufferPtr buf);
void cmd_loadBuffer(BufferPtr buf, int prop, LinkBufferTypes linkid);

// anchor
const Anchor *retrieveCurrentAnchor(BufferPtr buf);
const Anchor *retrieveCurrentImg(BufferPtr buf);
const Anchor *retrieveCurrentForm(BufferPtr buf);
const Anchor *searchURLLabel(BufferPtr buf, char *url);
void reAnchorWord(BufferPtr buf, Line *l, int spos, int epos);
char *reAnchor(BufferPtr buf, char *re);
char *reAnchorNews(BufferPtr buf, char *re);
char *reAnchorNewsheader(BufferPtr buf);
void addMultirowsForm(BufferPtr buf, AnchorList &al);
void addMultirowsImg(BufferPtr buf, AnchorList &al);
void shiftAnchorPosition(AnchorList &al, HmarkerList *hl, const BufferPoint &bp, int shift);
char *getAnchorText(BufferPtr buf, AnchorList &al, Anchor *a);
