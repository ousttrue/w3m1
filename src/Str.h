/* $Id: Str.h,v 1.6 2006/04/07 13:35:35 inu Exp $ */
/* 
 * String manipulation library for Boehm GC
 *
 * (C) Copyright 1998-1999 by Akinori Ito
 *
 * This software may be redistributed freely for this purpose, in full 
 * or in part, provided that this entire copyright notice is included 
 * on any copies of this software and applications and derivations thereof.
 *
 * This software is provided on an "as is" basis, without warranty of any
 * kind, either expressed or implied, as to any matter including, but not
 * limited to warranty of fitness of purpose, or merchantability, or
 * results obtained from use of this software.
 */
#pragma once
#include <stdio.h>
#include <string.h>
#include <gc_cpp.h>

//
// http://aitoweb.world.coocan.jp/gc/gc.html
//

struct GCStr : public gc_cleanup
{
    char *ptr;
    int length;
    int area_size;

    GCStr(int size = 32);
    GCStr(const char *src, int size);
    GCStr(const char *src) : GCStr(src, strlen(src))
    {
    }
    ~GCStr();

    void Clear();
    char *RequireSize(int size);
    void CopyFrom(const char *src, int size);
    void CopyFrom(const char *src)
    {
        CopyFrom(src, strlen(src));
    }
    void CopyFrom(const GCStr *src)
    {
        CopyFrom(src->ptr, src->length);
    }
    void Concat(const char *src, int size);
    void Concat(const char *src)
    {
        Concat(src, strlen(src));
    }
    void Concat(const GCStr *src)
    {
        Concat(src->ptr, src->length);
    }
    void Concat(char y)
    {
        Concat(&y, 1);
        // ((length + 1 >= area_size) ? Strgrow(x), 0 : 0, ptr[length++] = (y), ptr[length] = 0);
    }
};
using Str = GCStr *;

inline Str Strnew()
{
    return new GCStr();
}

inline Str Strnew_size(int size)
{
    return new GCStr(size);
}

inline Str Strnew_charp(const char *src)
{
    return new GCStr(src);
}

inline Str Strnew_charp_n(const char *src, int size)
{
    return new GCStr(src, size);
}

Str Strnew_m_charp(char *, ...);
Str Strdup(Str);

// void Str->Concat( Str);
// void Str->Concat( char *);

void Strcat_m_charp(Str, ...);
Str Strsubstr(Str, int, int);
void Strinsert_char(Str, int, char);
void Strinsert_charp(Str, int, char *);
void Strdelete(Str, int, int);
void Strtruncate(Str, int);
void Strlower(Str);
void Strupper(Str);
void Strchop(Str);
void Strshrink(Str, int);
void Strshrinkfirst(Str, int);
void Strremovefirstspaces(Str);
void Strremovetrailingspaces(Str);
Str Stralign_left(Str, int);
Str Stralign_right(Str, int);
Str Stralign_center(Str, int);

Str Sprintf(char *fmt, ...);

Str Strfgets(FILE *);
Str Strfgetall(FILE *);

void Strgrow(Str s);

inline int Strcmp(Str x, Str y)
{
    return strcmp((x)->ptr, (y)->ptr);
}

inline int Strcmp_charp(Str x, const char *y)
{
    return strcmp((x)->ptr, (y));
}

inline int Strncmp(Str x, Str y, int n)
{
    return strncmp((x)->ptr, (y)->ptr, (n));
}

inline int Strncmp_charp(Str x, char *y, int n)
{
    return strncmp((x)->ptr, (y), (n));
}

inline int Strcasecmp(Str x, Str y)
{
    return strcasecmp((x)->ptr, (y)->ptr);
}

inline int Strcasecmp_charp(Str x, char *y)
{
    return strcasecmp((x)->ptr, (y));
}

inline int Strncasecmp(Str x, Str y, int n)
{
    return strncasecmp((x)->ptr, (y)->ptr, (n));
}

inline int Strncasecmp_charp(Str x, char *y, int n)
{
    return strncasecmp((x)->ptr, (y), (n));
}

inline char Strlastchar(Str s)
{
    return ((s)->length > 0 ? (s)->ptr[(s)->length - 1] : '\0');
}

inline void Strinsert(Str s, int n, Str p)
{
    Strinsert_charp((s), (n), (p)->ptr);
}

inline void Strshrinkfirst(Str s, int n)
{
    Strdelete((s), 0, (n));
}

inline int Strfputs(Str s, FILE *f)
{
    return fwrite((s)->ptr, 1, (s)->length, (f));
}
