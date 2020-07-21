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

std::list<TabPtr> g_tabs;
std::weak_ptr<Tab> g_current;

void EachTab(const std::function<void(const TabPtr &)> callback)
{
    for (auto &tab : g_tabs)
    {
        callback(tab);
    }
}

// currentPrev -> buf -> current 
void Tab::BufferPushFront(Buffer *buf)
{
    deleteImage(GetCurrentbuf());
    if (clear_buffer)
    {
        tmpClearBuffer(GetCurrentbuf());
    }

    if (GetFirstbuf() == GetCurrentbuf())
    {
        buf->nextBuffer = GetFirstbuf();
        SetCurrentbuf(buf);
        SetFirstbuf(buf);
    }
    else
    {
        auto b = prevBuffer(GetFirstbuf(), GetCurrentbuf());
        if (b != NULL)
        {
            b->nextBuffer = buf;
            buf->nextBuffer = GetCurrentbuf();
            SetCurrentbuf(buf);
        }
    }
#ifdef USE_BUFINFO
    saveBufferInfo();
#endif
}


void InitializeTab()
{
    g_tabs.push_back(std::make_shared<Tab>());
    g_current = g_tabs.front();
}

int GetTabCount()
{
    return g_tabs.size();
}

int GetTabbarHeight()
{
    // 最後のtabのy + 境界線(1)
    return GetLastTab()->Y() + 1;
}

TabPtr GetTabByIndex(int index)
{
    int i = 0;
    for (auto tab : g_tabs)
    {
        if (i == index)
        {
            return tab;
        }
        ++i;
    }
    throw std::runtime_error("not found");
}

TabPtr GetFirstTab()
{
    return g_tabs.empty() ? nullptr : g_tabs.front();
}

TabPtr GetLastTab()
{
    return g_tabs.empty() ? nullptr : g_tabs.back();
}

TabPtr GetCurrentTab()
{
    auto current = g_current.lock();
    return current ? current : nullptr;
}

int GetCurrentTabIndex()
{
    auto current = g_current.lock();

    int i = 0;
    for (auto tab : g_tabs)
    {
        if (tab == current)
        {
            return i;
        }
        ++i;
    }
    throw std::runtime_error("not found");
}

void SetCurrentTab(TabPtr current)
{
    for (auto tab : g_tabs)
    {
        if (tab == current)
        {
            g_current = tab;
        }
    }
}

void calcTabPos()
{
    int lcol = GetMouseActionMenuStr() ? GetMouseActionMenuWidth() : 0;

    if (g_tabs.size() <= 0)
        return;
    int n1 = (COLS - lcol) / TabCols;
    int n2;
    int ny;
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
    int na = n1 + n2 * (ny - 1);
    n1 -= (na - g_tabs.size()) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    auto it = g_tabs.begin();
    for (int iy = 0; iy < ny && *it; iy++)
    {
        int col;
        int nx;
        if (iy == 0)
        {
            nx = n1;
            col = COLS - lcol;
        }
        else
        {
            nx = n2 - (na - g_tabs.size() + (iy - 1)) / (ny - 1);
            col = COLS;
        }
        int ix = 0;
        for (ix = 0; ix < nx && it != g_tabs.end(); ix++, ++it)
        {
            (*it)->Calc(col / nx, ix, iy, lcol);
        }
    }
}

TabPtr posTab(int x, int y)
{
    TabPtr tab;

    if (GetMouseActionMenuStr() && x < GetMouseActionMenuWidth() && y == 0)
        return nullptr;
    for (auto tab : g_tabs)
    {
        if (tab->IsHit(x, y))
        {
            return tab;
        }
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
    auto tab = std::make_shared<Tab>();
    tab->firstBuffer = tab->currentBuffer = buf;
    g_tabs.insert(found, tab);

    g_current = tab;
}

void deleteTab(TabPtr tab)
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

    auto found = std::find(g_tabs.begin(), g_tabs.end(), tab);
    g_tabs.erase(found);

    return;
}

void DeleteCurrentTab()
{
    deleteTab(g_current.lock());
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

void moveTab(TabPtr src, TabPtr dst, int right)
{
    if (!src || !dst || src == dst)
        return;

    auto s = std::find(g_tabs.begin(), g_tabs.end(), src);
    auto d = std::find(g_tabs.begin(), g_tabs.end(), dst);

    if (right)
    {
        ++d;
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
    TabPtr tab = posTab(x, y);
    if (!tab)
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

void followTab(TabPtr tab)
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
            GetCurrentTab()->BufferPushFront(buf);
        }
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
