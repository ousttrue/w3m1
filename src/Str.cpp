/* $Id: Str.c,v 1.8 2002/12/24 17:20:46 ukai Exp $ */
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

#include <stdio.h>
#include <stdlib.h>
#include <gc.h>
#include <stdarg.h>
#include <string.h>
#include "Str.h"
#include "myctype.h"
#include <algorithm>

Str Strnew_m_charp(char *p, ...)
{
    va_list ap;
    va_start(ap, p);

    Str r = Strnew();
    while (p != NULL)
    {
        r->Push(p);
        p = va_arg(ap, char *);
    }
    return r;
}

GCStr::GCStr(int size)
{
    // Str x = (Str)GC_MALLOC(sizeof(GCStr));
    ptr = (char *)GC_MALLOC_ATOMIC(size);
    ptr[0] = '\0';
    area_size = size;
    length = 0;
}

GCStr::GCStr(const char *src, int size)
{
    ptr = (char *)GC_MALLOC_ATOMIC(size + 1);
    area_size = size + 1;
    length = size;
    bcopy((void *)src, (void *)ptr, size);
    ptr[size] = '\0';
}

GCStr::~GCStr()
{
    // GC_FREE(ptr);
    auto a = 0;
}

GCStr *GCStr::Clone()
{
    return new GCStr(ptr, length);
}

void GCStr::Clear()
{
    length = 0;
    ptr[0] = '\0';
}

char *GCStr::RequireSize(int size)
{
    if (area_size >= size)
    {
        return nullptr;
    }
    auto old = ptr;
    ptr = (char *)GC_MALLOC_ATOMIC(size);
    area_size = size;
    return old;
}

void GCStr::Grow()
{
    char *old = ptr;
    int newlen = length * 6 / 5;
    if (newlen == length)
        newlen += 2;
    RequireSize(newlen);
    bcopy((void *)old, (void *)ptr, length);
}

void GCStr::CopyFrom(const char *y, int n)
{
    if (y == NULL)
    {
        length = 0;
        return;
    }
    RequireSize(n + 1);
    bcopy((void *)y, (void *)ptr, n);
    ptr[n] = '\0';
    length = n;
}

void GCStr::Push(const char *y, int n)
{
    if (y == NULL)
    {
        return;
    }

    auto old = RequireSize(length + n + 1);
    if (old)
    {
        bcopy((void *)old, (void *)ptr, length);
    }
    bcopy((void *)y, (void *)&ptr[length], n);
    length += n;
    ptr[length] = '\0';
}

void GCStr::Delete(int pos, int len)
{
    if (length <= pos + len)
    {
        ptr[pos] = '\0';
        length = pos;
        return;
    }

    int i = pos;
    for (; i < length - len; i++)
    {
        ptr[i] = ptr[i + len];
    }
    ptr[i] = '\0';
    length = i;
}

void GCStr::StripLeft()
{
    int i = 0;
    for (; i < length; i++)
    {
        if (!IS_SPACE(ptr[i]))
        {
            break;
        }
    }
    Delete(0, i);
}

void GCStr::StripRight()
{
    int i = length - 1;
    for (; i >= 0 ; i--){
        if(!IS_SPACE(ptr[i])){
            break;
        }
    }
    ptr[i + 1] = '\0';
    length = i + 1;
}

// void GCStr::Chop(Str s)
// {

//     while ((s->ptr[s->length - 1] == '\n' || s->ptr[s->length - 1] == '\r') &&
//            s->length > 0)
//     {
//         s->length--;
//     }
//     s->ptr[s->length] = '\0';
// }

GCStr *GCStr::Substr(int begin, int len) const
{
    if (begin >= length)
    {
        // return empty
        return new GCStr();
    }
    return new GCStr(ptr + begin, std::min(length - begin, len));
}

void GCStr::Insert(int pos, char c)
{
    if (pos < 0 || pos > length)
    {
        return;
    }
    if (length + 2 > area_size)
    {
        Grow();
    }
    for (int i = length; i > pos; i--)
    {
        ptr[i] = ptr[i - 1];
    }
    ptr[++length] = '\0';
    ptr[pos] = c;
}

void GCStr::Insert(int pos, const char *p)
{
    while (*p)
        Insert(pos++, *(p++));
}

void Strcat_m_charp(Str x, ...)
{
    va_list ap;
    char *p;

    va_start(ap, x);
    while ((p = va_arg(ap, char *)) != NULL)
        x->Push(p, strlen(p));
}

