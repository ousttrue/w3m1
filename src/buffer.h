#pragma once
#include "types.h"

void deleteImage(Buffer *buf);
void getAllImage(Buffer *buf);
void loadImage(Buffer *buf, int flag);
Anchor *registerForm(Buffer *buf, FormList *flist, struct parsed_tag *tag, int line, int pos);
MapArea *follow_map_menu(Buffer *buf, char *name, Anchor *a_img, int x,int y);
Buffer *nullBuffer(void);
void clearBuffer(Buffer *buf);
void discardBuffer(Buffer *buf);
Buffer *namedBuffer(Buffer *first, char *name);
Buffer *deleteBuffer(Buffer *first, Buffer *delbuf);
Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf);
Buffer *nthBuffer(Buffer *firstbuf, int n);
void gotoRealLine(Buffer *buf, int n);
void gotoLine(Buffer *buf, int n);
Buffer *selectBuffer(Buffer *firstbuf, Buffer *currentbuf,char *selectchar);
void reshapeBuffer(Buffer *buf);
void copyBuffer(Buffer *a, Buffer *b);
Buffer *prevBuffer(Buffer *first, Buffer *buf);
HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
void shiftAnchorPosition(AnchorList *a, HmarkerList *hl, int line,
                         int pos, int shift);
char *getAnchorText(Buffer *buf, AnchorList *al, Anchor *a);
