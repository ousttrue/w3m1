#pragma once
#include <wc.h>

class HSequence
{
    int cur_hseq = 1;

    int symbol_width = 0;
    int symbol_width0 = 0;

public:
    HSequence();
    ~HSequence();
    int Increment()
    {
        return cur_hseq++;
    }
    int Get() const { return cur_hseq; }

    Str GetLinkNumberStr(int correction);
    int SymbolWidth() const { return symbol_width; }
    int SymbolWidth0() const { return symbol_width0; }
};
