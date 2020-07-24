#pragma once
#include "types.h"

void deleteImage(BufferPtr buf);
void getAllImage(BufferPtr buf);
void loadImage(BufferPtr buf, int flag);
Anchor *registerForm(BufferPtr buf, FormList *flist, struct parsed_tag *tag, int line, int pos);
MapArea *follow_map_menu(BufferPtr buf, char *name, Anchor *a_img, int x, int y);
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
Anchor *registerHref(BufferPtr buf, char *url, char *target,
                     char *referer, char *title, unsigned char key,
                     int line, int pos);
Anchor *registerName(BufferPtr buf, char *url, int line, int pos);
Anchor *registerImg(BufferPtr buf, char *url, char *title, int line,
                    int pos);

Anchor *retrieveCurrentAnchor(BufferPtr buf);
Anchor *retrieveCurrentImg(BufferPtr buf);
Anchor *retrieveCurrentForm(BufferPtr buf);
Anchor *searchURLLabel(BufferPtr buf, char *url);
void reAnchorWord(BufferPtr buf, Line *l, int spos, int epos);
char *reAnchor(BufferPtr buf, char *re);
char *reAnchorNews(BufferPtr buf, char *re);
char *reAnchorNewsheader(BufferPtr buf);
void addMultirowsForm(BufferPtr buf, AnchorList &al);
void addMultirowsImg(BufferPtr buf, AnchorList &al);
void shiftAnchorPosition(AnchorList &al, HmarkerList *hl, const BufferPoint &bp, int shift);
char *getAnchorText(BufferPtr buf, AnchorList &al, Anchor *a);
