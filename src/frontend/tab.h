#pragma once

#include <memory>
#include <list>
#include <vector>
#include <functional>
#include "stream/url.h"
struct Tab;
using BufferPtr = std::shared_ptr<struct Buffer>;

///
/// [ tab ]
///
class Tab
{
    short m_left = -1;
    short m_right = -1;
    short y = -1;

    // avoid copy
    Tab(const Tab &) = delete;
    Tab &operator=(const Tab &) = delete;

    std::vector<URL> m_history;
    int m_current = -1;
    BufferPtr m_buffer;
    // BufferList::const_iterator find(BufferPtr b) const
    // {
    //     return std::find(m_history.begin(), m_history.end(), b);
    // }
    // BufferList::const_iterator findCurrent() const
    // {
    //     if (m_current < 0 || m_current > m_history.size())
    //     {
    //         return m_history.end();
    //     }
    //     return m_history.begin() + m_current;
    // }
    // BufferPtr GetBuffer(int n) const
    // {
    //     if (n < 0 || n >= m_history.size())
    //     {
    //         return nullptr;
    //     }
    //     return m_history[n];
    // }


public:
    Tab() = default;
    ~Tab();

    bool SetCurrent(int index);
    int Left() const { return m_left; }
    int Right() const { return m_right; }
    int Width() const
    {
        // -1 ?
        return m_right - m_left - 1;
    }
    int Y() const { return y; }
    void Calc(int w, int ix, int iy, int left)
    {
        this->m_left = w * ix;
        this->m_right = w * (ix + 1) - 1;
        this->y = iy;
        if (iy == 0)
        {
            this->m_left += left;
            this->m_right += left;
        }
    }
    bool IsHit(int x, int y) const
    {
        return m_left <= x && x <= m_right && y == y;
    }

    // buffer
    int GetBufferCount() const { return m_history.size(); }
    // int GetBufferIndex(const BufferPtr &buf) const;
    int GetCurrentBufferIndex() const { return m_current; }
    BufferPtr GetCurrentBuffer() const { return m_buffer; }

public:
    // history の 先頭に追加する
    void Push(const URL &url);
    // remove current back, for remove history(local cgi etc)
    void DeleteBack();
    // void DeleteBuffer(const BufferPtr &buf);

    bool Forward()
    {
        return SetCurrent(m_current + 1);
    }
    bool Back(bool remove = false)
    {
        auto success = SetCurrent(m_current - 1);
        if (remove)
        {
            m_history.resize(m_current + 1);
        }
        return success;
    }
};
using TabPtr = std::shared_ptr<Tab>;

void followTab(TabPtr tab, const struct CommandContext &context);
