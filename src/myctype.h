/* $Id: myctype.h,v 1.6 2003/09/22 21:02:20 ukai Exp $ */
#ifndef _MYCTYPE_H
#define _MYCTYPE_H

#define MYCTYPE_CNTRL 1
#define MYCTYPE_SPACE 2
#define MYCTYPE_ALPHA 4
#define MYCTYPE_DIGIT 8
#define MYCTYPE_PRINT 16
#define MYCTYPE_HEX   32
#define MYCTYPE_INTSPACE 64
#define MYCTYPE_ASCII (MYCTYPE_CNTRL|MYCTYPE_PRINT)
#define MYCTYPE_ALNUM (MYCTYPE_ALPHA|MYCTYPE_DIGIT)
#define MYCTYPE_XDIGIT (MYCTYPE_HEX|MYCTYPE_DIGIT)

#define GET_MYCTYPE(x) (MYCTYPE_MAP[(int)(unsigned char)(x)])
#define GET_MYCDIGIT(x) (MYCTYPE_DIGITMAP[(int)(unsigned char)(x)])

#define IS_CNTRL(x) (GET_MYCTYPE(x) & MYCTYPE_CNTRL)
#define IS_SPACE(x) (GET_MYCTYPE(x) & MYCTYPE_SPACE)
#define IS_ALPHA(x) (GET_MYCTYPE(x) & MYCTYPE_ALPHA)
#define IS_DIGIT(x) (GET_MYCTYPE(x) & MYCTYPE_DIGIT)
#define IS_PRINT(x) (GET_MYCTYPE(x) & MYCTYPE_PRINT)
#define IS_ASCII(x) (GET_MYCTYPE(x) & MYCTYPE_ASCII)
#define IS_ALNUM(x) (GET_MYCTYPE(x) & MYCTYPE_ALNUM)
#define IS_XDIGIT(x) (GET_MYCTYPE(x) & MYCTYPE_XDIGIT)
#define IS_INTSPACE(x) (MYCTYPE_MAP[(unsigned char)(x)] & MYCTYPE_INTSPACE)

extern unsigned char MYCTYPE_MAP[];
extern unsigned char MYCTYPE_DIGITMAP[];

#define	TOLOWER(x)	(IS_ALPHA(x) ? ((x)|0x20) : (x))
#define	TOUPPER(x)	(IS_ALPHA(x) ? ((x)&~0x20) : (x))

// #define SKIP_BLANKS(p)                 \
//     {                                  \
//         while (*(p) && IS_SPACE(*(p))) \
//             (p)++;                     \
//     }
// #define SKIP_NON_BLANKS(p)              \
//     {                                   \
//         while (*(p) && !IS_SPACE(*(p))) \
//             (p)++;                      \
//     }

#include <functional>
inline void SKIP(const char **pp, const std::function<int(char)> &pred)
{
    auto p = *pp;
    for (; *p; ++p)
    {
        if (!pred(*p))
        {
            *pp = p;
            break;
        }
    }
}

inline void SKIP_BLANKS(const char **pp)
{
    return SKIP(pp, [](auto c){ return IS_SPACE(c); });
}
template <typename T>
inline void SKIP_BLANKS(T **pp)
{
    static_assert(sizeof(T) == 1, "require char variant");
    return SKIP_BLANKS(const_cast<const char **>(reinterpret_cast<char **>(pp)));
}

inline void SKIP_NON_BLANKS(const char **pp)
{
    return SKIP(pp, [](auto c) { return !IS_SPACE(c); });
}
template <typename T>
inline void SKIP_NON_BLANKS(T **pp)
{
    static_assert(sizeof(T) == 1, "require char variant");
    return SKIP_NON_BLANKS(const_cast<const char **>(reinterpret_cast<char **>(pp)));
}

#define IS_ENDL(c) ((c) == '\0' || (c) == '\r' || (c) == '\n')
#define IS_ENDT(c) (IS_ENDL(c) || (c) == ';')

#define EOL(l) (&(l)->ptr[(l)->length])
#define IS_EOL(p, l) ((p) == &(l)->ptr[(l)->length])

#endif
