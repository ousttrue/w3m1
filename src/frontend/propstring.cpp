#include "propstring.h"
#include "w3m.h"
#include "ctrlcode.h"
#include "indep.h"
#include "gc_helper.h"

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

static int parse_ansi_color(char **str, Lineprop *effect, Linecolor *color)
{
    char *p = *str, *q;
    Lineprop e = *effect;
    Linecolor c = *color;
    int i;

    if (*p != ESC_CODE || *(p + 1) != '[')
        return 0;
    p += 2;
    for (q = p; IS_DIGIT(*q) || *q == ';'; q++)
        ;
    if (*q != 'm')
        return 0;
    *str = q + 1;
    while (1)
    {
        if (*p == 'm')
        {
            e = PE_NORMAL;
            c = 0;
            break;
        }
        if (IS_DIGIT(*p))
        {
            q = p;
            for (p++; IS_DIGIT(*p); p++)
                ;
            i = atoi(allocStr(q, p - q));
            switch (i)
            {
            case 0:
                e = PE_NORMAL;
                c = 0;
                break;
            case 1:
            case 5:
                e = PE_BOLD;
                break;
            case 4:
                e = PE_UNDER;
                break;
            case 7:
                e = PE_STAND;
                break;
            case 100: /* for EWS4800 kterm */
                c = 0;
                break;
            case 39:
                c &= 0xf0;
                break;
            case 49:
                c &= 0x0f;
                break;
            default:
                if (i >= 30 && i <= 37)
                    c = (c & 0xf0) | (i - 30) | 0x08;
                else if (i >= 40 && i <= 47)
                    c = (c & 0x0f) | ((i - 40) << 4) | 0x80;
                break;
            }
            if (*p == 'm')
                break;
        }
        else
        {
            e = PE_NORMAL;
            c = 0;
            break;
        }
        p++; /* *p == ';' */
    }
    *effect = e;
    *color = c;
    return 1;
}

/*
 * Check character type
 * 
 * Create Lineprop* and Linecolor*
 * 
 * Lineprop: PC_WCHAR2, PE_BOLD, PE_UNDER
 */
