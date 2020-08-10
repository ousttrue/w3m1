#pragma once
#include <wc.h>

class HtmlContext
{
    CharacterEncodingScheme cur_document_charset;

    int cur_hseq = 1;

    int symbol_width = 0;
    int symbol_width0 = 0;

    Str cur_title = nullptr;

public:
    HtmlContext();
    ~HtmlContext();

    CharacterEncodingScheme CES() const { return cur_document_charset; }
    void SetCES(CharacterEncodingScheme ces) { cur_document_charset = ces; }

    void SetCurTitle(Str title)
    {
        cur_title = title;
    }

    int Increment()
    {
        return cur_hseq++;
    }
    int Get() const { return cur_hseq; }

    Str GetLinkNumberStr(int correction);
    int SymbolWidth() const { return symbol_width; }
    int SymbolWidth0() const { return symbol_width0; }

    // process <title>{content}</title> tag
    Str TitleOpen(struct parsed_tag *tag);
    void TitleContent(const char *str);
    Str TitleClose(struct parsed_tag *tag);
};
