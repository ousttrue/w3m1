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
#include <stdint.h>
#include <gc_cpp.h>
#include <functional>
#include <string_view>

//
// http://aitoweb.world.coocan.jp/gc/gc.html
//

class GCStr : public gc_cleanup
{
    int m_capacity = 0;
    int m_size = 0;

private:
    void Allocate(int size);
    void Assign(const char *src, int size);

public:
    char *ptr;

    const char *const c_str() const { return ptr; }
    const uint8_t *const data() const { return reinterpret_cast<const uint8_t *const>(ptr); }

    GCStr(int size = 32);
    GCStr(const char *src, int size);
    GCStr(const char *src);
    ~GCStr();
    int Size() const { return m_size; }
    int Cmp(const GCStr *y) const;
    int Cmp(const GCStr *y, int len) const;
    int Cmp(const char *y) const;
    int Cmp(const char *y, int n) const;
    int ICaseCmp(const GCStr *y) const;
    int ICaseCmp(const GCStr *y, int n) const;
    int ICaseCmp(const char *y) const;
    int ICaseCmp(const char *y, int n) const;
    char Back() const;

    GCStr *Clone() const;
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
        CopyFrom(src->ptr, src->m_size);
    }
    void Push(const char *src, int size);
    void Push(const char *src)
    {
        Push(src, (int)strlen(src));
    }
    void Push(std::string_view src)
    {
        Push(src.data(), src.size());
    }
    void Push(const GCStr *src)
    {
        Push(src->ptr, src->m_size);
    }
    void Push(char y)
    {
        if (y == 0)
        {
            return;
        }
        Push(&y, 1);
    }

    // recursive variadic template
    void Concat()
    {
        // stop recursion
    }
    template <typename T, typename... ARGS>
    void Concat(const T &t, ARGS... args)
    {
        Push(t);
        Concat(args...);
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
    void Insert(int pos, const char *src, int size);
    void Insert(int pos, const GCStr *src)
    {
        Insert(pos, src->ptr);
    }
    void Insert(int pos, std::string_view src)
    {
        Insert(pos, src.data(), src.size());
    }
    void Replace(const std::function<void(char &)> &pred);
    void ToLower();
    void ToUpper();
    GCStr *AlignLeft(int width) const;
    GCStr *AlignRight(int width) const;
    GCStr *AlignCenter(int width) const;
    GCStr *UrlEncode();
    GCStr *UrlDecode(bool is_form, bool safe);
    int Puts(FILE *f) const;
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

inline Str Strnew_charp_n(const char *src, int size)
{
    return new GCStr(src, size);
}

inline Str Strnew(std::string_view src)
{
    return new GCStr(src.data(), src.size());
}

// recursive variadic template
template <typename... ARGS>
void Strcat_m_charp(Str str, ARGS... args)
{
    str->Concat(args...);
}

// recursive variadic template
template <typename... ARGS>
Str Strnew_m_charp(ARGS... args)
{
    auto str = new GCStr();
    str->Concat(args...);
    return str;
}

Str Sprintf(const char *fmt, ...);
Str Strfgets(FILE *);
Str Strfgetall(FILE *);