void Strlower(Str s)
{
    int i;

    for (i = 0; i < s->length; i++)
        s->ptr[i] = TOLOWER(s->ptr[i]);
}

void Strupper(Str s)
{
    int i;

    for (i = 0; i < s->length; i++)
        s->ptr[i] = TOUPPER(s->ptr[i]);
}

void Strtruncate(Str s, int pos)
{

    s->ptr[pos] = '\0';
    s->length = pos;
}

void Strshrink(Str s, int n)
{

    if (n >= s->length)
    {
        s->length = 0;
        s->ptr[0] = '\0';
    }
    else
    {
        s->length -= n;
        s->ptr[s->length] = '\0';
    }
}

Str Stralign_left(Str s, int width)
{
    if (s->length >= width)
        return s->Clone();
    auto n = Strnew_size(width);
    n->CopyFrom(s);
    for (int i = s->length; i < width; i++)
        n->Push(' ');
    return n;
}

Str Stralign_right(Str s, int width)
{
    if (s->length >= width)
        return s->Clone();
    auto n = Strnew_size(width);
    for (int i = s->length; i < width; i++)
        n->Push(' ');
    n->Push(s);
    return n;
}

Str Stralign_center(Str s, int width)
{
    if (s->length >= width)
        return s->Clone();
    auto n = Strnew_size(width);
    auto w = (width - s->length) / 2;
    for (int i = 0; i < w; i++)
        n->Push(' ');
    n->Push(s);
    for (int i = w + s->length; i < width; i++)
        n->Push(' ');
    return n;
}

#define SP_NORMAL 0
#define SP_PREC 1
#define SP_PREC2 2

Str Sprintf(char *fmt, ...)
{
    int len = 0;
    int status = SP_NORMAL;
    int p = 0;
    char *f;
    Str s;
    va_list ap;

    va_start(ap, fmt);
    for (f = fmt; *f; f++)
    {
    redo:
        switch (status)
        {
        case SP_NORMAL:
            if (*f == '%')
            {
                status = SP_PREC;
                p = 0;
            }
            else
                len++;
            break;
        case SP_PREC:
            if (IS_ALPHA(*f))
            {
                /* conversion char. */
                double vd;
                int vi;
                char *vs;
                void *vp;

                switch (*f)
                {
                case 'l':
                case 'h':
                case 'L':
                case 'w':
                    continue;
                case 'd':
                case 'i':
                case 'o':
                case 'x':
                case 'X':
                case 'u':
                    vi = va_arg(ap, int);
                    len += (p > 0) ? p : 10;
                    break;
                case 'f':
                case 'g':
                case 'e':
                case 'G':
                case 'E':
                    vd = va_arg(ap, double);
                    len += (p > 0) ? p : 15;
                    break;
                case 'c':
                    len += 1;
                    vi = va_arg(ap, int);
                    break;
                case 's':
                    vs = va_arg(ap, char *);
                    vi = strlen(vs);
                    len += (p > vi) ? p : vi;
                    break;
                case 'p':
                    vp = va_arg(ap, void *);
                    len += 10;
                    break;
                case 'n':
                    vp = va_arg(ap, void *);
                    break;
                }
                status = SP_NORMAL;
            }
            else if (IS_DIGIT(*f))
                p = p * 10 + *f - '0';
            else if (*f == '.')
                status = SP_PREC2;
            else if (*f == '%')
            {
                status = SP_NORMAL;
                len++;
            }
            break;
        case SP_PREC2:
            if (IS_ALPHA(*f))
            {
                status = SP_PREC;
                goto redo;
            }
            break;
        }
    }
    va_end(ap);
    s = Strnew_size(len * 2);
    va_start(ap, fmt);
    vsprintf(s->ptr, fmt, ap);
    va_end(ap);
    s->length = strlen(s->ptr);
    if (s->length > len * 2)
    {
        fprintf(stderr, "Sprintf: string too long\n");
        exit(1);
    }
    return s;
}

Str Strfgets(FILE *f)
{
    Str s = Strnew();
    char c;
    while (1)
    {
        c = fgetc(f);
        if (feof(f) || ferror(f))
            break;
        s->Push(c);
        if (c == '\n')
            break;
    }
    return s;
}

Str Strfgetall(FILE *f)
{
    Str s = Strnew();
    char c;
    while (1)
    {
        c = fgetc(f);
        if (feof(f) || ferror(f))
            break;
        s->Push(c);
    }
    return s;
}
