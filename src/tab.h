#pragma once

#include "types.h"
#include <memory>
#include <list>

///
/// [ tab ]
///
class Tab
{
    short x1 = -1;
    short x2 = -1;
    short y = -1;

public:
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
using TabBufferPtr = std::shared_ptr<Tab>;

std::list<TabBufferPtr> &Tabs();

void _newT();
void InitializeTab();
int GetTabCount();
int GetTabbarHeight();
Tab *GetTabByIndex(int index);
Tab *GetFirstTab();
Tab *GetLastTab();
Tab *GetCurrentTab();

void SetCurrentTab(Tab *tab);
void SelectRelativeTab(int prec);
void SelectTabByPosition(int x, int y);
void MoveTab(int x);
void deleteTab(Tab *tab);
void DeleteCurrentTab();
void DeleteAllTabs();
Buffer *GetCurrentbuf();
Tab *posTab(int x, int y);
void SetCurrentbuf(Buffer *buf);
Buffer *GetFirstbuf();
int HasFirstBuffer();
void SetFirstbuf(Buffer *buffer);
void moveTab(Tab *src, Tab *dst, int right);
void calcTabPos();
void followTab(Tab *tab);
