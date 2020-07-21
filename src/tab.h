#pragma once

#include "types.h"
#include <memory>
#include <list>

struct TabBuffer
{
    Buffer *currentBuffer = nullptr;
    Buffer *firstBuffer = nullptr;
    short x1 = -1;
    short x2 = -1;
    short y = -1;
};
using TabBufferPtr = std::shared_ptr<TabBuffer>;

std::list<TabBufferPtr> &Tabs();

void _newT();
void InitializeTab();
int GetTabCount();
TabBuffer *GetTabByIndex(int index);
TabBuffer *GetFirstTab();
TabBuffer *GetLastTab();
TabBuffer *GetCurrentTab();

void SetCurrentTab(TabBuffer *tab);
void SelectRelativeTab(int prec);
void SelectTabByPosition(int x, int y);
void MoveTab(int x);
void deleteTab(TabBuffer *tab);
void DeleteCurrentTab();
void DeleteAllTabs();
Buffer *GetCurrentbuf();
TabBuffer *posTab(int x, int y);
void SetCurrentbuf(Buffer *buf);
Buffer *GetFirstbuf();
int HasFirstBuffer();
void SetFirstbuf(Buffer *buffer);
void moveTab(TabBuffer *src, TabBuffer *dst, int right);
void calcTabPos();
void followTab(TabBuffer *tab);
