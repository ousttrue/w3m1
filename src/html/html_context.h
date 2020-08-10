#pragma once
#include <wc.h>

class HSequence
{
    CharacterEncodingScheme cur_document_charset;

    int cur_hseq = 1;

    int symbol_width = 0;
    int symbol_width0 = 0;

public:
    HSequence();
    ~HSequence();

    CharacterEncodingScheme CES() const { return cur_document_charset; }
    void SetCES(CharacterEncodingScheme ces) { cur_document_charset = ces; }

    int Increment()
    {
        return cur_hseq++;
    }
    int Get() const { return cur_hseq; }

    Str GetLinkNumberStr(int correction);
    int SymbolWidth() const { return symbol_width; }
    int SymbolWidth0() const { return symbol_width0; }
};
