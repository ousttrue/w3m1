#include "tabbar.h"
#include "mouse.h"
#include "fm.h"
#include "commands.h"
#include "display.h"
#include "html/image.h"

using TabList = std::list<TabPtr>;
TabList g_tabs;
TabList::iterator find(const TabPtr &tab)
{
    return std::find(g_tabs.begin(), g_tabs.end(), tab);
}

std::weak_ptr<Tab> g_current;

void EachTab(const std::function<void(const TabPtr &)> callback)
{
    for (auto &tab : g_tabs)
    {
        callback(tab);
    }
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
TabPtr CreateTabSetCurrent()
{
    // setup buffer
    // auto buf = GetCurrentTab()->GetCurrentBuffer()->Copy();
    // buf->ClearLink();

    // new tab
    auto current = g_current.lock();
    auto found = find(current);
    ++found;
    auto tab = std::make_shared<Tab>();
    g_tabs.insert(found, tab);

    g_current = tab;
    // tab->SetFirstBuffer(buf, true);
    return tab;
}

void deleteTab(TabPtr tab)
{
    if (g_tabs.size() <= 1)
        return;

    auto found = find(tab);

    bool isCurrent = (*found == g_current.lock());

    auto eraseNext = g_tabs.erase(found);

    if (isCurrent)
    {
        // update current tab
        if (eraseNext != g_tabs.end())
        {
            g_current = *eraseNext;
        }
        else
        {
            g_current = g_tabs.back();
        }
    }

    return;
}

void DeleteCurrentTab()
{
    deleteTab(g_current.lock());
}

void DeleteAllTabs()
{
    g_tabs.clear();
}

void moveTab(TabPtr src, TabPtr dst, int right)
{
    if (!src || !dst || src == dst)
        return;

    auto s = find(src);
    auto d = find(dst);

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

static int s_check_target = TRUE;
int check_target()
{
    return s_check_target;
}
void set_check_target(int check)
{
    s_check_target = check;
}

void followTab(TabPtr tab)
{
    BufferPtr buf;

    auto a = retrieveCurrentImg(GetCurrentTab()->GetCurrentBuffer());
    if (!(a && a->image && a->image->map))

        a = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
    if (a == NULL)
        return;

    if (tab == GetCurrentTab())
    {
        set_check_target(FALSE);
        followA();
        set_check_target(TRUE);
        return;
    }
    CreateTabSetCurrent();
    buf = GetCurrentTab()->GetCurrentBuffer();
    set_check_target(FALSE);
    followA();
    set_check_target(TRUE);
    if (tab == NULL)
    {
        if (buf != GetCurrentTab()->GetCurrentBuffer())
            GetCurrentTab()->DeleteBuffer(buf);
        else
            deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentTab()->GetCurrentBuffer())
    {
        // TODO:

        /* buf <- p <- ... <- Currentbuf = c */
        deleteTab(GetCurrentTab());
        // auto c = GetCurrentTab()->GetCurrentBuffer();
        // auto p = prevBuffer(c, buf);
        // p->nextBuffer = NULL;
        tab->SetFirstBuffer(buf);
        SetCurrentTab(tab);
        // for (buf = p; buf; buf = p)
        // {
        //     p = prevBuffer(c, buf);
        //     GetCurrentTab()->BufferPushBeforeCurrent(buf);
        // }
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}
