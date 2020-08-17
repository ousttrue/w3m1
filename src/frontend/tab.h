#pragma once

#include <memory>
#include <list>
#include <vector>
#include <functional>
struct Tab;
using BufferPtr = std::shared_ptr<struct Buffer>;
using BufferList = std::vector<BufferPtr>;

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

    BufferList m_buffers;

    int m_current = 0;
    BufferList::const_iterator find(BufferPtr b) const
    {
        return std::find(m_buffers.begin(), m_buffers.end(), b);
    }
    BufferList::const_iterator findCurrent() const
    {
        if (m_current < 0 || m_current > m_buffers.size())
        {
            return m_buffers.end();
        }
        return m_buffers.begin() + m_current;
    }
    BufferPtr GetBuffer(int n) const
    {
        if (n < 0 || n >= m_buffers.size())
        {
            return nullptr;
        }
        return m_buffers[n];
    }

public:
    Tab() = default;
    ~Tab();

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
    int GetBufferCount() const { return m_buffers.size(); }
    int GetBufferIndex(const BufferPtr &buf) const;
    int GetCurrentBufferIndex() const { return m_current; }
    void SetCurrentBufferIndex(int index) { m_current = index; }
    BufferPtr GetCurrentBuffer() const { return GetBuffer(m_current); }

private:
    bool ValidateIndex()
    {
        if (m_current < 0)
        {
            m_current = 0;
            return false;
        }

        if (m_current >= m_buffers.size())
        {
            m_current = m_buffers.size() - 1;
            return false;
        }

        return true;
    }

public:
    // history の 先頭に追加する
    void Push(const BufferPtr &buf);
    // remove current back, for remove history(local cgi etc)
    void DeleteBack();
    // void DeleteBuffer(const BufferPtr &buf);

    bool Forward()
    {
        ++m_current;
        return ValidateIndex();
    }
    bool Back(bool remove = false)
    {
        --m_current;
        bool valid = ValidateIndex();
        if (valid && remove)
        {
            m_buffers.resize(m_current + 1);
        }
        return valid;
    }
};
using TabPtr = std::shared_ptr<Tab>;

void followTab(TabPtr tab);
