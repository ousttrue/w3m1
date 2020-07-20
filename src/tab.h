#pragma once

#include "buffer.h"

typedef struct _TabBuffer
{
    struct _TabBuffer *nextTab;
    struct _TabBuffer *prevTab;
    Buffer *currentBuffer;
    Buffer *firstBuffer;
    short x1;
    short x2;
    short y;
} TabBuffer;

TabBuffer *newTab();
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
TabBuffer *deleteTab(TabBuffer *tab);
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
