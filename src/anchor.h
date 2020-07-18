#pragma once
#include "bufferpoint.h"
#include "image.h"
#include "parsetagx.h"

typedef struct _anchor {
    char *url;
    char *target;
    char *referer;
    char *title;
    unsigned char accesskey;
    BufferPoint start;
    BufferPoint end;
    int hseq;
    char slave;
    short y;
    short rows;
    Image *image;
} Anchor;

typedef struct _anchorList {
    Anchor *anchors;
    int nanchor;
    int anchormax;
    int acache;
} AnchorList;
