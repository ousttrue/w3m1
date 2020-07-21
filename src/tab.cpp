
#include "tab.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "display.h"
#include "dispatcher.h"
#include "mouse.h"
#include "public.h"
#include "commands.h"
#include <stdexcept>
#include <algorithm>

std::list<TabBufferPtr> g_tabs;
std::weak_ptr<TabBuffer> g_current;

std::list<TabBufferPtr> &Tabs()
{
    return g_tabs;
}

void InitializeTab()
{
    g_tabs.push_back(std::make_shared<TabBuffer>());
    g_current = g_tabs.front();
}

int GetTabCount()
{
    return g_tabs.size();
}

TabBuffer *GetTabByIndex(int index)
{
    int i = 0;
    for (auto tab : g_tabs)
    {
        if (i == index)
        {
            return tab.get();
        }
        ++i;
    }
    throw std::runtime_error("not found");
}

TabBuffer *GetFirstTab()
{
    return g_tabs.empty() ? nullptr : g_tabs.front().get();
}

TabBuffer *GetLastTab()
{
    return g_tabs.empty() ? nullptr : g_tabs.back().get();
}

TabBuffer *GetCurrentTab()
{
    auto current = g_current.lock();
    return current ? current.get() : nullptr;
}

int GetCurrentTabIndex()
{
    auto current = g_current.lock();

    int i = 0;
    for (auto tab : g_tabs)
    {
        if (tab.get() == current.get())
        {
            return i;
        }
        ++i;
    }
    throw std::runtime_error("not found");
}

void SetCurrentTab(TabBuffer *current)
{
    for (auto tab : g_tabs)
    {
        if (tab.get() == current)
        {
            g_current = tab;
        }
    }
}

