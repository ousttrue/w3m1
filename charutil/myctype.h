///
/// ASCII utility
///
#pragma once
#include <assert.h>
#include <functional>

enum MyCTypeFlags
{
    MYCTYPE_CNTRL = 1,
    MYCTYPE_SPACE = 2,
    MYCTYPE_ALPHA = 4,
    MYCTYPE_DIGIT = 8,
    MYCTYPE_PRINT = 16,
    MYCTYPE_HEX = 32,
    MYCTYPE_INTSPACE = 64,
    MYCTYPE_ASCII = (MYCTYPE_CNTRL | MYCTYPE_PRINT),
    MYCTYPE_ALNUM = (MYCTYPE_ALPHA | MYCTYPE_DIGIT),
    MYCTYPE_XDIGIT = (MYCTYPE_HEX | MYCTYPE_DIGIT),
};

extern unsigned char MYCTYPE_MAP[];

inline auto GET_MYCTYPE(int x)
{
    return MYCTYPE_MAP[(int)(unsigned char)x];
}
inline bool IS_CNTRL(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_CNTRL);
}
inline bool IS_SPACE(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_SPACE);
}
inline bool IS_ALPHA(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_ALPHA);
}
inline bool IS_DIGIT(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_DIGIT);
}
inline bool IS_PRINT(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_PRINT);
}
inline bool IS_ASCII(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_ASCII);
}
inline bool IS_ALNUM(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_ALNUM);
}
inline bool IS_XDIGIT(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_XDIGIT);
}
inline bool IS_INTSPACE(int x)
{
    return (GET_MYCTYPE(x) & MYCTYPE_INTSPACE);
}

// ascii(0 ~ F) to digit
extern unsigned char MYCTYPE_DIGITMAP[];
inline unsigned char GET_MYCDIGIT(int x)
{
    auto value = MYCTYPE_DIGITMAP[(int)(unsigned char)x];
    assert(value != 255);
    return value;
}

inline int TOLOWER(int x)
{
    return (IS_ALPHA(x) ? ((x) | 0x20) : (x));
}
inline int TOUPPER(int x)
{
    return (IS_ALPHA(x) ? ((x) & ~0x20) : (x));
}

inline void SKIP(const char **pp, const std::function<bool(char)> &pred)
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
    return SKIP(pp, IS_SPACE);
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

inline bool IS_ENDL(char c)
{
    return ((c) == '\0' || (c) == '\r' || (c) == '\n');
}

// cookie etc...
inline bool IS_ENDT(char c)
{
    return (IS_ENDL(c) || (c) == ';');
}

// #define EOL(l) (&(l)->ptr[(l)->length])
// #define IS_EOL(p, l) ((p) == &(l)->ptr[(l)->length])

/* is this '<' really means the beginning of a tag? */
inline bool REALLY_THE_BEGINNING_OF_A_TAG(const char *p)
{
    return (IS_ALPHA(p[1]) || p[1] == '/' || p[1] == '!' || p[1] == '?' || p[1] == '\0' || p[1] == '_');
}
