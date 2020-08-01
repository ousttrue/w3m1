#pragma once

// #include "frontend/buffer.h"
#include <memory>
#include <list>
#include <functional>
struct Tab;
using BufferPtr = struct Buffer *;

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
    BufferPtr currentBuffer = nullptr;
    BufferPtr firstBuffer = nullptr;

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
    BufferPtr GetFirstBuffer() { return firstBuffer; }
    BufferPtr GetCurrentBuffer() const { return currentBuffer; }
    BufferPtr PrevBuffer(BufferPtr buf) const;
    BufferPtr NextBuffer(BufferPtr buf) const;
    BufferPtr GetBuffer(int n) const;
    BufferPtr NamedBuffer(const char *name) const;
    BufferPtr SelectBuffer(BufferPtr currentbuf, char *selectchar) const;

    void SetFirstBuffer(BufferPtr buf, bool isCurrent = false);
    void SetCurrentBuffer(BufferPtr buf);
    void PushBufferCurrentPrev(BufferPtr buf);
    void PushBufferCurrentNext(BufferPtr buf);
    void DeleteBuffer(BufferPtr delbuf);
    void ReplaceBuffer(BufferPtr delbuf, BufferPtr newbuf);

private:
    bool IsConnectFirstCurrent() const;
};
using TabPtr = std::shared_ptr<Tab>;

void calcTabPos();
void followTab(TabPtr tab);
