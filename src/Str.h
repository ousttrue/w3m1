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
    GCStr *Clone();
    void Clear();
    char *RequireSize(int size);
    void Grow();
    void CopyFrom(const char *src, int size);
    void CopyFrom(const char *src)
    {
        CopyFrom(src, strlen(src));
    }
    void CopyFrom(const GCStr *src)
    {
        CopyFrom(src->ptr, src->length);
    }
    void Push(const char *src, int size);
    void Push(const char *src)
    {
        Push(src, strlen(src));
    }
    void Push(const GCStr *src)
    {
        Push(src->ptr, src->length);
    }
    void Push(char y)
    {
        Push(&y, 1);
        // ((length + 1 >= area_size) ? Strgrow(x), 0 : 0, ptr[length++] = (y), ptr[length] = 0);
    }
    void Truncate(int pos);
    void Pop(int len);
    void Delete(int pos, int len);
    void StripLeft();
    void StripRight();
    void Strip()
    {
        StripLeft();
        StripRight();
    }

    GCStr *Substr(int begin, int len) const;
    void Insert(int pos, char c);
    void Insert(int pos, const char *src);
    void Insert(int pos, const GCStr *src)
    {
        Insert(pos, src->ptr);
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
void Strcat_m_charp(Str, ...);

void Strlower(Str);
void Strupper(Str);
Str Stralign_left(Str, int);
Str Stralign_right(Str, int);
Str Stralign_center(Str, int);

Str Sprintf(char *fmt, ...);

Str Strfgets(FILE *);
Str Strfgetall(FILE *);

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

inline int Strfputs(Str s, FILE *f)
{
    return fwrite((s)->ptr, 1, (s)->length, (f));
}