PropertiedString PropertiedString::create(Str s, bool use_color)
{
    PropertiedString dst;

    // tmp buffer
    static int prop_size = 0;
    static Lineprop *prop_buffer = NULL;
    static int color_size = 0;
    static Linecolor *color_buffer = NULL;

    if (prop_size < s->Size())
    {
        prop_size = (s->Size() > LINELEN) ? s->Size() : LINELEN;
        prop_buffer = New_Reuse(Lineprop, prop_buffer, prop_size);
    }
    auto prop = prop_buffer;

    Linecolor *color = NULL;
    char *bs = NULL;
    char *es = NULL;
    char *str = s->ptr;
    char *endp = &s->ptr[s->Size()];
    int do_copy = false;
    if (w3mApp::Instance().ShowEffect)
    {
        bs = (char *)memchr(str, '\b', s->Size());

        if (use_color)
        {
            es = (char *)memchr(str, ESC_CODE, s->Size());
            if (es)
            {
                if (color_size < s->Size())
                {
                    color_size = (s->Size() > LINELEN) ? s->Size() : LINELEN;
                    color_buffer = New_Reuse(Linecolor, color_buffer,
                                             color_size);
                }
                color = color_buffer;
            }
        }

        if ((bs != NULL) || (es != NULL))
        {
            char *sp = str, *ep;
            s = Strnew_size(s->Size());
            do_copy = true;
            ep = bs ? (bs - 2) : endp;
            if (es && ep > es - 2)
                ep = es - 2;
            for (; str < ep && IS_ASCII(*str); str++)
            {
                // set property before escape
                *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
                if (color)
                    *(color++) = 0;
            }
            s->Push(sp, (int)(str - sp));
        }
    }
    if (!do_copy)
    {
        for (; str < endp && IS_ASCII(*str); str++)
            *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
    }

    Lineprop effect = PE_NORMAL;
    int plen = 0;
    int clen = 0;
    Lineprop ceffect = PE_NORMAL;
    Linecolor cmode = 0;
    int check_color = false;
    while (str < endp)
    {
        if (prop - prop_buffer >= prop_size)
            break;
        if (bs != NULL)
        {
            if (str == bs - 2 && !strncmp(str, "__\b\b", 4))
            {
                str += 4;
                effect = PE_UNDER;
                if (str < endp)
                    bs = (char *)memchr(str, '\b', endp - str);
                continue;
            }
            else if (str == bs - 1 && *str == '_')
            {
                str += 2;
                effect = PE_UNDER;
                if (str < endp)
                    bs = (char *)memchr(str, '\b', endp - str);
                continue;
            }
            else if (str == bs)
            {
                if (*(str + 1) == '_')
                {
                    if (s->Size())
                    {
                        str += 2;
                        for (int i = 1; i <= plen; i++)
                            *(prop - i) |= PE_UNDER;
                    }
                    else
                    {
                        str++;
                    }
                }
                else if (!strncmp(str + 1, "\b__", 3))
                {
                    if (s->Size())
                    {
                        str += (plen == 1) ? 3 : 4;
                        for (int i = 1; i <= plen; i++)
                            *(prop - i) |= PE_UNDER;
                    }
                    else
                    {
                        str += 2;
                    }
                }
                else if (*(str + 1) == '\b')
                {
                    if (s->Size())
                    {
                        clen = get_mclen(str + 2);
                        if (plen == clen &&
                            !strncmp(str - plen, str + 2, plen))
                        {
                            for (int i = 1; i <= plen; i++)
                                *(prop - i) |= PE_BOLD;
                            str += 2 + clen;
                        }
                        else
                        {
                            s->Pop(plen);
                            prop -= plen;
                            str += 2;
                        }
                    }
                    else
                    {
                        str += 2;
                    }
                }
                else
                {
                    if (s->Size())
                    {

                        clen = get_mclen(str + 1);
                        if (plen == clen &&
                            !strncmp(str - plen, str + 1, plen))
                        {
                            for (int i = 1; i <= plen; i++)
                                *(prop - i) |= PE_BOLD;
                            str += 1 + clen;
                        }
                        else
                        {
                            s->Pop(plen);
                            prop -= plen;
                            str++;
                        }
                    }
                    else
                    {
                        str++;
                    }
                }
                if (str < endp)
                    bs = (char *)memchr(str, '\b', endp - str);
                continue;
            }
            else if (str > bs)
                bs = (char *)memchr(str, '\b', endp - str);
        } // bs

        if (es != NULL)
        {
            if (str == es)
            {
                int ok = parse_ansi_color(&str, &ceffect, &cmode);
                if (str < endp)
                    es = (char *)memchr(str, ESC_CODE, endp - str);
                if (ok)
                {
                    if (cmode)
                        check_color = true;
                    continue;
                }
            }
            else if (str > es)
                es = (char *)memchr(str, ESC_CODE, endp - str);
        } // es

        //
        //
        //
        plen = get_mclen(str);
        auto mode = get_mctype(*str) | effect;
        if (color)
        {
            *(color++) = cmode;
            mode |= ceffect;
        }
        *(prop++) = mode;

        if (plen > 1)
        {
            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
            for (int i = 1; i < plen; i++)
            {
                *(prop++) = mode;
                if (color)
                    *(color++) = cmode;
            }
            if (do_copy)
                s->Push((char *)str, plen);
            str += plen;
        }
        else
        {
            if (do_copy)
                s->Push((char)*str);
            str++;
        }
        effect = PE_NORMAL;
    }

    // *oprop = prop_buffer;
    // if (ocolor)
    //     *ocolor = check_color ? color_buffer : NULL;

    return PropertiedString(s->ptr, prop_buffer, s->Size(), check_color ? color_buffer : NULL);
}

int PropertiedString::calcPosition(int pos, int bpos, CalcPositionMode mode) const
{
    static int *realColumn = nullptr;
    static int size = 0;
    static char *prevl = nullptr;
    int i, j;

    auto l = const_cast<char *>(lineBuf());
    auto pr = propBuf();
    auto len = this->len();

    if (l == nullptr || len == 0)
        return bpos;
    if (l == prevl && mode == CP_AUTO)
    {
        if (pos <= len)
            return realColumn[pos];
    }
    if (size < len + 1)
    {
        size = (len + 1 > LINELEN) ? (len + 1) : LINELEN;
        realColumn = New_N(int, size);
    }
    prevl = l;
    i = 0;
    j = bpos;
    if (pr[i] & PC_WCHAR2)
    {
        for (; i < len && pr[i] & PC_WCHAR2; i++)
            realColumn[i] = j;
        if (i > 0 && pr[i - 1] & PC_KANJI && WcOption.use_wide)
            j++;
    }
    while (1)
    {
        realColumn[i] = j;
        if (i == len)
            break;
        j += PropertiedCharacter{l + i, pr[i]}.ColumnLen();
        i++;
        for (; i < len && pr[i] & PC_WCHAR2; i++)
            realColumn[i] = realColumn[i - 1];
    }
    if (pos >= i)
        return j;
    return realColumn[pos];
}
