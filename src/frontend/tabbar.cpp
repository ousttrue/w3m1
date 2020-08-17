#include "tabbar.h"
#include "mouse.h"
#include "fm.h"
#include "commands.h"
#include "display.h"
#include "html/image.h"
#include "w3m.h"
#include "frontend/terms.h"

using TabList = std::list<TabPtr>;
struct TabBar
{
    TabList tabs;
    TabList::iterator find(const TabPtr &tab)
    {
        return std::find(tabs.begin(), tabs.end(), tab);
    }

    std::weak_ptr<Tab> current;

    TabBar()
    {
        tabs.push_back(std::make_shared<Tab>());
        current = tabs.front();
    }
};
static TabBar g_tabBar;

void EachTab(const std::function<void(const TabPtr &)> callback)
{
    for (auto &tab : g_tabBar.tabs)
    {
        callback(tab);
    }
}

int GetTabCount()
{
    return g_tabBar.tabs.size();
}

int GetTabbarHeight()
{
    // 最後のtabのy + 境界線(1)
    return GetLastTab()->Y() + 1;
}

TabPtr GetTabByIndex(int index)
{
    int i = 0;
    for (auto tab : g_tabBar.tabs)
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
    return g_tabBar.tabs.empty() ? nullptr : g_tabBar.tabs.front();
}

TabPtr GetLastTab()
{
    return g_tabBar.tabs.empty() ? nullptr : g_tabBar.tabs.back();
}

TabPtr GetCurrentTab()
{
    auto current = g_tabBar.current.lock();
    return current ? current : nullptr;
}

int GetCurrentTabIndex()
{
    auto current = g_tabBar.current.lock();

    int i = 0;
    for (auto tab : g_tabBar.tabs)
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
    for (auto tab : g_tabBar.tabs)
    {
        if (tab == current)
        {
            g_tabBar.current = tab;
        }
    }
}

void calcTabPos()
{
    int lcol = GetMouseActionMenuStr() ? GetMouseActionMenuWidth() : 0;

    if (g_tabBar.tabs.size() <= 0)
        return;
    int n1 = (::COLS - lcol) / TabCols;
    int n2;
    int ny;
    if (n1 >= g_tabBar.tabs.size())
    {
        n2 = 1;
        ny = 1;
    }
    else
    {
        if (n1 < 0)
            n1 = 0;
        n2 = ::COLS / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (g_tabBar.tabs.size() - n1 - 1) / n2 + 2;
    }
    int na = n1 + n2 * (ny - 1);
    n1 -= (na - g_tabBar.tabs.size()) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    auto it = g_tabBar.tabs.begin();
    for (int iy = 0; iy < ny && *it; iy++)
    {
        int col;
        int nx;
        if (iy == 0)
        {
            nx = n1;
            col = ::COLS - lcol;
        }
        else
        {
            nx = n2 - (na - g_tabBar.tabs.size() + (iy - 1)) / (ny - 1);
            col = ::COLS;
        }
        int ix = 0;
        for (ix = 0; ix < nx && it != g_tabBar.tabs.end(); ix++, ++it)
        {
            (*it)->Calc(col / nx, ix, iy, lcol);
        }
    }
}

TabPtr GetTabByPosition(int x, int y)
{
    TabPtr tab;

    if (GetMouseActionMenuStr() && x < GetMouseActionMenuWidth() && y == 0)
        return nullptr;
    for (auto tab : g_tabBar.tabs)
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
    auto current = g_tabBar.current.lock();
    auto found = g_tabBar.find(current);
    ++found;
    auto tab = std::make_shared<Tab>();
    g_tabBar.tabs.insert(found, tab);

    g_tabBar.current = tab;
    // tab->SetFirstBuffer(buf, true);
    return tab;
}

void deleteTab(TabPtr tab)
{
    if (g_tabBar.tabs.size() <= 1)
        return;

    auto found = g_tabBar.find(tab);

    bool isCurrent = (*found == g_tabBar.current.lock());

    auto eraseNext = g_tabBar.tabs.erase(found);

    if (isCurrent)
    {
        // update current tab
        if (eraseNext != g_tabBar.tabs.end())
        {
            g_tabBar.current = *eraseNext;
        }
        else
        {
            g_tabBar.current = g_tabBar.tabs.back();
        }
    }

    return;
}

void DeleteCurrentTab()
{
    deleteTab(g_tabBar.current.lock());
}

void DeleteAllTabs()
{
    g_tabBar.tabs.clear();
}

void moveTab(TabPtr src, TabPtr dst, int right)
{
    if (!src || !dst || src == dst)
        return;

    auto s = g_tabBar.find(src);
    auto d = g_tabBar.find(dst);

    if (right)
    {
        ++d;
    }

    auto end = s;
    ++end;
    g_tabBar.tabs.insert(d, s, end);
}

void SelectRelativeTab(int prec)
{
    if (g_tabBar.tabs.size() <= 1)
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
    if (tab == GetCurrentTab())
    {
        set_check_target(FALSE);
        followA(&w3mApp::Instance());
        set_check_target(TRUE);
        return;
    }

    CreateTabSetCurrent();
    auto buf = GetCurrentTab()->GetCurrentBuffer();
    set_check_target(FALSE);
    followA(&w3mApp::Instance());
    set_check_target(TRUE);
    if (tab == NULL)
    {
        // if (buf != GetCurrentTab()->GetCurrentBuffer())
        //     GetCurrentTab()->DeleteBuffer(buf);
        // else
        //     deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentTab()->GetCurrentBuffer())
    {
        // TODO:

        /* buf <- p <- ... <- Currentbuf = c */
        deleteTab(GetCurrentTab());
        // auto c = GetCurrentTab()->GetCurrentBuffer();
        // auto p = prevBuffer(c, buf);
        // p->nextBuffer = NULL;
        // tab->Push(buf);
        SetCurrentTab(tab);
        // for (buf = p; buf; buf = p)
        // {
        //     p = prevBuffer(c, buf);
        //     GetCurrentTab()->BufferPushBeforeCurrent(buf);
        // }
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}
