extern "C"
{
#include "tab.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "display.h"
#include "dispatcher.h"
}

static TabBuffer *g_FirstTab = nullptr;
static TabBuffer *g_LastTab = nullptr;
static TabBuffer *g_CurrentTab = nullptr;
static int g_nTab = 0;

void InitializeTab()
{
    g_FirstTab = g_LastTab = g_CurrentTab = newTab();
    g_nTab = 1;
}

int GetTabCount()
{
    return g_nTab;
}

TabBuffer *GetFirstTab()
{
    return g_FirstTab;
}

void SetFirstTab(TabBuffer *tab)
{
    g_FirstTab = tab;
}

TabBuffer *GetLastTab()
{
    return g_LastTab;
}

void SetLastTab(TabBuffer *tab)
{
    g_LastTab = tab;
}

TabBuffer *GetCurrentTab()
{
    return g_CurrentTab;
}

void SetCurrentTab(TabBuffer *tab)
{
    g_CurrentTab = tab;
}

void calcTabPos()
{
    TabBuffer *tab;
#if 0
    int lcol = 0, rcol = 2, col;
#else
    int lcol = 0, rcol = 0, col;
#endif
    int n1, n2, na, nx, ny, ix, iy;

#ifdef USE_MOUSE
    lcol = mouse_action.menu_str ? mouse_action.menu_width : 0;
#endif

    if (g_nTab <= 0)
        return;
    n1 = (COLS - rcol - lcol) / TabCols;
    if (n1 >= g_nTab)
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
        ny = (g_nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - g_nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = g_FirstTab;
    for (iy = 0; iy < ny && tab; iy++)
    {
        if (iy == 0)
        {
            nx = n1;
            col = COLS - rcol - lcol;
        }
        else
        {
            nx = n2 - (na - g_nTab + (iy - 1)) / (ny - 1);
            col = COLS;
        }
        for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab)
        {
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

    if (mouse_action.menu_str && x < mouse_action.menu_width && y == 0)
        return NO_TABBUFFER;
    if (y > GetLastTab()->y)
        return NULL;
    for (tab = g_FirstTab; tab; tab = tab->nextTab)
    {
        if (tab->x1 <= x && x <= tab->x2 && tab->y == y)
            return tab;
    }
    return NULL;
}

TabBuffer *numTab(int n)
{
    TabBuffer *tab;
    int i;

    if (n == 0)
        return g_CurrentTab;
    if (n == 1)
        return g_FirstTab;
    if (g_nTab <= 1)
        return NULL;
    for (tab = g_FirstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
        ;
    return tab;
}

TabBuffer *newTab(void)
{
    TabBuffer *n;

    n = New(TabBuffer);
    if (n == NULL)
        return NULL;
    n->nextTab = NULL;
    n->currentBuffer = NULL;
    n->firstBuffer = NULL;
    return n;
}

// setup
void _newT()
{
    auto tag = newTab();
    if (!tag)
        return;

    auto buf = newBuffer(GetCurrentbuf()->width);
    copyBuffer(buf, GetCurrentbuf());
    buf->nextBuffer = NULL;
    for (int i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    (*buf->clone)++;
    tag->firstBuffer = tag->currentBuffer = buf;

    tag->nextTab = g_CurrentTab->nextTab;
    tag->prevTab = g_CurrentTab;
    if (g_CurrentTab->nextTab)
        g_CurrentTab->nextTab->prevTab = tag;
    else
        SetLastTab(tag);
    g_CurrentTab->nextTab = tag;
    g_CurrentTab = tag;
    g_nTab++;
}

TabBuffer *deleteTab(TabBuffer *tab)
{
    Buffer *buf, *next;

    if (g_nTab <= 1)
        return g_FirstTab;
    if (tab->prevTab)
    {
        if (tab->nextTab)
            tab->nextTab->prevTab = tab->prevTab;
        else
            SetLastTab(tab->prevTab);
        tab->prevTab->nextTab = tab->nextTab;
        if (tab == g_CurrentTab)
            g_CurrentTab = tab->prevTab;
    }
    else
    { /* tab == FirstTab */
        tab->nextTab->prevTab = NULL;
        g_FirstTab = tab->nextTab;
        if (tab == g_CurrentTab)
            g_CurrentTab = tab->nextTab;
    }
    g_nTab--;
    buf = tab->firstBuffer;
    while (buf && buf != NO_BUFFER)
    {
        next = buf->nextBuffer;
        discardBuffer(buf);
        buf = next;
    }
    return g_FirstTab;
}

void DeleteCurrentTab()
{
    if (GetTabCount() > 1)
        deleteTab(g_CurrentTab);
}

void DeleteAllTabs()
{
    for (g_CurrentTab = GetFirstTab(); g_CurrentTab; g_CurrentTab = g_CurrentTab->nextTab)
    {
        while (HasFirstBuffer())
        {
            auto buf = GetFirstbuf()->nextBuffer;
            discardBuffer(GetFirstbuf());
            SetFirstbuf(buf);
        }
    }
}

void moveTab(TabBuffer *t, TabBuffer *t2, int right)
{
    if (t2 == NO_TABBUFFER)
        t2 = g_FirstTab;
    if (!t || !t2 || t == t2 || t == NO_TABBUFFER)
        return;
    if (t->prevTab)
    {
        if (t->nextTab)
            t->nextTab->prevTab = t->prevTab;
        else
            SetLastTab(t->prevTab);
        t->prevTab->nextTab = t->nextTab;
    }
    else
    {
        t->nextTab->prevTab = NULL;
        g_FirstTab = t->nextTab;
    }
    if (right)
    {
        t->nextTab = t2->nextTab;
        t->prevTab = t2;
        if (t2->nextTab)
            t2->nextTab->prevTab = t;
        else
            SetLastTab(t);
        t2->nextTab = t;
    }
    else
    {
        t->prevTab = t2->prevTab;
        t->nextTab = t2;
        if (t2->prevTab)
            t2->prevTab->nextTab = t;
        else
            g_FirstTab = t;
        t2->prevTab = t;
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}

void SelectRelativeTab(int prec)
{
    if (g_nTab <= 1)
    {
        return;
    }

    if (prec > 0)
    {
        for (int i = 0; i < prec; i++)
        {
            if (g_CurrentTab->nextTab)
                g_CurrentTab = g_CurrentTab->nextTab;
            else
                g_CurrentTab = g_FirstTab;
        }
    }
    else if (prec < 0)
    {
        for (int i = 0; i < PREC_NUM(); i++)
        {
            if (g_CurrentTab->prevTab)
                g_CurrentTab = g_CurrentTab->prevTab;
            else
                g_CurrentTab = GetLastTab();
        }
    }
}

void SelectTabByPosition(int x, int y)
{
    TabBuffer *tab = posTab(x, y);
    if (!tab || tab == NO_TABBUFFER)
        return;
    g_CurrentTab = tab;
}

void MoveTab(int x)
{
    TabBuffer *tab;
    int i;
    if (x > 0)
    {
        for (tab = g_CurrentTab, i = 0; tab && i < x;
             tab = tab->nextTab, i++)
            ;
        moveTab(g_CurrentTab, tab ? tab : GetLastTab(), TRUE);
    }
    else if (x < 0)
    {
        for (tab = g_CurrentTab, i = 0; tab && i < PREC_NUM();
             tab = tab->prevTab, i++)
            ;
        moveTab(g_CurrentTab, tab ? tab : g_FirstTab, FALSE);
    }
}

Buffer *GetCurrentbuf()
{
    return g_CurrentTab->currentBuffer;
}

void SetCurrentbuf(Buffer *buf)
{
    g_CurrentTab->currentBuffer = buf;
}

Buffer *GetFirstbuf()
{
    return g_CurrentTab->firstBuffer;
}

int HasFirstBuffer()
{
    return GetFirstbuf() && GetFirstbuf() != NO_BUFFER;
}

void SetFirstbuf(Buffer *buffer)
{
    g_CurrentTab->firstBuffer = buffer;
}
