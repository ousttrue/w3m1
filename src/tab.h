#pragma once

#include "buffer.h"

struct TabBuffer
{
    TabBuffer *nextTab;
    TabBuffer *prevTab;
    Buffer *currentBuffer;
    Buffer *firstBuffer;
    short x1;
    short x2;
    short y;

    TabBuffer *AddNext(Buffer *buffer);
    void Remove();
};

void _newT();
void InitializeTab();
int GetTabCount();
TabBuffer *GetFirstTab();
void SetFirstTab(TabBuffer *tab);
TabBuffer *GetLastTab();
void SetLastTab(TabBuffer *tab);
TabBuffer *GetCurrentTab();
void SetCurrentTab(TabBuffer *tab);
void SelectRelativeTab(int prec);
void SelectTabByPosition(int x, int y);
TabBuffer *numTab(int n);
void MoveTab(int x);
void deleteTab(TabBuffer *tab);
void DeleteCurrentTab();
void DeleteAllTabs();
Buffer *GetCurrentbuf();
TabBuffer *posTab(int x, int y);
void SetCurrentbuf(Buffer *buf);
Buffer *GetFirstbuf();
int HasFirstBuffer();
// int NoFirstBuffer();
void SetFirstbuf(Buffer *buffer);
void moveTab(TabBuffer *t, TabBuffer *t2, int right);
void calcTabPos();
void followTab(TabBuffer *tab);
