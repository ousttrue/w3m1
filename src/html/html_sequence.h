#pragma once
#include <wc.h>

class HSequence
{
    int cur_hseq = 1;

public:
    int Increment()
    {
        return cur_hseq++;
    }
    int Get() const { return cur_hseq; }
    // void SetCurHSeq(int seq) { cur_hseq = seq; }
    Str GetLinkNumberStr(int correction);
};
