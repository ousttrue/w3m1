#pragma once
#include <string_view>
#include <sstream>
#include <vector>
#include "myctype.h"

namespace svu
{

inline std::string_view strip_left(std::string_view src)
{
    while (src.size() && IS_SPACE(src.front()))
    {
        src.remove_prefix(1);
    }
    return src;
}

inline std::string_view strip_right(std::string_view src)
{
    while (src.size() && IS_SPACE(src.back()))
    {
        src.remove_suffix(1);
    }
    return src;
}

inline std::string_view strip(std::string_view src)
{
    return strip_left(strip_right(src));
}

inline std::tuple<std::string_view, std::string_view> split(std::string_view src, char delemeter)
{
    auto pos = src.find(delemeter);
    if (pos == std::string::npos)
    {
        return {};
    }

    auto key = strip(src.substr(0, pos));
    auto value = strip(src.substr(pos + 1));

    return {key, value};
}

struct splitter
{
    std::string_view src;
    using Delemeter = std::function<bool(char)>;
    Delemeter delemeter;

    struct Iterator
    {
        const Delemeter *delemeter = nullptr;
        std::string_view view;
        std::string_view next;

        Iterator()
        {
        }

        Iterator(std::string_view p, const Delemeter *d)
            : delemeter(d)
        {
            set(p, d);
        }

    private:
        void set(std::string_view p, const Delemeter *d)
        {
            assert(d);

            // skip left
            while (p.size() && (*d)(p[0]))
                p.remove_prefix(1);
            if (p.empty())
            {
                // end
                view = {};
                next = {};
                return;
            }

            // search tail
            int i = 1;
            for (; i < p.size(); ++i)
            {
                if ((*d)(p[i]))
                {
                    break;
                }
            }

            view = p.substr(0, i);
            if (i >= p.size())
            {
                // no next
                next = {};
            }
            else
            {
                next = p.substr(i);
            }
        }

    public:
        std::string_view operator*() const
        {
            return view;
        }

        Iterator &operator++()
        {
            set(next, delemeter);
            return *this;
        }

        bool operator==(const Iterator &rhs) const
        {
            return view == rhs.view && next == rhs.next;
        }
    };

    splitter(std::string_view s, const Delemeter &d)
        : src(s), delemeter(d)
    {
    }

    Iterator begin()
    {
        return Iterator(src, &delemeter);
    }

    Iterator end()
    {
        return {};
    }
};

// ignore case equal
inline bool ic_eq(std::string_view l, std::string_view r)
{
    if (l.size() != r.size())
    {
        return false;
    }
    auto rr = r.begin();
    for (auto ll = l.begin(); ll != l.end(); ++ll, ++rr)
    {
        if (tolower(*ll) != tolower(*rr))
        {
            return false;
        }
    }
    return true;
}

inline bool ic_begin_with(std::string_view src, std::string_view find)
{
    if (src.size() < find.size())
    {
        return false;
    }
    return ic_eq(src.substr(0, find.size()), find);
}

inline bool ic_ends_with(std::string_view src, std::string_view find)
{
    if (src.size() < find.size())
    {
        return false;
    }
    return ic_eq(src.substr(src.size() - find.size()), find);
}

inline void _join(std::string_view delemeter, std::ostream &os, int index)
{
}
template <typename... ARGS>
inline void _join(std::string_view delemeter, std::ostream &os, int index, std::string_view arg, ARGS... args)
{
    if (index > 0)
    {
        os << delemeter;
    }
    os << arg;
    _join(delemeter, os, index + 1, args...);
}
template <typename... ARGS>
inline std::string join(std::string_view delemeter, ARGS... args)
{
    std::stringstream ss;
    _join(delemeter, ss, 0, args...);
    return ss.str();
}

inline bool is_null_or_space(std::string_view s)
{
    return strip_left(s).empty();
}

} // namespace  svu
