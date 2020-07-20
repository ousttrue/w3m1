/* $Id: frame.h,v 1.6 2003/01/25 17:42:17 ukai Exp $ */
/*
 * frame support
 */
#pragma once
#include "html.h"
#include "form.h"
#include "buffer.h"

struct frame_element
{
    char attr;
#define F_UNLOADED 0x00
#define F_BODY 0x01
#define F_FRAMESET 0x02
    char dummy;
    char *name;
};

struct frame_body
{
    char attr;
    char flags;
#define FB_NO_BUFFER 0x01
    char *name;
    char *url;
    ParsedURL *baseURL;
    char *source;
    char *type;
    char *referer;
    AnchorList *nameList;
    FormList *request;
#ifdef USE_SSL
    char *ssl_certificate;
#endif
};

union frameset_element {
    struct frame_element *element;
    struct frame_body *body;
    struct frameset *set;
};

struct frameset
{
    char attr;
    char dummy;
    char *name;
    ParsedURL *currentURL;
    char **width;
    char **height;
    int col;
    int row;
    int i;
    union frameset_element *frame;
};

struct frameset_queue
{
    struct frameset_queue *next;
    struct frameset_queue *back;
    struct frameset *frameset;
    long linenumber;
    long top_linenumber;
    int pos;
    int currentColumn;
    AnchorList *formitem;
};

extern struct frameset *renderFrameSet;

void addFrameSetElement(struct frameset *f,
                        union frameset_element element);
void deleteFrameSetElement(union frameset_element e);
void resetFrameElement(union frameset_element *f_element, Buffer *buf,
                       char *referer, FormList *request);
