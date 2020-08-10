#include "propstring.h"
#include "w3m.h"

///
/// PropertiedCharacter
///
int PropertiedCharacter::ColumnLen() const
{
    if (this->prop & PC_CTRL)
    {
        if (*this->head == '\t')
            return (w3mApp::Instance().Tabstop) / w3mApp::Instance().Tabstop * w3mApp::Instance().Tabstop;
        else if (*this->head == '\n')
            return 1;
        else if (*this->head != '\r')
            return 2;
        return 0;
    }

    if (this->prop & PC_UNKNOWN)
    {
        return 4;
    }

    return wtf_width((uint8_t *)this->head);
}

///
/// PropertiedString
///
PropertiedString::PropertiedString(const char *str, int len)
{
    // Lineprop *propBuffer = NULL;
    // Linecolor *colorBuffer = NULL;
    // auto lineBuf2 = checkType(line, &propBuffer, &colorBuffer);
    assert(false);
}
// void Buffer::addnewline(Str line, int nlines)
// {
//     Lineprop *propBuffer = NULL;
//     Linecolor *colorBuffer = NULL;
//     auto lineBuf2 = checkType(line, &propBuffer, &colorBuffer);
//     addnewline(lineBuf2->ptr, propBuffer, colorBuffer, pos, FOLD_BUFFER_WIDTH(), nlines);
// }

Str PropertiedString::conv_symbol() const
{
    const char **symbol = NULL;
    auto tmp = Strnew();
    auto pr = this->propBuf();
    for (auto p = this->lineBuf(), ep = p + this->len(); p < ep; p++, pr++)
    {
        if (*pr & PC_SYMBOL)
        {

            char c = ((char)wtf_get_code((uint8_t *)p) & 0x7f) - SYMBOL_BASE;
            int len = get_mclen(p);
            tmp->Push(symbol[(int)c]);

            p += len - 1;
            pr += len - 1;
        }
        else
        {
            tmp->Push(*p);
        }
    }
    return tmp;
}
