#include "propstring.h"
#include "w3m.h"

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
