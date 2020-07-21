#pragma once

#include "types.h"
#include <memory>
#include <list>
#include <functional>

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

public:
    Tab() = default;
    ~Tab()
    {
    }
    Buffer *currentBuffer = nullptr;
    Buffer *firstBuffer = nullptr;

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
    bool IsHit(int x, int y)
    {
        return x1 <= x && x <= x2 && y == y;
    }
};
using TabPtr = std::shared_ptr<Tab>;

void EachTab(const std::function<void(const TabPtr&)> callback);
void _newT();
void InitializeTab();
int GetTabCount();
int GetTabbarHeight();
TabPtr GetTabByIndex(int index);
TabPtr GetFirstTab();
TabPtr GetLastTab();
TabPtr GetCurrentTab();

void SetCurrentTab(TabPtr tab);
void SelectRelativeTab(int prec);
void SelectTabByPosition(int x, int y);
void MoveTab(int x);
void deleteTab(TabPtr tab);
void DeleteCurrentTab();
void DeleteAllTabs();
Buffer *GetCurrentbuf();
TabPtr posTab(int x, int y);
void SetCurrentbuf(Buffer *buf);
Buffer *GetFirstbuf();
int HasFirstBuffer();
void SetFirstbuf(Buffer *buffer);
void moveTab(TabPtr src, TabPtr dst, int right);
void calcTabPos();
void followTab(TabPtr tab);
