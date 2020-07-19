#pragma once
#include "bufferpoint.h"

typedef struct
{
    BufferPoint *marks;
    int nmark;
    int markmax;
    int prevhseq;
} HmarkerList;
