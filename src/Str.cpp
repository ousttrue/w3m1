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

#include "Str.h"
#include <assert.h>

void GCStr::Allocate(int size)
{
    size = std::max(32, size);
    ptr = (char *)GC_MALLOC_ATOMIC(size);
    ptr[0] = '\0';
    m_capacity = size;
}

void GCStr::Assign(const char *src, int size)
{
    assert(src);
    Allocate(size+1);
    m_size = size;
    bcopy((void *)src, (void *)ptr, size);
    ptr[size] = '\0';
}

GCStr::GCStr(int size)
{
    Allocate(size);
}

GCStr::GCStr(const char *src, int size)
{
    Assign(src, size);
}

GCStr::GCStr(const char *src)
{
    if (src)
    {
        Assign(src, strlen(src));
    }
    else
    {
        Allocate(0);
    }
}

GCStr::~GCStr()
{
    // GC_FREE(ptr);
    auto a = 0;
}

int GCStr::Cmp(const GCStr *y) const
{
    return strcmp(ptr, y->ptr);
}
int GCStr::Cmp(const char *y) const
{
    return strcmp(ptr, y);
}
int GCStr::Cmp(const GCStr *y, int n) const
{
    return strncmp(ptr, y->ptr, n);
}
int GCStr::Cmp(const char *y, int n) const
{
    return strncmp(ptr, y, n);
}
// ignore case
int GCStr::ICaseCmp(const GCStr *y) const
{
    return strcasecmp(ptr, y->ptr);
}
int GCStr::ICaseCmp(const GCStr *y, int n) const
{
    return strncasecmp(ptr, y->ptr, n);
}
int GCStr::ICaseCmp(const char *y) const
{
    return strcasecmp(ptr, y);
}
int GCStr::ICaseCmp(const char *y, int n) const
{
    return strncasecmp(ptr, y, n);
}

char GCStr::Back() const
{
    return (m_size > 0 ? ptr[m_size - 1] : '\0');
}

GCStr *GCStr::Clone() const
{
    return new GCStr(ptr, m_size);
}

void GCStr::Clear()
{
    m_size = 0;
    ptr[0] = '\0';
}

char *GCStr::RequireSize(int size)
{
    if (m_capacity >= size)
    {
        return nullptr;
    }
    auto old = ptr;
    size = std::max(m_capacity * 2, size);
    ptr = (char *)GC_MALLOC_ATOMIC(size);
    m_capacity = size;
    return old;
}

void GCStr::Grow()
{
    char *old = ptr;
    int newlen = m_size * 6 / 5;
    if (newlen == m_size)
        newlen += 2;
    RequireSize(newlen);
    bcopy((void *)old, (void *)ptr, m_size);
}

void GCStr::CopyFrom(const char *y, int n)
{
    if (y == NULL)
    {
        m_size = 0;
        return;
    }
    RequireSize(n + 1);
    bcopy((void *)y, (void *)ptr, n);
    ptr[n] = '\0';
    m_size = n;
}

void GCStr::Push(const char *y, int n)
{
    if (y == NULL)
    {
        return;
    }

    auto old = RequireSize(m_size + n + 1);
    if (old)
    {
        bcopy((void *)old, (void *)ptr, m_size);
    }
    bcopy((void *)y, (void *)&ptr[m_size], n);
    m_size += n;
    ptr[m_size] = '\0';
}

void GCStr::Truncate(int pos)
{
    ptr[pos] = '\0';
    m_size = pos;
}

void GCStr::Pop(int len)
{
    if (len >= m_size)
    {
        m_size = 0;
        ptr[0] = '\0';
    }
    else
    {
        m_size -= len;
        ptr[m_size] = '\0';
    }
}

void GCStr::Delete(int pos, int len)
{
    if (m_size <= pos + len)
    {
        ptr[pos] = '\0';
        m_size = pos;
        return;
    }

    int i = pos;
    for (; i < m_size - len; i++)
    {
        ptr[i] = ptr[i + len];
    }
    ptr[i] = '\0';
    m_size = i;
}

GCStr *GCStr::Substr(int begin, int len) const
{
    if (begin >= m_size)
    {
        // return empty
        return new GCStr();
    }
    return new GCStr(ptr + begin, std::min(m_size - begin, len));
}

void GCStr::Insert(int pos, char c)
{
    if (pos < 0 || pos > m_size)
    {
        return;
    }
    if (m_size + 2 > m_capacity)
    {
        Grow();
    }
    for (int i = m_size; i > pos; i--)
    {
        ptr[i] = ptr[i - 1];
    }
    ptr[++m_size] = '\0';
    ptr[pos] = c;
}

void GCStr::Insert(int pos, const char *p, int size)
{
    // while (*p)
    //     Insert(pos++, *(p++));
    for(int i=0; i<size; ++i)
    {
        Insert(pos++, *(p++));
    }
}

GCStr *GCStr::AlignLeft(int width) const
{
    if (m_size >= width)
    {
        return Clone();
    }

    auto n = new GCStr(width);
    n->CopyFrom(this);
    for (int i = m_size; i < width; i++)
    {
        n->Push(' ');
    }
    return n;
}

GCStr *GCStr::AlignRight(int width) const
{
    if (m_size >= width)
    {
        return Clone();
    }

    auto n = new GCStr(width);
    for (int i = m_size; i < width; i++)
    {
        n->Push(' ');
    }
    n->Push(this);
    return n;
}

GCStr *GCStr::AlignCenter(int width) const
{
    if (m_size >= width)
    {
        return Clone();
    }

    auto n = new GCStr(width);
    auto w = (width - m_size) / 2;
    for (int i = 0; i < w; i++)
        n->Push(' ');
    n->Push(this);
    for (int i = w + m_size; i < width; i++)
        n->Push(' ');
    return n;
}

int GCStr::Puts(FILE *f) const
{
    return fwrite(ptr, 1, m_size, f);
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
