#pragma once

#include <memory>
#include <list>
#include <functional>
struct Tab;
using BufferPtr = struct Buffer *;
using BufferList = std::list<BufferPtr>;

///
/// [ tab ]
///
class Tab
{
    short x1 = -1;
    short x2 = -1;
    short y = -1;

    // avoid copy
    Tab(const Tab &) = delete;
    Tab &operator=(const Tab &) = delete;

    BufferList buffers;
    BufferPtr currentBuffer = nullptr;
    BufferList::const_iterator find(BufferPtr b) const
    {
        return std::find(buffers.begin(), buffers.end(), b);
    }

public:
    Tab() = default;
    ~Tab();

    int Left() const { return x1; }
    int Right() const { return x2; }
    int Width() const
    {
        // -1 ?
        return x2 - x1 - 1;
    }
    int Y() const { return y; }
    void Calc(int w, int ix, int iy, int left)
    {
        this->x1 = w * ix;
        this->x2 = w * (ix + 1) - 1;
        this->y = iy;
        if (iy == 0)
        {
            this->x1 += left;
            this->x2 += left;
        }
    }
    bool IsHit(int x, int y) const
    {
        return x1 <= x && x <= x2 && y == y;
    }

    // buffer
    int GetCurrentBufferIndex() const;
    BufferPtr GetFirstBuffer() { return buffers.front(); }
    BufferPtr GetCurrentBuffer() const { return currentBuffer; }
    BufferPtr PrevBuffer(BufferPtr buf) const;
    BufferPtr NextBuffer(BufferPtr buf) const;
    BufferPtr GetBuffer(int n) const;
    BufferPtr NamedBuffer(const char *name) const;
    BufferPtr SelectBuffer(BufferPtr currentbuf, char *selectchar) const;

    void SetFirstBuffer(BufferPtr buf);
    void PushBufferCurrentPrev(BufferPtr buf);
    void PushBufferCurrentNext(BufferPtr buf);

    void SetCurrentBuffer(BufferPtr buf);
    void DeleteBuffer(BufferPtr delbuf);
    void ReplaceBuffer(BufferPtr delbuf, BufferPtr newbuf);

private:
    bool IsConnectFirstCurrent() const;
};
using TabPtr = std::shared_ptr<Tab>;

void followTab(TabPtr tab);
