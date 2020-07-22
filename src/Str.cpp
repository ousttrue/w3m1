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
#include "myctype.h"
#include "indep.h"
#include <stdio.h>
#include <stdlib.h>
// #include <gc.h>
#include <stdarg.h>
#include <string.h>
#include <algorithm>

GCStr::GCStr(int size)
{
    // Str x = (Str)GC_MALLOC(sizeof(GCStr));
    ptr = (char *)GC_MALLOC_ATOMIC(size);
    ptr[0] = '\0';
    m_capacity = size;
}

GCStr::GCStr(const char *src, int size)
{
    ptr = (char *)GC_MALLOC_ATOMIC(size + 1);
    m_capacity = size + 1;
    m_size = size;
    bcopy((void *)src, (void *)ptr, size);
    ptr[size] = '\0';
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

void GCStr::StripLeft()
{
    int i = 0;
    for (; i < m_size; i++)
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
    int i = m_size - 1;
    for (; i >= 0; i--)
    {
        if (!IS_SPACE(ptr[i]))
        {
            break;
        }
    }
    ptr[i + 1] = '\0';
    m_size = i + 1;
}

// void GCStr::Chop(Str s)
// {

//     while ((s->ptr[s->m_size - 1] == '\n' || s->ptr[s->m_size - 1] == '\r') &&
//            s->m_size > 0)
//     {
//         s->m_size--;
//     }
//     s->ptr[s->m_size] = '\0';
// }

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

void GCStr::Insert(int pos, const char *p)
{
    while (*p)
        Insert(pos++, *(p++));
}

void GCStr::Replace(const std::function<void(char &)> &pred)
{
    for (auto p = ptr; *p; p++)
    {
        pred(*p);
    }
}

void GCStr::ToLower()
{
    for (int i = 0; i < m_size; i++)
        ptr[i] = TOLOWER(ptr[i]);
}

void GCStr::ToUpper()
{
    for (int i = 0; i < m_size; i++)
        ptr[i] = TOUPPER(ptr[i]);
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

GCStr *GCStr::UrlEncode()
{
    GCStr *tmp = NULL;
    auto end = ptr + Size();
    for (auto p = ptr; p < end; p++)
    {
        if (*p == ' ')
        {
            // space
            if (tmp == NULL)
                tmp = new GCStr(ptr, (int)(p - ptr));
            tmp->Push('+');
        }
        else if (is_url_unsafe(*p))
        {
            //
            if (tmp == NULL)
                tmp = new GCStr(ptr, (int)(p - ptr));
            char buf[4];
            sprintf(buf, "%%%02X", (unsigned char)*p);
            tmp->Push(buf);
        }
        else
        {
            if (tmp)
                tmp->Push(*p);
        }
    }
    if (tmp)
        return tmp;
    return this;
}

GCStr *GCStr::UrlDecode(bool is_form, bool safe)
{
    Str tmp = NULL;
    char *p = ptr, *ep = ptr + Size(), *q;
    int c;

    for (; p < ep;)
    {
        if (is_form && *p == '+')
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(ptr, (int)(p - ptr));
            tmp->Push(' ');
            p++;
            continue;
        }
        else if (*p == '%')
        {
            q = p;
            c = url_unquote_char(&q);
            if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c)))
            {
                if (tmp == NULL)
                    tmp = Strnew_charp_n(ptr, (int)(p - ptr));
                tmp->Push((char)c);
                p = q;
                continue;
            }
        }
        if (tmp)
            tmp->Push(*p);
        p++;
    }
    if (tmp)
        return tmp;
    return this;
}

int GCStr::Puts(FILE *f) const
{
    return fwrite(ptr, 1, m_size, f);
}

Str Strnew_m_charp(const char *p, ...)
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

void Strcat_m_charp(Str x, ...)
{
    va_list ap;
    va_start(ap, x);

    for (char *p = va_arg(ap, char *); p != NULL; p = va_arg(ap, char *))
    {
        x->Push(p, strlen(p));
    }
}

#define SP_NORMAL 0
#define SP_PREC 1
#define SP_PREC2 2

Str Sprintf(const char *fmt, ...)
{
    int len = 0;
    int status = SP_NORMAL;
    int p = 0;

    va_list ap;
    va_start(ap, fmt);
    for (auto f = fmt; *f; f++)
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

    auto s = Strnew_size(len * 2);
    va_start(ap, fmt);
    vsprintf(s->ptr, fmt, ap);
    va_end(ap);
    // s->m_size = ;
    s->Pop(s->Size() - strlen(s->ptr));
    if (s->Size() > len * 2)
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