void calcTabPos()
{
    int lcol = 0, rcol = 0, col;
    int n1, n2, na, nx, ny, ix, iy;

    lcol = GetMouseActionMenuStr() ? GetMouseActionMenuWidth() : 0;

    if (g_tabs.size() <= 0)
        return;
    n1 = (COLS - rcol - lcol) / TabCols;
    if (n1 >= g_tabs.size())
    {
        n2 = 1;
        ny = 1;
    }
    else
    {
        if (n1 < 0)
            n1 = 0;
        n2 = COLS / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (g_tabs.size() - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - g_tabs.size()) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    auto it = g_tabs.begin();
    for (iy = 0; iy < ny && *it; iy++)
    {
        if (iy == 0)
        {
            nx = n1;
            col = COLS - rcol - lcol;
        }
        else
        {
            nx = n2 - (na - g_tabs.size() + (iy - 1)) / (ny - 1);
            col = COLS;
        }
        ix = 0;
        for (ix = 0; ix < nx && it != g_tabs.end(); ix++, ++it)
        {
            auto tab = *it;
            tab->x1 = col * ix / nx;
            tab->x2 = col * (ix + 1) / nx - 1;
            tab->y = iy;
            if (iy == 0)
            {
                tab->x1 += lcol;
                tab->x2 += lcol;
            }
        }
    }
}

TabBuffer *posTab(int x, int y)
{
    TabBuffer *tab;

    if (GetMouseActionMenuStr() && x < GetMouseActionMenuWidth() && y == 0)
        return NO_TABBUFFER;
    if (y > GetLastTab()->y)
        return NULL;
    for (auto tab : g_tabs)
    {
        if (tab->x1 <= x && x <= tab->x2 && tab->y == y)
            return tab.get();
    }
    return NULL;
}

// setup
void _newT()
{
    // setup buffer
    auto buf = newBuffer(GetCurrentbuf()->width);
    copyBuffer(buf, GetCurrentbuf());
    buf->nextBuffer = NULL;
    for (int i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    (*buf->clone)++;

    // new tab
    auto current = g_current.lock();
    auto found = std::find(g_tabs.begin(), g_tabs.end(), current);
    ++found;
    auto tab = std::make_shared<TabBuffer>();
    tab->firstBuffer = tab->currentBuffer = buf;
    g_tabs.insert(found, tab);

    g_current = tab;
}

void deleteTab(TabBuffer *tab)
{
    if (g_tabs.size() <= 1)
        return;

    // clear buffer
    Buffer *buf, *next;
    buf = tab->firstBuffer;
    while (buf && buf != NO_BUFFER)
    {
        next = buf->nextBuffer;
        discardBuffer(buf);
        buf = next;
    }

    auto found = std::find_if(g_tabs.begin(), g_tabs.end(), [tab](auto t) {
        return t.get() == tab;
    });
    g_tabs.erase(found);

    return;
}

void DeleteCurrentTab()
{
    deleteTab(g_current.lock().get());
}

void DeleteAllTabs()
{
    for (auto it = g_tabs.begin(); it != g_tabs.end();)
    {
        while (HasFirstBuffer())
        {
            auto buf = GetFirstbuf()->nextBuffer;
            discardBuffer(GetFirstbuf());
            SetFirstbuf(buf);
        }
        it = g_tabs.erase(it);
    }
}

void moveTab(TabBuffer *src, TabBuffer *dst, int right)
{
    if (!src || !dst || src == dst || src == NO_TABBUFFER || dst == NO_TABBUFFER)
        return;

    auto s = std::find_if(g_tabs.begin(), g_tabs.end(), [src](auto t) {
        return t.get() == src;
    });
    auto d = std::find_if(g_tabs.begin(), g_tabs.end(), [dst](auto t) {
        return t.get() == dst;
    });

    if (right)
    {
        ++dst;
    }

    auto end = s;
    ++end;
    g_tabs.insert(d, s, end);
}

void SelectRelativeTab(int prec)
{
    if (g_tabs.size() <= 1)
    {
        return;
    }
    if (prec == 0)
    {
        return;
    }

    auto current = GetCurrentTabIndex();
    auto index = current + prec;
    if (index < 0)
    {
        index = 0;
    }
    else if (index >= GetTabCount())
    {
        index = GetTabCount() - 1;
    }

    SetCurrentTab(GetTabByIndex(index));
}

void SelectTabByPosition(int x, int y)
{
    TabBuffer *tab = posTab(x, y);
    if (!tab || tab == NO_TABBUFFER)
        return;
    SetCurrentTab(tab);
}

void MoveTab(int x)
{
    if (x == 0)
    {
        return;
    }
    auto current = GetCurrentTabIndex();
    auto index = current + x;
    if (index < 0)
    {
        index = 0;
    }
    else if (index >= GetTabCount())
    {
        index = GetTabCount() - 1;
    }
    auto tab = GetTabByIndex(index);
    moveTab(GetCurrentTab(), tab ? tab : GetLastTab(), x > 0);
}

Buffer *GetCurrentbuf()
{
    return g_current.lock()->currentBuffer;
}

void SetCurrentbuf(Buffer *buf)
{
    g_current.lock()->currentBuffer = buf;
}

Buffer *GetFirstbuf()
{
    return g_current.lock()->firstBuffer;
}

int HasFirstBuffer()
{
    return GetFirstbuf() && GetFirstbuf() != NO_BUFFER;
}

void SetFirstbuf(Buffer *buffer)
{
    g_current.lock()->firstBuffer = buffer;
}

void followTab(TabBuffer *tab)
{
    Buffer *buf;
    Anchor *a;

#ifdef USE_IMAGE
    a = retrieveCurrentImg(GetCurrentbuf());
    if (!(a && a->image && a->image->map))
#endif
        a = retrieveCurrentAnchor(GetCurrentbuf());
    if (a == NULL)
        return;

    if (tab == GetCurrentTab())
    {
        set_check_target(FALSE);
        followA();
        set_check_target(TRUE);
        return;
    }
    _newT();
    buf = GetCurrentbuf();
    set_check_target(FALSE);
    followA();
    set_check_target(TRUE);
    if (tab == NULL)
    {
        if (buf != GetCurrentbuf())
            delBuffer(buf);
        else
            deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentbuf())
    {
        /* buf <- p <- ... <- Currentbuf = c */
        Buffer *c, *p;

        c = GetCurrentbuf();
        p = prevBuffer(c, buf);
        p->nextBuffer = NULL;
        SetFirstbuf(buf);
        deleteTab(GetCurrentTab());
        SetCurrentTab(tab);
        for (buf = p; buf; buf = p)
        {
            p = prevBuffer(c, buf);
            pushBuffer(buf);
        }
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
